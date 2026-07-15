const chat = document.getElementById("chat");
const tools = document.getElementById("tools");
const input = document.getElementById("input");
const send = document.getElementById("send");
const statusText = document.getElementById("status-text");
const statusDot = document.getElementById("status-dot");
const eventCount = document.getElementById("event-count");
const toolCount = document.getElementById("tool-count");
const busyText = document.getElementById("busy");
const permission = document.getElementById("permission");
const permTitle = document.getElementById("perm-title");
const permPreview = document.getElementById("perm-preview");

let lastEventId = 0;
let eventsSeen = 0;
let toolsSeen = 0;
let currentAssistant = null;

function escapeText(value) {
  return String(value ?? "");
}

function scrollToBottom(el) {
  el.scrollTop = el.scrollHeight;
}

function message(role, text) {
  const div = document.createElement("div");
  div.className = `message ${role}`;
  const label = document.createElement("span");
  label.className = "role";
  label.textContent = role === "user" ? "you" : "assistant";
  const content = document.createElement("span");
  content.className = "content";
  content.textContent = escapeText(text);
  div.append(label, content);
  chat.appendChild(div);
  scrollToBottom(chat);
  return content;
}

function tool(type, title, detail) {
  toolsSeen += 1;
  toolCount.textContent = String(toolsSeen);
  const div = document.createElement("div");
  div.className = `tool-item ${type}`;
  const t = document.createElement("div");
  t.className = "tool-title";
  t.textContent = title || type;
  const d = document.createElement("div");
  d.className = "tool-detail";
  d.textContent = detail || "";
  div.append(t, d);
  tools.appendChild(div);
  scrollToBottom(tools);
}

function handleEvent(event) {
  lastEventId = Math.max(lastEventId, event.id);
  eventsSeen += 1;
  eventCount.textContent = String(eventsSeen);

  if (event.type === "user") {
    message("user", event.detail);
  } else if (event.type === "assistant_chunk") {
    if (!currentAssistant) currentAssistant = message("assistant", "");
    currentAssistant.textContent += event.detail || "";
    scrollToBottom(chat);
  } else if (event.type === "assistant_message") {
    currentAssistant = null;
    message("assistant", event.detail || "");
  } else if (event.type === "assistant_done") {
    currentAssistant = null;
  } else if (event.type === "tool_call") {
    tool("tool", `● ${event.title}`, event.detail);
  } else if (event.type === "tool_result") {
    tool("tool", `└ ${event.title}`, event.detail);
  } else if (event.type === "warning") {
    tool("warning", "warning", event.detail);
  } else if (event.type === "error") {
    tool("error", "error", event.detail);
  } else if (event.type === "permission") {
    const data = event.data || {};
    permTitle.textContent = `${data.tool || event.title} · ${data.risk || "unknown"}`;
    permPreview.textContent = data.preview || data.arguments || event.detail || "";
    permission.classList.add("visible");
    tool("permission", "permission required", permTitle.textContent);
  } else if (event.type === "permission_result") {
    tool("permission", "permission result", event.detail);
  }
}

async function poll() {
  try {
    const res = await fetch(`/api/events?after=${lastEventId}`);
    const data = await res.json();
    statusText.textContent = "connected";
    statusDot.style.background = "var(--green)";
    busyText.textContent = data.busy ? "busy" : "idle";
    send.disabled = Boolean(data.busy);
    for (const event of data.events || []) handleEvent(event);
  } catch {
    statusText.textContent = "offline";
    statusDot.style.background = "var(--rose)";
  } finally {
    setTimeout(poll, 550);
  }
}

async function sendMessage() {
  const content = input.value.trim();
  if (!content) return;
  input.value = "";
  send.disabled = true;
  await fetch("/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ content }),
  });
}

async function approve(approved) {
  permission.classList.remove("visible");
  await fetch("/api/permission", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ approved }),
  });
}

send.addEventListener("click", sendMessage);
input.addEventListener("keydown", (event) => {
  if (event.key === "Enter" && !event.shiftKey) {
    event.preventDefault();
    sendMessage();
  }
});
document.getElementById("approve").addEventListener("click", () => approve(true));
document.getElementById("deny").addEventListener("click", () => approve(false));
document.getElementById("clear-btn").addEventListener("click", () => {
  chat.innerHTML = "";
  tools.innerHTML = "";
});

message("assistant", "系统就绪。");
poll();
