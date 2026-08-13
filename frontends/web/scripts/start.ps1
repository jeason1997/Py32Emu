param([int]$Port = 4174)
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
wsl.exe --cd $root bash -lc "make web-core"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
node (Join-Path $root "frontends\web\server.mjs") --port $Port
