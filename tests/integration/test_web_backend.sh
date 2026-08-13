#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
make -C "$root/examples/hello_world" >/dev/null
output=$(
    printf 'load\t%s\tpy32f002ax5\nstep\nbreakpoints\t0x080001ba\nrun\t2000\nmemory\t0x20000000\t16\nreset\ngpio\t1\t2\t1\t1\nusart_rx\t65,66\n' \
        "$root/examples/hello_world/build/hello_world.elf" |
        "$root/build/py32emu-web-core"
)
printf '%s\n' "$output" | python3 -c '
import json, sys
states = [json.loads(line) for line in sys.stdin if line.strip()]
assert len(states) == 8, states
assert states[0]["ok"] and states[0]["chip"] == "py32f002ax5"
assert len(states[0]["registers"]) == 16
assert len(states[0]["disassembly"]) == 12
assert any(row.get("symbol") for row in states[0]["disassembly"])
assert states[1]["cycles"] > states[0]["cycles"]
assert states[3]["stopped"] and states[3]["stopReason"] == "breakpoint"
assert len(states[4]["data"]) == 16
assert states[5]["cycles"] == 0 and not states[5]["stopped"]
assert states[6]["gpio"]["B"]["idr"] & 4
assert states[7]["ok"]
print("Py32Emu Web backend protocol test passed.")
'
