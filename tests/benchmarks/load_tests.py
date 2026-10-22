"""Benchmark regression checks, using only Python's standard library.

Usage: python3 tests/benchmarks/load_tests.py build/linux-prod
The TCP tests use an isolated fake server on an ephemeral port.
"""
import contextlib
import pathlib
import socketserver
import struct
import subprocess
import sys
import threading
import time
import unittest

BUILD = pathlib.Path(sys.argv.pop(1) if len(sys.argv) > 1 else "build/linux-prod").resolve()


def unpack(data, offset=0):
    """Decode the array/string/positive-fixint subset used by requests."""
    tag = data[offset]
    offset += 1
    if tag < 0x80:
        return tag, offset
    if 0x90 <= tag <= 0x9F:
        items = []
        for _ in range(tag & 15):
            item, offset = unpack(data, offset)
            items.append(item)
        return items, offset
    if 0xA0 <= tag <= 0xBF:
        size = tag & 31
    else:
        width = {0xD9: 1, 0xDA: 2, 0xDB: 4}[tag]
        size = int.from_bytes(data[offset:offset + width], "big")
        offset += width
    return data[offset:offset + size], offset + size


def pack_string(value):
    if len(value) < 32:
        return bytes([0xA0 + len(value)]) + value
    return b"\xdb" + struct.pack("!I", len(value)) + value


@contextlib.contextmanager
def server(behavior="normal"):
    class Handler(socketserver.BaseRequestHandler):
        def exact(self, size):
            result = b""
            while len(result) < size:
                part = self.request.recv(size - len(result))
                if not part:
                    raise EOFError
                result += part
            return result

        def handle(self):
            self.request.settimeout(2)
            try:
                while True:
                    size = struct.unpack("!I", self.exact(4))[0]
                    raw = self.exact(size)
                    (command, args), consumed = unpack(raw)
                    assert consumed == len(raw)
                    key = args[0][1]
                    with self.server.lock:
                        self.server.commands.append((command, key))
                        if command == b"SET":
                            self.server.values[key] = args[1][1]
                            reply = b"\x92\x06\xa2OK"
                        elif command == b"DEL":
                            existed = key in self.server.values
                            self.server.values.pop(key, None)
                            reply = b"\x92\x03" + bytes([int(existed)])
                        else:
                            value = self.server.values.get(key)
                            reply = b"\x92\x01\xc0" if value is None else b"\x92\x06" + pack_string(value)
                    if behavior == "close":
                        return
                    if behavior == "timeout":
                        time.sleep(.2)
                        return
                    if behavior == "error":
                        reply = b"\x92\x06" + pack_string(b"ERR deliberate failure")
                    if behavior == "wrong_value" and command == b"GET":
                        reply = b"\x92\x06" + pack_string(b"wrong")
                    if behavior == "wrong_type":
                        reply = b"\x92\x06\xc0"
                    if behavior == "trailing":
                        reply += b"\x00"
                    if behavior == "oversize":
                        self.request.sendall(struct.pack("!I", 16 * 1024 * 1024 + 1))
                        return
                    if behavior == "zero":
                        self.request.sendall(b"\x00\x00\x00\x00")
                        return
                    frame = struct.pack("!I", len(reply)) + reply
                    if behavior == "fragmented":
                        for byte in frame:
                            self.request.sendall(bytes([byte]))
                    else:
                        self.request.sendall(frame)
            except (EOFError, OSError):
                pass

    class Server(socketserver.ThreadingTCPServer):
        daemon_threads = True

    with Server(("127.0.0.1", 0), Handler) as instance:
        instance.values = {}
        instance.commands = []
        instance.lock = threading.Lock()
        thread = threading.Thread(target=instance.serve_forever, kwargs={"poll_interval": .01})
        thread.start()
        try:
            yield instance
        finally:
            instance.shutdown()
            thread.join()


def run(binary, *args, ok=True):
    result = subprocess.run([str(BUILD / binary), *map(str, args)], capture_output=True, text=True, timeout=20)
    if ok and result.returncode != 0:
        raise AssertionError(result.stdout + result.stderr)
    if not ok and result.returncode == 0:
        raise AssertionError("Unexpected success: " + result.stdout)
    return result.stdout + result.stderr


