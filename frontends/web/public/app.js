const $ = id => document.getElementById(id);
const hex = (value, width = 8) => `0x${Number(value >>> 0).toString(16).toUpperCase().padStart(width, "0")}`;
async function post(path, value) {
  const response = await fetch(path, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(value) });
  const result = await response.json();
  if (!result.ok) throw new Error(result.error || "操作失败");
  return result;
}
function render(state) {
  $("connection").textContent = state.chip || "已连接";
  $("pc").textContent = hex(state.pc); $("cycles").textContent = state.cycles.toLocaleString();
  $("status").textContent = state.breakpointHit ? "命中断点" : state.stopped ? state.stopReason : "运行中";
  $("exception").textContent = state.exception;
  $("registers").innerHTML = state.registers.map((v, i) => `<div><small>R${i}</small><b>${hex(v)}</b></div>`).join("") +
    `<div><small>xPSR</small><b>${hex(state.xpsr)}</b></div><div><small>CONTROL</small><b>${hex(state.control)}</b></div>`;
  $("gpio").innerHTML = Object.entries(state.gpio).map(([port, v]) =>
    `<article><b>GPIO${port}</b><span>ODR ${hex(v.odr,4)}</span><span>IDR ${hex(v.idr,4)}</span></article>`).join("");
  $("error").textContent = "";
}
async function action(fn) { try { render(await fn()); } catch (error) { $("error").textContent = error.message; } }
$("load").onclick = () => action(() => post("/api/load", { firmware: $("firmware").value, chip: $("chip").value }));
document.querySelectorAll("[data-command]").forEach(button => button.onclick = () => action(() => post("/api/command", { command: button.dataset.command })));
$("run").onclick = () => action(() => post("/api/command", { command: "run", steps: 10000 }));
$("setBreakpoint").onclick = () => action(() => post("/api/command", { command: "breakpoints", addresses: [$("breakpoint").value] }));
$("readMemory").onclick = async () => { try { const r = await post("/api/command", { command: "memory", address: $("memoryAddress").value, count: 64 });
  $("memory").textContent = r.data.map((v,i) => `${i%16===0 ? `\n${hex(r.address+i)}  ` : ""}${v.toString(16).padStart(2,"0")} `).join("").trim();
} catch (error) { $("error").textContent = error.message; } };
