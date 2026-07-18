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
const workflowCards = document.getElementById("workflow-cards");
const expertCards = document.getElementById("expert-cards");
const runCards = document.getElementById("run-cards");
const activeWorkflowText = document.getElementById("active-workflow");
const activeContractText = document.getElementById("active-contract");
const activeExpertText = document.getElementById("active-expert");
const activeSkillText = document.getElementById("active-skill");
const clearModeButton = document.getElementById("clear-mode");

let lastEventId = 0;
let eventsSeen = 0;
let uploadsSeen = 0;
let currentAssistant = null;
let thinkingIndicator = null;
const seenEventIds = new Set();
let currentView = "chat";
let statusCache = { tools: [], skills: [], workflows: [], experts: [], commands: [] };

/* 鈹€鈹€ typewriter buffer for smooth streaming 鈹€鈹€ */
let typeBuffer = "";
let typeTimer = null;
let streamText = "";
const TYPE_SPEED = 18; // chars per frame (~60fps)

/* 鈹€鈹€ markdown renderer 鈹€鈹€ */
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
  const btn = '<button class="copy-btn" onclick="copyCode(this)">复制</button>';
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

/* 鈹€鈹€ code copy 鈹€鈹€ */
window.copyCode = function(btn) {
  const block = btn.closest('.code-block');
  const code = block ? block.querySelector('code').textContent : '';
  navigator.clipboard.writeText(code).then(() => {
    btn.textContent = '已复制';
    setTimeout(() => { btn.textContent = '复制'; }, 1800);
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
  chat: ["本地智能体工作区", "控制台"],
  tools: ["工具注册表", "工具"],
  skills: ["技能目录", "技能"],
  modes: ["直接提示注入", "模式"],
  runs: ["演示命令中心", "运行"],
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

/* 鈹€鈹€ welcome screen 鈹€鈹€ */
function showWelcome() {
  const card = document.createElement("div");
  card.className = "welcome-card";
  card.id = "welcome";
  card.innerHTML = `
    <div class="welcome-mark">F</div>
    <h2>FORGE-17</h2>
    <p>面向答辩演示的 C++17 AI 编程智能体，支持本地工具、工作流模式、专家模式、MCP 和 Web 控制台。</p>
    <div class="welcome-chips">
      <button>总结 README 和核心功能</button>
      <button>说明 src 架构</button>
      <button>演示一次安全写文件</button>
      <button>解释 MCP 和专家模式</button>
    </div>
  `;
  card.querySelector(".welcome-mark").textContent = "F";
  card.querySelector("h2").textContent = "FORGE-17";
  card.querySelector("p").textContent = "面向答辩演示的 C++17 AI 编程智能体，支持本地工具、工作流模式、专家模式、MCP 和 Web 控制台。";
  ["总结 README 和核心功能", "说明 src 架构", "演示一次安全写文件", "解释 MCP 和专家模式"].forEach((text, index) => {
    const button = card.querySelectorAll(".welcome-chips button")[index];
    if (button) button.textContent = text;
  });
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

/* 鈹€鈹€ history list 鈹€鈹€ */
async function loadHistory() {
  try {
    const res = await fetch("/api/history");
    const list = await res.json();
    sessionList.innerHTML = "";
    if (!Array.isArray(list) || list.length === 0) {
      sessionList.innerHTML = '<div class="session-empty">暂无会话</div>';
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
    sessionList.innerHTML = '<div class="session-empty">不可用</div>';
  }
}

function message(role, text) {
  hideWelcome();
  const row = document.createElement("article");
  row.className = `message ${role}`;
  const meta = document.createElement("div");
  meta.className = "message-meta";
  meta.innerHTML = `<span>${role === "user" ? "你" : "助手"}</span><span>${formatTime()}</span>`;
  const body = document.createElement("div");
  body.className = "message-body";
  body.innerHTML = text ? renderMarkdown(text) : "";

  if (role === "assistant" && text) {
    const actions = document.createElement("div");
    actions.className = "msg-actions";
    actions.innerHTML = '<button class="msg-act-btn" title="复制消息">复制</button>';
    actions.querySelector("button").addEventListener("click", (e) => {
      e.stopPropagation();
      navigator.clipboard.writeText(text).then(() => {
        actions.querySelector("button").textContent = "已复制";
        setTimeout(() => { actions.querySelector("button").textContent = "复制"; }, 1500);
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
  const labels = {
    ask_each_time: "每次询问",
    read_only: "只读",
    trust_session: "信任会话",
  };
  document.getElementById("permission-mode-label").textContent = labels[mode] || "每次询问";
  document.querySelectorAll("#permission-modes button").forEach((button) => {
    button.classList.toggle("active", button.dataset.mode === mode);
  });
}

function hydrateStatus(status) {
  statusCache = {
    tools: status.tools || [],
    skills: status.skills || [],
    workflows: status.workflows || [],
    experts: status.experts || [],
    commands: status.commands || [],
  };
  document.getElementById("model").textContent = status.model || "模型";
  document.getElementById("workspace").textContent = status.workspace || ".";
  document.getElementById("history").textContent = status.history || "日志";
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
  activeWorkflowText.textContent = status.active_workflow?.name || "基础";
  activeContractText.textContent = status.active_workflow?.contract_template || "未定义";
  activeExpertText.textContent = status.active_expert?.name || "通用";
  activeSkillText.textContent = status.active_skill?.name || status.active_expert?.skill || "无";
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
    const risk = event.data?.risk ? ` 路 ${event.data.risk}` : "";
    activity("tool", `${event.title}${risk}`, event.detail);
  } else if (event.type === "tool_result") {
    activity("tool", `${event.title} 结果`, event.detail);
  } else if (event.type === "warning") {
    activity("warning", "警告", event.detail);
  } else if (event.type === "error") {
    activity("error", "错误", event.detail);
  } else if (event.type === "permission") {
    const data = event.data || {};
    permTitle.textContent = `${data.tool || event.title} 路 ${data.risk || "unknown"}`;
    permPreview.textContent = data.preview || data.arguments || event.detail || "";
    permission.classList.add("visible");
    activity("permission", "权限请求", permTitle.textContent);
  } else if (event.type === "permission_result") {
    activity("permission", "权限结果", event.detail);
  } else if (event.type === "upload") {
    uploadsSeen += 1;
    uploadCount.textContent = String(uploadsSeen);
    activity("upload", `已上传 ${event.title}`, event.detail);
  } else if (event.type === "settings") {
    activity("settings", "设置", event.detail);
  } else if (event.type === "mode") {
    activity("mode", event.title || "模式", event.detail);
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
  if (risk === "dangerous") return "危险命令";
  if (risk === "write") return "写入权限";
  return "只读";
}

async function applyMode(kind, name = "") {
  const body = kind === "clear" ? { action: "clear" } : { kind, name };
  sendHint.textContent = kind === "clear" ? "正在清除模式" : `正在注入 ${name}`;
  const res = await fetch("/api/mode", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
  });
  const data = await res.json().catch(() => ({}));
  if (data.status) hydrateStatus(data.status);
  sendHint.textContent = data.ok === false ? "模式注入失败" : "模式已注入";
  if (data.error) activity("error", "mode", data.error);
}

function renderMainViews() {
  toolCards.innerHTML = "";
  skillCards.innerHTML = "";
  workflowCards.innerHTML = "";
  expertCards.innerHTML = "";
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
      "技能预设",
      `${skill.description || ""}${allowed ? ` · 工具：${allowed}` : ""}`,
      "skill",
      () => {
        input.value = `/use-skill ${skill.name}`;
        input.focus();
      },
    ));
  }

  for (const workflow of statusCache.workflows) {
    const allowed = (workflow.tools || []).join(", ");
    const contract = workflow.contract_template || "";
    workflowCards.appendChild(card(
      workflow.name,
      "工作流模式",
      `${workflow.description || ""}${contract ? ` · 合同：${contract}` : ""}${allowed ? ` · 工具：${allowed}` : ""}`,
      "workflow",
      () => applyMode("workflow", workflow.name),
    ));
  }

  for (const expert of statusCache.experts) {
    expertCards.appendChild(card(
      expert.name,
      expert.skill ? `专家包 · ${expert.skill}` : "专家包",
      expert.description || "",
      "expert",
      () => applyMode("expert", expert.name),
    ));
  }

  for (const command of statusCache.commands) {
    runCards.appendChild(card(
      command,
      "命令",
      "点击后复制到输入框。",
      "command",
      () => {
        input.value = command
          .replace(" <query>", " MCP 是什么")
          .replace(" <log.jsonl>", "")
          .replace(" <name> [target]", "");
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
    statusText.textContent = "已连接";
    statusDot.style.background = "var(--ok)";
    busyText.textContent = data.busy ? "忙碌" : "空闲";

    if (data.busy) {
      send.textContent = "停止";
      send.classList.add("stop-btn");
      send.dataset.stopping = "true";
      send.disabled = false;
    } else {
      send.textContent = "发送";
      send.classList.remove("stop-btn");
      delete send.dataset.stopping;
      send.disabled = false;
    }

    for (const event of data.events || []) handleEvent(event);
  } catch {
    updateConnection(false);
    statusText.textContent = "离线";
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
    send.textContent = "发送";
    send.classList.remove("stop-btn");
    sendHint.textContent = "已停止";
    delete send.dataset.stopping;
    return;
  }
  const content = input.value.trim();
  if (!content) return;
  input.value = "";
  autoSizeInput();
  send.disabled = true;
  sendHint.textContent = "正在运行";
  resetTypeBuffer();
  currentAssistant = null;
  hideWelcome();
  showThinking();
  const res = await fetch("/api/chat", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ content }),
  });
  sendHint.textContent = res.ok ? "已接受" : "已拒绝";
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
      activity("error", `上传失败 ${file.name}`, await res.text());
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
clearModeButton.addEventListener("click", () => applyMode("clear"));
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

