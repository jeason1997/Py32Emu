#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
port=4184
log=$root/build/web-server-test.log
server_script=$root/frontends/web/server.mjs
curl_runtime=curl
if command -v node >/dev/null 2>&1; then node_runtime=node
elif command -v node.exe >/dev/null 2>&1; then
    node_runtime=node.exe
    server_script=$(wslpath -w "$server_script")
    curl_runtime=curl.exe
else echo 'Node.js runtime not found' >&2; exit 1
fi
"$node_runtime" "$server_script" --port "$port" >"$log" 2>&1 &
server_pid=$!
trap 'kill "$server_pid" 2>/dev/null || true' EXIT INT TERM

ready=false
for delay in 1 1 1 1 1; do
    if "$curl_runtime" -fsS "http://127.0.0.1:$port/" >/dev/null; then ready=true; break; fi
    sleep "$delay"
done
test "$ready" = true

"$curl_runtime" -fsS -X POST -H 'Content-Type: application/json' \
    -d '{"firmware":"examples/hello_world/build/hello_world.elf","chip":"py32f002ax5"}' \
    "http://127.0.0.1:$port/api/load" |
    python3 -c 'import json,sys; s=json.load(sys.stdin); assert s["ok"] and len(s["registers"]) == 16 and len(s["disassembly"]) == 12'
"$curl_runtime" -fsS -X POST -H 'Content-Type: application/json' \
    -d '{"command":"step"}' "http://127.0.0.1:$port/api/command" |
    python3 -c 'import json,sys; assert json.load(sys.stdin)["cycles"] > 0'
"$curl_runtime" -fsS -X POST -H 'Content-Type: application/json' \
    -d '{"command":"memory","address":"0x20000000","count":16}' \
    "http://127.0.0.1:$port/api/command" |
    python3 -c 'import json,sys; assert len(json.load(sys.stdin)["data"]) == 16'

echo 'Py32Emu Web server API test passed.'
