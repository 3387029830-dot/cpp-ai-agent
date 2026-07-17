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
let thinkingIndicator = null;
const seenEventIds = new Set();
let currentView = "chat";
let statusCache = { tools: [], skills: [], commands: [] };

/* ── typewriter buffer for smooth streaming ── */
let typeBuffer = "";
let typeTimer = null;
let streamText = "";
const TYPE_SPEED = 18; // chars per frame (~60fps)

/* ── markdown renderer ── */
function escHtml(text) {
  return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function inlineMd(text) {
  text = escHtml(text);
  text = text.replace(/\*\*(.+?)\*\*/g, '<strong>$1</strong>');
  text = text.replace(/`([^`]+)`/g, '<code>$1</code>');
  return text;
}

function codeBlockHtml(lang, code) {
  const langTag = lang ? `<span class="code-lang">${escHtml(lang)}</span>` : '';
  const btn = '<button class="copy-btn" onclick="copyCode(this)">Copy</button>';
  return `<div class="code-block">${langTag}${btn}<pre><code>${escHtml(code)}</code></pre></div>`;
}

function renderMarkdown(text) {
  const lines = text.split('\n');
  let html = '';
  let inFence = false;
  let fenceLang = '';
  let fenceBuf = '';
  let inList = 0; // 0=none, 1=ul, 2=ol

  function closeList() {
    if (inList === 1) { html += '</ul>'; }
    if (inList === 2) { html += '</ol>'; }
    inList = 0;
  }

  for (let i = 0; i < lines.length; i++) {
    const raw = lines[i];
    const trimmed = raw.trimStart();

    // fenced code block
    if (trimmed.startsWith('```')) {
      if (inFence) {
        html += codeBlockHtml(fenceLang, fenceBuf);
        fenceBuf = '';
        inFence = false;
      } else {
        closeList();
        fenceLang = trimmed.slice(3).trim();
        inFence = true;
      }
      continue;
    }

    if (inFence) {
      fenceBuf += (fenceBuf ? '\n' : '') + raw;
      continue;
    }

    // blank line
    if (!trimmed) {
      closeList();
      continue;
    }

    // heading
    if (trimmed.match(/^### /)) { closeList(); html += `<h3>${inlineMd(trimmed.slice(4))}</h3>`; continue; }
    if (trimmed.match(/^## /))  { closeList(); html += `<h2>${inlineMd(trimmed.slice(3))}</h2>`; continue; }
    if (trimmed.match(/^# /))   { closeList(); html += `<h1>${inlineMd(trimmed.slice(2))}</h1>`; continue; }

    // blockquote
    if (trimmed.startsWith('> ')) { closeList(); html += `<blockquote>${inlineMd(trimmed.slice(2))}</blockquote>`; continue; }

    // unordered list
    if (trimmed.match(/^[-*] /)) {
      if (inList !== 1) { closeList(); html += '<ul>'; inList = 1; }
      html += `<li>${inlineMd(trimmed.slice(2))}</li>`;
      continue;
    }

    // ordered list
    if (trimmed.match(/^\d+\. /)) {
      if (inList !== 2) { closeList(); html += '<ol>'; inList = 2; }
      html += `<li>${inlineMd(trimmed.replace(/^\d+\. /, ''))}</li>`;
      continue;
    }

    closeList();
    html += `<p>${inlineMd(trimmed)}</p>`;
  }

  if (inFence) html += codeBlockHtml(fenceLang, fenceBuf);
  closeList();
  return html;
}

/* ── code copy ── */
window.copyCode = function(btn) {
  const block = btn.closest('.code-block');
  const code = block ? block.querySelector('code').textContent : '';
  navigator.clipboard.writeText(code).then(() => {
    btn.textContent = 'Copied!';
    setTimeout(() => { btn.textContent = 'Copy'; }, 1800);
  }).catch(() => {});
};

function flushTypeBuffer() {
  if (typeTimer) {
    clearInterval(typeTimer);
    typeTimer = null;
  }
  if (typeBuffer && currentAssistant) {
    streamText += typeBuffer;
    typeBuffer = "";
    currentAssistant.innerHTML = renderMarkdown(streamText) + '<span class="cursor-blink"></span>';
    scrollToBottom(chat);
  }
}

function feedTypeBuffer(chunk) {
  typeBuffer += chunk;
  if (!typeTimer) {
    typeTimer = setInterval(() => {
      if (!typeBuffer) {
        clearInterval(typeTimer);
        typeTimer = null;
        return;
      }
      const take = Math.min(typeBuffer.length, TYPE_SPEED);
      const part = typeBuffer.slice(0, take);
      typeBuffer = typeBuffer.slice(take);
      streamText += part;
      if (currentAssistant) {
        currentAssistant.innerHTML = renderMarkdown(streamText) + '<span class="cursor-blink"></span>';
        scrollToBottom(chat);
      }
    }, 16); // ~60fps
  }
}

function resetTypeBuffer() {
  flushTypeBuffer();
  typeBuffer = "";
  streamText = "";
  if (typeTimer) {
    clearInterval(typeTimer);
    typeTimer = null;
  }
}

const viewCopy = {
  chat: ["local agent workspace", "Control Deck"],
  tools: ["tool registry", "Tools"],
  skills: ["skill catalog", "Skills"],
  runs: ["demo command center", "Runs"],
};

const banner = document.getElementById("reconnect-banner");
const sessionList = document.getElementById("session-list");
let connectionLost = false;

function scrollToBottom(el) {
  const last = el.lastElementChild;
  if (last) last.scrollIntoView({ behavior: "smooth", block: "end" });
}

function formatTime() {
  return new Date().toLocaleTimeString('zh-CN', { hour: '2-digit', minute: '2-digit' });
}

function updateConnection(ok) {
  if (ok && connectionLost) {
    banner.classList.add("hidden");
    connectionLost = false;
  } else if (!ok && !connectionLost) {
    banner.classList.remove("hidden");
    connectionLost = true;
  }
}

/* ── welcome screen ── */
function showWelcome() {
  const card = document.createElement("div");
  card.className = "welcome-card";
  card.id = "welcome";
  card.innerHTML = `
    <div class="welcome-mark">C</div>
    <h2>cpp-ai-agent</h2>
    <p>C++17 terminal AI coding agent — local tools, MCP, web search.</p>
    <div class="welcome-chips">
      <button>总结 README 和当前功能</button>
      <button>列出 src 目录并说明架构</button>
      <button>演示一次安全的文件写入</button>
      <button>用 Web Search 搜索 MCP 协议</button>
    </div>
  `;
  chat.appendChild(card);
  card.querySelectorAll("button").forEach((btn) => {
    btn.addEventListener("click", () => {
      input.value = btn.textContent;
      input.focus();
      autoSizeInput();
    });
  });
}

function hideWelcome() {
  const w = document.getElementById("welcome");
  if (w) w.remove();
}

/* ── history list ── */
async function loadHistory() {
  try {
    const res = await fetch("/api/history");
    const list = await res.json();
    sessionList.innerHTML = "";
    if (!Array.isArray(list) || list.length === 0) {
      sessionList.innerHTML = '<div class="session-empty">No sessions yet</div>';
      return;
    }
    for (const s of list.slice(0, 20)) {
      const item = document.createElement("div");
      item.className = "session-item";
      const title = s.title || s.name.replace(/^session-|\.jsonl$/g, "");
      const kb = s.size > 1024 ? (s.size / 1024).toFixed(1) + "KB" : s.size + "B";
      item.innerHTML = `<b>${escHtml(title)}</b><span>${kb} · ${s.modified || ""}</span>`;
      item.title = s.name;
      item.addEventListener("click", () => {
        input.value = `/load logs/${s.name}`;
        input.focus();
        autoSizeInput();
      });
      sessionList.appendChild(item);
    }
  } catch {
    sessionList.innerHTML = '<div class="session-empty">unavailable</div>';
  }
}

function message(role, text) {
  hideWelcome();
  const row = document.createElement("article");
  row.className = `message ${role}`;
  const meta = document.createElement("div");
  meta.className = "message-meta";
  meta.innerHTML = `<span>${role === "user" ? "you" : "assistant"}</span><span>${formatTime()}</span>`;
  const body = document.createElement("div");
  body.className = "message-body";
  body.innerHTML = text ? renderMarkdown(text) : "";

  if (role === "assistant" && text) {
    const actions = document.createElement("div");
    actions.className = "msg-actions";
    actions.innerHTML = '<button class="msg-act-btn" title="Copy message">Copy</button>';
    actions.querySelector("button").addEventListener("click", (e) => {
      e.stopPropagation();
      navigator.clipboard.writeText(text).then(() => {
        actions.querySelector("button").textContent = "Copied!";
        setTimeout(() => { actions.querySelector("button").textContent = "Copy"; }, 1500);
      }).catch(() => {});
    });
    row.appendChild(actions);
  }

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
    resetTypeBuffer();
    currentAssistant = null;
    message("user", event.detail);
  } else if (event.type === "assistant_chunk") {
    if (thinkingIndicator) {
      thinkingIndicator.remove();
      thinkingIndicator = null;
    }
    if (!currentAssistant) currentAssistant = message("assistant", "");
    feedTypeBuffer(event.detail || "");
  } else if (event.type === "assistant_message") {
    resetTypeBuffer();
    if (thinkingIndicator) {
      thinkingIndicator.remove();
      thinkingIndicator = null;
    }
    currentAssistant = null;
    message("assistant", event.detail || "");
  } else if (event.type === "assistant_done") {
    flushTypeBuffer();
    if (currentAssistant) {
      currentAssistant.innerHTML = renderMarkdown(streamText);
    }
    streamText = "";
    currentAssistant = null;
    if (thinkingIndicator) {
      thinkingIndicator.remove();
      thinkingIndicator = null;
    }
  } else if (event.type === "turn_done") {
    resetTypeBuffer();
    if (currentAssistant) {
      currentAssistant.innerHTML = renderMarkdown(streamText);
    }
    currentAssistant = null;
    if (thinkingIndicator) {
      thinkingIndicator.remove();
      thinkingIndicator = null;
    }
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
    updateConnection(true);
    statusText.textContent = "connected";
    statusDot.style.background = "var(--ok)";
    busyText.textContent = data.busy ? "busy" : "idle";

    if (data.busy) {
      send.textContent = "Stop";
      send.classList.add("stop-btn");
      send.dataset.stopping = "true";
      send.disabled = false;
    } else {
      send.textContent = "Run";
      send.classList.remove("stop-btn");
      delete send.dataset.stopping;
      send.disabled = false;
    }

    for (const event of data.events || []) handleEvent(event);
  } catch {
    updateConnection(false);
    statusText.textContent = "offline";
    statusDot.style.background = "var(--danger)";
  } finally {
    setTimeout(poll, 100);
  }
}

function showThinking() {
  if (thinkingIndicator) return;
  thinkingIndicator = document.createElement("div");
  thinkingIndicator.className = "thinking-indicator";
  thinkingIndicator.innerHTML = '<span></span><span></span><span></span>';
  chat.appendChild(thinkingIndicator);
  scrollToBottom(chat);
}

async function sendMessage() {
  if (send.dataset.stopping === "true") {
    send.textContent = "Run";
    send.classList.remove("stop-btn");
    sendHint.textContent = "stopped";
    delete send.dataset.stopping;
    return;
  }
  const content = input.value.trim();
  if (!content) return;
  input.value = "";
  autoSizeInput();
  send.disabled = true;
  sendHint.textContent = "running";
  resetTypeBuffer();
  currentAssistant = null;
  hideWelcome();
  showThinking();
  const res = await fetch("/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ content }),
  });
  sendHint.textContent = res.ok ? "accepted" : "rejected";
}

function autoSizeInput() {
  input.style.height = "auto";
  input.style.height = Math.min(input.scrollHeight, 160) + "px";
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
input.addEventListener("input", autoSizeInput);

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

showWelcome();
switchView(window.location.hash.slice(1) || "chat", false);
refreshStatus().catch(() => {});
loadHistory().catch(() => {});
poll();
