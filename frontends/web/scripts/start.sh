#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
cd "$root"
make web-core
if command -v node >/dev/null 2>&1; then exec node frontends/web/server.mjs "$@"
elif command -v node.exe >/dev/null 2>&1; then
    exec node.exe "$(wslpath -w "$root/frontends/web/server.mjs")" "$@"
else echo '需要安装 Node.js' >&2; exit 1
fi