class LoadTests(unittest.TestCase):
    def test_direct_workloads(self):
        for mode in ("GET", "GET_MISS", "SET", "OVERWRITE", "MIX"):
            with self.subTest(mode=mode):
                out = run("database_load", "--command", mode, "--clients", 4,
                          "--requests", 103, "--keys", 7, "--warmup", 9, "--value-size", 4096)
                self.assertIn("Successful: 103 Failed/uncompleted: 0", out)

    def test_direct_resize(self):
        out = run("database_load", "--command", "SET", "--clients", 1,
                  "--requests", 33000, "--warmup", 0, "--value-size", 0)
        self.assertIn("Successful: 33000", out)

    def test_tcp_workloads_and_shared_keys(self):
        for mode in ("GET", "GET_MISS", "SET", "OVERWRITE", "MIX"):
            with self.subTest(mode=mode), server() as fixture:
                out = run("memory_load", "--port", fixture.server_address[1], "--command", mode,
                          "--clients", 3, "--requests", 103, "--keys", 7, "--pipeline", 8,
                          "--warmup", 9, "--value-size", 0)
                self.assertIn("Successful: 103 Failed/uncompleted: 0", out)
                reads = {key for command, key in fixture.commands if command == b"GET"}
                writes = {key for command, key in fixture.commands if command == b"SET"}
                if mode in ("GET", "MIX"):
                    self.assertTrue(reads)
                    self.assertTrue(reads <= writes)
                if mode == "GET_MISS":
                    self.assertFalse(writes)
                if mode == "SET":
                    # Warmup keys are removed; measured inserts are unique.
                    self.assertEqual(len(fixture.values), 103)
                if mode == "OVERWRITE":
                    self.assertEqual(len(fixture.values), 7)

    def test_mix_extremes(self):
        for percent, expected in ((0, "Reads: 0 Writes: 100"), (100, "Reads: 100 Writes: 0")):
            for binary in ("database_load", "memory_load"):
                with self.subTest(percent=percent, binary=binary), server() as fixture:
                    args = ["--port", fixture.server_address[1]] if binary == "memory_load" else []
                    out = run(binary, *args, "--command", "MIX", "--read-percent", percent,
                              "--clients", 2, "--requests", 100, "--keys", 1, "--warmup", 0)
                    self.assertIn(expected, out)

    def test_fragmented_replies(self):
        with server("fragmented") as fixture:
            out = run("memory_load", "--port", fixture.server_address[1], "--command", "GET_MISS",
                      "--clients", 1, "--requests", 17, "--pipeline", 8, "--warmup", 0)
            self.assertIn("Successful: 17", out)

    def test_tcp_failures_are_not_successes(self):
        for behavior in ("error", "wrong_type", "trailing", "oversize", "zero", "close", "timeout"):
            with self.subTest(behavior=behavior), server(behavior) as fixture:
                out = run("memory_load", "--port", fixture.server_address[1], "--command", "GET_MISS",
                          "--clients", 1, "--requests", 3, "--pipeline", 2, "--warmup", 0,
                          "--timeout-ms", 100, ok=False)
                self.assertIn("Successful: 0 Failed/uncompleted: 3", out)

    def test_wrong_get_value(self):
        with server("wrong_value") as fixture:
            out = run("memory_load", "--port", fixture.server_address[1], "--command", "GET",
                      "--clients", 1, "--requests", 3, "--keys", 1, "--warmup", 0, ok=False)
            self.assertIn("Successful: 0 Failed/uncompleted: 3", out)

    def test_warmup_failure_releases_all_workers(self):
        with server("error") as fixture:
            out = run("memory_load", "--port", fixture.server_address[1], "--command", "GET_MISS",
                      "--clients", 4, "--requests", 8, "--warmup", 1, ok=False)
            self.assertIn("Successful: 0 Failed/uncompleted: 8", out)

    def test_invalid_arguments(self):
        for binary in ("database_load", "memory_load"):
            for args in (("--clients", "-1"), ("--requests", "0"), ("--keys", "0"),
                         ("--clients", "4", "--requests", "3"), ("--read-percent", "101"),
                         ("--requests", "18446744073709551616"), ("--value-size",), ("--command", "bogus")):
                with self.subTest(binary=binary, args=args):
                    run(binary, *args, ok=False)


if __name__ == "__main__":
    unittest.main()
