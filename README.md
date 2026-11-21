# CacheDB

CacheDB is an in-memory key-value database written in C++20 for Linux. It includes a TCP server and an interactive CLI for storing, retrieving, and deleting values, with support for key expiration and disk snapshots.

## How it works

Clients send commands over TCP using length-prefixed MessagePack messages. The server executes them against an in-memory hash table and returns the result. Keys can have a time-to-live (TTL), after which they expire automatically.

`SET` and `DEL` operations are buffered to an append-only log (`database.aof`), which is replayed on startup. You can also create a snapshot with `SAVE` and restore it with `LOAD_SNAPSHOT`; both use `dump.db` by default. These files are stored relative to the server's working directory.

## Build and run

Requires Linux, a C++20 compiler, CMake 3.21+, Make, and Git. CMake downloads MessagePack and, if needed, GoogleTest during configuration.

From the project directory:

```sh
cmake --preset linux-debug
cmake --build --preset linux-debug
./build/linux-debug/cache_db
```

The server listens on port `5634` on all IPv4 interfaces. In another terminal, start the CLI, which connects to `127.0.0.1:5634` by default:

```sh
./build/linux-debug/cli
```

Enter one command per line:

```text
SET greeting "Hello, CacheDB!"
GET greeting
EXPIRE greeting 60
TTL greeting
DEL greeting
SAVE
quit
```

Use `LOAD_SNAPSHOT` to restore a saved snapshot. Quote strings containing spaces or numeric strings. The CLI also accepts `--host <IPv4>` and `--port <port>`. Stop the server with `Ctrl+C`.

See [Protocol.md](Protocol.md) for the command reference.

## Tests

```sh
ctest --preset linux-debug
```
