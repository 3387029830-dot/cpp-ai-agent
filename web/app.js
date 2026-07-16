const chat = document.getElementById("chat");
const tools = document.getElementById("tools");
const input = document.getElementById("input");
const send = document.getElementById("send");
const statusText = document.getElementById("status-text");
const statusDot = document.getElementById("status-dot");
const eventCount = document.getElementById("event-count");
const toolCount = document.getElementById("tool-count");
const skillCount = document.getElementById("skill-count");
const uploadCount = document.getElementById("upload-count");
const busyText = document.getElementById("busy");
const permission = document.getElementById("permission");
const permTitle = document.getElementById("perm-title");
const permPreview = document.getElementById("perm-preview");
const sendHint = document.getElementById("send-hint");
const dropZone = document.getElementById("drop-zone");
const fileInput = document.getElementById("file-input");
const viewTitle = document.getElementById("view-title");
const viewEyebrow = document.getElementById("view-eyebrow");
const toolCards = document.getElementById("tool-cards");
const skillCards = document.getElementById("skill-cards");
const runCards = document.getElementById("run-cards");

let lastEventId = 0;
let eventsSeen = 0;
let uploadsSeen = 0;
let currentAssistant = null;
const seenEventIds = new Set();
let currentView = "chat";
let statusCache = { tools: [], skills: [], commands: [] };

const viewCopy = {
  chat: ["local agent workspace", "Control Deck"],
  tools: ["tool registry", "Tools"],
  skills: ["skill catalog", "Skills"],
  runs: ["demo command center", "Runs"],
};

function scrollToBottom(el) {
  el.scrollTop = el.scrollHeight;
}

function message(role, text) {
  const row = document.createElement("article");
  row.className = `message ${role}`;
  const meta = document.createElement("div");
  meta.className = "message-meta";
  meta.textContent = role === "user" ? "you" : "agent";
  const body = document.createElement("div");
  body.className = "message-body";
  body.textContent = text || "";
  row.append(meta, body);
  chat.appendChild(row);
  scrollToBottom(chat);
  return body;
}

function activity(type, title, detail) {
  const empty = tools.querySelector("[data-empty='activity']");
  if (empty) empty.remove();
  const row = document.createElement("div");
  row.className = `timeline-item ${type}`;
  const head = document.createElement("div");
  head.className = "timeline-head";
  head.textContent = title || type;
  const body = document.createElement("pre");
  body.textContent = detail || "";
  row.append(head, body);
  tools.prepend(row);
  while (tools.querySelectorAll(".timeline-item").length > 30) {
    tools.querySelector(".timeline-item:last-of-type")?.remove();
  }
}

function setPermissionMode(mode) {
  document.getElementById("permission-mode-label").textContent = mode.replace("_", " ");
  document.querySelectorAll("#permission-modes button").forEach((button) => {
    button.classList.toggle("active", button.dataset.mode === mode);
  });
}

function hydrateStatus(status) {
  statusCache = {
    tools: status.tools || [],
    skills: status.skills || [],
    commands: status.commands || [],
  };
  document.getElementById("model").textContent = status.model || "model";
  document.getElementById("workspace").textContent = status.workspace || ".";
  document.getElementById("history").textContent = status.history || "logs";
  setPermissionMode(status.permission_mode || "ask_each_time");

  const toolList = document.getElementById("tool-list");
  const skillList = document.getElementById("skill-list");
  const commandList = document.getElementById("command-list");
  toolList.innerHTML = "";
  skillList.innerHTML = "";
  commandList.innerHTML = "";

  for (const tool of statusCache.tools) {
    const item = document.createElement("span");
    item.className = `pill ${tool.risk || "safe"}`;
    item.textContent = tool.name;
    item.title = `${tool.risk || "safe"} · ${tool.description || ""}`;
    toolList.appendChild(item);
  }

  for (const skill of statusCache.skills) {
    const item = document.createElement("span");
    item.className = "pill skill";
    item.textContent = skill.name;
    item.title = skill.description || "";
    skillList.appendChild(item);
  }

  for (const command of statusCache.commands) {
    const item = document.createElement("code");
    item.textContent = command;
    commandList.appendChild(item);
  }

  skillCount.textContent = String(statusCache.skills.length);
  toolCount.textContent = String(statusCache.tools.length);
  renderMainViews();
}

function handleEvent(event) {
  if (seenEventIds.has(event.id)) return;
  seenEventIds.add(event.id);
  lastEventId = Math.max(lastEventId, event.id);
  eventsSeen += 1;
  eventCount.textContent = `${eventsSeen} events`;

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
    const risk = event.data?.risk ? ` · ${event.data.risk}` : "";
    activity("tool", `${event.title}${risk}`, event.detail);
  } else if (event.type === "tool_result") {
    activity("tool", `${event.title} result`, event.detail);
  } else if (event.type === "warning") {
    activity("warning", "warning", event.detail);
  } else if (event.type === "error") {
    activity("error", "error", event.detail);
  } else if (event.type === "permission") {
    const data = event.data || {};
    permTitle.textContent = `${data.tool || event.title} · ${data.risk || "unknown"}`;
    permPreview.textContent = data.preview || data.arguments || event.detail || "";
    permission.classList.add("visible");
    activity("permission", "permission requested", permTitle.textContent);
  } else if (event.type === "permission_result") {
    activity("permission", "permission result", event.detail);
  } else if (event.type === "upload") {
    uploadsSeen += 1;
    uploadCount.textContent = String(uploadsSeen);
    activity("upload", `uploaded ${event.title}`, event.detail);
  } else if (event.type === "settings") {
    activity("settings", "settings", event.detail);
  }
}

