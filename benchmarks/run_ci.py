"""Run the Release benchmarks and write a GitHub-friendly report.

Usage: python3 benchmarks/run_ci.py --build-dir build/linux-prod \
           --output-dir benchmark-results
"""

import argparse
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import sys
import tempfile
import time


MODES = ("GET", "SET", "MIX")
SERVER_PORT = 5634  # TCPServer's current listening port.
METRICS = re.compile(
    r"Status: OK\b.*?Throughput: ([\d.]+) req/s\n"
    r"Latency \(successful requests\): p50: ([\d.]+) us "
    r"p95: ([\d.]+) us p99: ([\d.]+) us p999: ([\d.]+) us"
)


def benchmark(binary, mode, args, output_dir, cwd):
    command = [str(binary), "--command", mode, *args]
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=180)
    (output_dir / f"{binary.name}_{mode}.log").write_text(
        result.stdout + result.stderr, encoding="utf-8"
    )
    if result.returncode:
        raise RuntimeError(f"{binary.name} {mode} exited with code {result.returncode}")
    match = METRICS.search(result.stdout)
    if not match:
        raise RuntimeError(f"{binary.name} {mode} produced no valid metrics")
    throughput, p50, p95, p99, p999 = map(float, match.groups())
    return {
        "benchmark": binary.name,
        "mode": mode,
        "throughput_req_s": throughput,
        "latency_us": {"p50": p50, "p95": p95, "p99": p99, "p999": p999},
        "status": "OK",
    }


def wait_for_server(process):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"cache_db exited during startup (code {process.returncode})")
        try:
            with socket.create_connection(("127.0.0.1", SERVER_PORT), timeout=0.2):
                return
        except OSError:
            time.sleep(0.1)
    raise RuntimeError("cache_db did not open port 5634 within 15 seconds")


def server_benchmark(binary, mode, args, output_dir, server_binary):
    with tempfile.TemporaryDirectory(prefix="cachedb-benchmark-") as workdir:
        with (output_dir / f"cache_db_{mode}.log").open("w", encoding="utf-8") as log:
            process = subprocess.Popen(
                [str(server_binary)], cwd=workdir, stdout=log, stderr=subprocess.STDOUT
            )
            try:
                wait_for_server(process)
                return benchmark(binary, mode, [*args, "--port", str(SERVER_PORT)], output_dir, workdir)
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


def report(results, requests, clients, keys, value_size, warmup):
    sha = os.environ.get("GITHUB_SHA", "local")
    lines = [
        "# CacheDB benchmark results",
        "",
        f"Commit: `{sha}`  ",
        f"Workload: {requests:,} requests, {clients} clients, {keys:,} keys, "
        f"{value_size}-byte values, {warmup} warmup operations per client; "
        "TCP pipeline depth 1.  ",
        "Build: `linux-prod` (Release). Each TCP workload uses a fresh server and data directory.",
        "",
        "| Benchmark | Command | Throughput (req/s) | p50 (µs) | p95 (µs) | p99 (µs) | Status |",
        "| --- | --- | ---: | ---: | ---: | ---: | --- |",
    ]
    for item in results:
        if item["status"] == "OK":
            latency = item["latency_us"]
            lines.append(
                f"| `{item['benchmark']}` | {item['mode']} | {item['throughput_req_s']:,.0f} "
                f"| {latency['p50']:.1f} | {latency['p95']:.1f} | {latency['p99']:.1f} | OK |"
            )
        else:
            error = item["error"].replace("|", "\\|").replace("\n", " ")
            lines.append(f"| `{item['benchmark']}` | {item['mode']} | — | — | — | — | {error} |")
    lines.extend([
        "",
        "`database_load` measures direct database calls; `server_load` includes the TCP server "
        "and protocol. These are single-run measurements on the current runner.",
        "",
    ])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--requests", type=int, default=100_000)
    parser.add_argument("--clients", type=int, default=4)
    parser.add_argument("--keys", type=int, default=10_000)
    parser.add_argument("--value-size", type=int, default=128)
    parser.add_argument("--warmup", type=int, default=1_000)
    parser.add_argument("--modes", nargs="+", choices=MODES, default=MODES)
    options = parser.parse_args()
    build_dir = options.build_dir.resolve()
    output_dir = options.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    args = [
        "--requests", str(options.requests), "--clients", str(options.clients),
        "--keys", str(options.keys), "--value-size", str(options.value_size),
        "--warmup", str(options.warmup),
    ]
    results = []
    for mode in options.modes:
        for name in ("database_load", "server_load"):
            try:
                if name == "server_load":
                    item = server_benchmark(build_dir / name, mode, args, output_dir, build_dir / "cache_db")
                else:
                    with tempfile.TemporaryDirectory(prefix="cachedb-benchmark-") as workdir:
                        item = benchmark(build_dir / name, mode, args, output_dir, workdir)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                item = {"benchmark": name, "mode": mode, "status": "FAILED", "error": str(error)}
            results.append(item)
    data = {
        "commit": os.environ.get("GITHUB_SHA", "local"),
        "configuration": {
            "requests": options.requests, "clients": options.clients, "keys": options.keys,
            "value_size": options.value_size, "warmup": options.warmup,
            "pipeline": 1, "build": "linux-prod",
        },
        "results": results,
    }
    (output_dir / "results.json").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    markdown = report(results, options.requests, options.clients, options.keys,
                      options.value_size, options.warmup)
    (output_dir / "report.md").write_text(markdown, encoding="utf-8")
    if summary := os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(summary, "a", encoding="utf-8") as stream:
            stream.write(markdown)
    print(markdown)
    return 1 if any(item["status"] != "OK" for item in results) else 0


if __name__ == "__main__":
    sys.exit(main())
