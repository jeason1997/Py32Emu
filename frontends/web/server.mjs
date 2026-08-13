import { createServer } from "node:http";
import { spawn } from "node:child_process";
import { promises as fs } from "node:fs";
import path from "node:path";
import process from "node:process";
import readline from "node:readline";

const root = path.resolve(process.cwd());
const publicRoot = path.join(root, "frontends", "web", "public");
const args = process.argv.slice(2);
const portIndex = args.indexOf("--port");
const port = Number(portIndex >= 0 ? args[portIndex + 1] : 4174);

function backendPath(file) {
  if (process.platform !== "win32") return file;
  const match = /^([A-Za-z]):[\\/](.*)$/.exec(file);
  if (!match) throw new Error("无法转换 WSL 路径");
  return `/mnt/${match[1].toLowerCase()}/${match[2].replaceAll("\\", "/")}`;
}

const child = process.platform === "win32"
  ? spawn("wsl.exe", ["--cd", root, "bash", "-lc", "./build/py32emu-web-core"],
      { stdio: ["pipe", "pipe", "inherit"] })
  : spawn(path.join(root, "build", "py32emu-web-core"), [],
      { stdio: ["pipe", "pipe", "inherit"] });
const pending = [];
readline.createInterface({ input: child.stdout }).on("line", line => {
  const request = pending.shift();
  if (!request) return;
  try { request.resolve(JSON.parse(line)); }
  catch { request.reject(new Error(`后端返回无效 JSON：${line}`)); }
});
child.on("exit", code => {
  while (pending.length) pending.shift().reject(new Error(`后端退出：${code}`));
});

function command(fields) {
  return new Promise((resolve, reject) => {
    pending.push({ resolve, reject });
    child.stdin.write(fields.join("\t") + "\n");
  });
}

function safePath(relative) {
  const file = path.resolve(root, String(relative || ""));
  if (file !== root && !file.startsWith(root + path.sep))
    throw new Error("固件路径超出项目目录");
  return file;
}

async function body(request) {
  const chunks = [];
  for await (const chunk of request) chunks.push(chunk);
  return chunks.length ? JSON.parse(Buffer.concat(chunks).toString("utf8")) : {};
}

function json(response, status, value) {
  response.writeHead(status, { "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store" });
  response.end(JSON.stringify(value));
}

const mime = new Map([[".html", "text/html; charset=utf-8"],
  [".js", "text/javascript; charset=utf-8"], [".css", "text/css; charset=utf-8"]]);

const server = createServer(async (request, response) => {
  try {
    const url = new URL(request.url, `http://${request.headers.host}`);
    if (request.method === "POST" && url.pathname === "/api/load") {
      const value = await body(request);
      const file = safePath(value.firmware);
      await fs.access(file);
      const result = await command(["load", backendPath(file),
        value.chip || "py32f002ax5"]);
      return json(response, result.ok ? 200 : 400, result);
    }
    if (request.method === "POST" && url.pathname === "/api/command") {
      const value = await body(request);
      let fields;
      if (value.command === "run") fields = ["run", value.steps || 10000];
      else if (value.command === "breakpoints")
        fields = ["breakpoints", (value.addresses || []).join(",")];
      else if (value.command === "memory")
        fields = ["memory", value.address || 0x20000000, value.count || 64];
      else if (value.command === "gpio")
        fields = ["gpio", value.port || 0, value.pin || 0,
          value.driven === false ? 0 : 1, value.high ? 1 : 0];
      else if (value.command === "usart_rx")
        fields = ["usart_rx", (value.bytes || []).join(",")];
      else if (value.command === "analog")
        fields = ["analog", value.port || 0, value.pin || 0,
          value.millivolts || 0];
      else if (["step", "reset", "state"].includes(value.command))
        fields = [value.command];
      else return json(response, 400, { ok: false, error: "未知命令" });
      const result = await command(fields);
      return json(response, result.ok ? 200 : 400, result);
    }
    if (url.pathname.startsWith("/api/"))
      return json(response, 404, { ok: false, error: "API 不存在" });
    const relative = url.pathname === "/" ? "index.html" : decodeURIComponent(url.pathname.slice(1));
    const file = path.resolve(publicRoot, relative);
    if (file !== publicRoot && !file.startsWith(publicRoot + path.sep)) {
      response.writeHead(403); return response.end("Forbidden");
    }
    const data = await fs.readFile(file);
    response.writeHead(200, { "Content-Type": mime.get(path.extname(file)) ||
      "application/octet-stream", "Cache-Control": "no-store" });
    response.end(data);
  } catch (error) { json(response, 500, { ok: false, error: error.message }); }
});

server.listen(port, "127.0.0.1", () =>
  console.log(`Py32Emu Web: http://127.0.0.1:${port}`));
function shutdown() { child.kill(); server.close(() => process.exit(0)); }
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