function card(title, subtitle, detail, tone, action) {
  const node = document.createElement("button");
  node.className = `capability-card ${tone || ""}`;
  node.type = "button";
  node.innerHTML = `
    <span>${subtitle || ""}</span>
    <b></b>
    <p></p>
  `;
  node.querySelector("b").textContent = title;
  node.querySelector("p").textContent = detail || "";
  node.addEventListener("click", action);
  return node;
}

function riskLabel(risk) {
  if (risk === "dangerous") return "Dangerous command";
  if (risk === "write") return "Write access";
  return "Safe read";
}

function renderMainViews() {
  toolCards.innerHTML = "";
  skillCards.innerHTML = "";
  runCards.innerHTML = "";

  for (const tool of statusCache.tools) {
    toolCards.appendChild(card(
      tool.name,
      riskLabel(tool.risk),
      tool.description,
      tool.risk,
      () => {
        input.value = `请演示 ${tool.name} 工具，并说明它的输入、风险等级和适用场景。`;
        input.focus();
      },
    ));
  }

  for (const skill of statusCache.skills) {
    const allowed = (skill.tools || []).join(", ");
    skillCards.appendChild(card(
      skill.name,
      "Skill preset",
      `${skill.description || ""}${allowed ? ` Allowed tools: ${allowed}` : ""}`,
      "skill",
      () => {
        input.value = `/use-skill ${skill.name}`;
        input.focus();
      },
    ));
  }

  for (const command of statusCache.commands) {
    runCards.appendChild(card(
      command,
      "Command",
      "Click to copy this command into the input box.",
      "command",
      () => {
        input.value = command.replace(" <query>", " MCP 是什么").replace(" <log.jsonl>", "");
        input.focus();
      },
    ));
  }
}

function switchView(view, updateHash = true) {
  if (!viewCopy[view]) view = "chat";
  currentView = view;
  document.querySelectorAll(".nav-item").forEach((item) => {
    item.classList.toggle("active", item.dataset.view === view);
  });
  document.querySelectorAll(".view-page").forEach((page) => {
    page.classList.toggle("active", page.id === `${view}-view`);
  });
  const copy = viewCopy[view] || viewCopy.chat;
  viewEyebrow.textContent = copy[0];
  viewTitle.textContent = copy[1];
  if (updateHash && window.location.hash !== `#${view}`) {
    history.replaceState(null, "", `#${view}`);
  }
}

async function refreshStatus() {
  const res = await fetch("/api/status");
  hydrateStatus(await res.json());
}

async function poll() {
  try {
    const res = await fetch(`/api/events?after=${lastEventId}`);
    const data = await res.json();
    statusText.textContent = "connected";
    statusDot.style.background = "var(--ok)";
    busyText.textContent = data.busy ? "busy" : "idle";
    send.disabled = Boolean(data.busy);
    for (const event of data.events || []) handleEvent(event);
  } catch {
    statusText.textContent = "offline";
    statusDot.style.background = "var(--danger)";
  } finally {
    setTimeout(poll, 420);
  }
}

async function sendMessage() {
  const content = input.value.trim();
  if (!content) return;
  input.value = "";
  send.disabled = true;
  sendHint.textContent = "running";
  const res = await fetch("/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ content }),
  });
  sendHint.textContent = res.ok ? "accepted" : "rejected";
}

async function approve(approved) {
  permission.classList.remove("visible");
  await fetch("/api/permission", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ approved }),
  });
}

async function updatePermissionMode(mode) {
  setPermissionMode(mode);
  const res = await fetch("/api/settings", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ permission_mode: mode }),
  });
  if (res.ok) {
    const data = await res.json();
    hydrateStatus(data.status || {});
  }
}

async function uploadFiles(files) {
  for (const file of files) {
    const content = await file.text();
    const res = await fetch("/api/upload", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ filename: file.name, content }),
    });
    if (!res.ok) {
      activity("error", `upload failed ${file.name}`, await res.text());
    }
  }
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
document.getElementById("upload-btn").addEventListener("click", () => fileInput.click());
fileInput.addEventListener("change", () => uploadFiles(fileInput.files));

dropZone.addEventListener("dragover", (event) => {
  event.preventDefault();
  dropZone.classList.add("dragging");
});
dropZone.addEventListener("dragleave", () => dropZone.classList.remove("dragging"));
dropZone.addEventListener("drop", (event) => {
  event.preventDefault();
  dropZone.classList.remove("dragging");
  uploadFiles(event.dataTransfer.files);
});

document.querySelectorAll("#permission-modes button").forEach((button) => {
  button.addEventListener("click", () => updatePermissionMode(button.dataset.mode));
});

document.querySelectorAll(".tab").forEach((tab) => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach((item) => item.classList.remove("active"));
    document.querySelectorAll(".tab-page").forEach((item) => item.classList.remove("active"));
    tab.classList.add("active");
    document.getElementById(tab.dataset.tab).classList.add("active");
  });
});

document.querySelectorAll(".prompt-chip").forEach((chip) => {
  chip.addEventListener("click", () => {
    input.value = chip.textContent;
    input.focus();
  });
});

document.querySelectorAll(".nav-item").forEach((item) => {
  item.addEventListener("click", () => switchView(item.dataset.view));
});

window.addEventListener("hashchange", () => switchView(window.location.hash.slice(1) || "chat", false));

message("assistant", "Control Deck online.");
switchView(window.location.hash.slice(1) || "chat", false);
refreshStatus().catch(() => {});
poll();
