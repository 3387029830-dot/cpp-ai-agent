const path = require("path");

let pptxgen;
try {
  pptxgen = require("pptxgenjs");
} catch {
  pptxgen = require("C:/Users/Lenovo/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/pptxgenjs");
}

const pptx = new pptxgen();
pptx.layout = "LAYOUT_WIDE";
pptx.author = "cpp-ai-agent team";
pptx.company = "cpp-ai-agent";
pptx.subject = "C++17 terminal AI coding agent defense presentation";
pptx.title = "cpp-ai-agent 项目答辩";
pptx.lang = "zh-CN";
pptx.theme = {
  headFontFace: "Microsoft YaHei",
  bodyFontFace: "Microsoft YaHei",
  lang: "zh-CN",
};

const W = 13.333;
const H = 7.5;
const C = {
  ink: "172033",
  muted: "5E6B7A",
  paper: "F7FAFC",
  white: "FFFFFF",
  navy: "10243C",
  deep: "0B1220",
  teal: "0E7490",
  cyan: "0891B2",
  green: "0F766E",
  mint: "D9F99D",
  amber: "B45309",
  coral: "E11D48",
  line: "D8E1EA",
  soft: "EAF2F8",
  card: "FFFFFF",
};

const assets = path.resolve(__dirname, "assets");
const out = path.resolve(__dirname, "cpp-ai-agent-项目答辩.pptx");

function bg(slide, color = C.paper) {
  slide.background = { color };
}

function footer(slide, n) {
  slide.addText("cpp-ai-agent · C++17 终端 AI 编程智能体", {
    x: 0.5, y: 7.08, w: 6.8, h: 0.18,
    fontSize: 7.5, color: "7890A4", margin: 0,
  });
  slide.addText(String(n).padStart(2, "0"), {
    x: 12.28, y: 7.04, w: 0.48, h: 0.22,
    fontSize: 8.5, color: "7890A4", bold: true, align: "right", margin: 0,
  });
}

function title(slide, text, sub, n) {
  slide.addText(text, {
    x: 0.58, y: 0.38, w: 7.6, h: 0.46,
    fontFace: "Microsoft YaHei", fontSize: 24, bold: true,
    color: C.ink, margin: 0,
  });
  if (sub) {
    slide.addText(sub, {
      x: 0.6, y: 0.88, w: 8.8, h: 0.26,
      fontSize: 9.5, color: C.muted, margin: 0,
    });
  }
  slide.addShape(pptx.ShapeType.rect, {
    x: 11.68, y: 0.36, w: 1.05, h: 0.12,
    fill: { color: C.coral }, line: { color: C.coral, transparency: 100 },
  });
  footer(slide, n);
}

function card(slide, x, y, w, h, heading, body, color = C.teal) {
  slide.addShape(pptx.ShapeType.rect, {
    x, y, w, h,
    fill: { color: C.card },
    line: { color: C.line, width: 0.7 },
    shadow: { type: "outer", color: "000000", opacity: 0.10, blur: 1.2, offset: 1.1, angle: 45 },
  });
  slide.addShape(pptx.ShapeType.rect, {
    x, y, w: 0.08, h,
    fill: { color }, line: { color, transparency: 100 },
  });
  slide.addText(heading, {
    x: x + 0.22, y: y + 0.18, w: w - 0.4, h: 0.28,
    fontSize: 13, bold: true, color: C.ink, margin: 0,
  });
  slide.addText(body, {
    x: x + 0.22, y: y + 0.58, w: w - 0.42, h: h - 0.74,
    fontSize: 9.2, color: C.muted, breakLine: false,
    fit: "shrink", valign: "top", margin: 0.02,
  });
}

function pill(slide, x, y, text, color) {
  slide.addShape(pptx.ShapeType.roundRect, {
    x, y, w: 1.35, h: 0.34,
    rectRadius: 0.05,
    fill: { color },
    line: { color, transparency: 100 },
  });
  slide.addText(text, {
    x, y: y + 0.075, w: 1.35, h: 0.15,
    fontSize: 7.5, bold: true, color: C.white, align: "center", margin: 0,
  });
}

function addDiagram(slide, name, x, y, w, h) {
  slide.addShape(pptx.ShapeType.rect, {
    x: x - 0.06, y: y - 0.06, w: w + 0.12, h: h + 0.12,
    fill: { color: C.white }, line: { color: C.line, width: 0.8 },
    shadow: { type: "outer", color: "000000", opacity: 0.09, blur: 1.2, offset: 1.2, angle: 45 },
  });
  slide.addImage({
    path: path.join(assets, name),
    x, y, w, h,
  });
}

function commandBlock(slide, x, y, w, h, lines) {
  slide.addShape(pptx.ShapeType.rect, {
    x, y, w, h,
    fill: { color: "0F172A" },
    line: { color: "1E293B", width: 0.8 },
  });
  slide.addText(lines.join("\n"), {
    x: x + 0.22, y: y + 0.2, w: w - 0.44, h: h - 0.36,
    fontFace: "Consolas", fontSize: 8.5, color: "E2E8F0",
    breakLine: false, fit: "shrink", margin: 0,
  });
}

function metric(slide, x, y, num, label, color) {
  slide.addShape(pptx.ShapeType.rect, {
    x, y, w: 1.85, h: 1.1,
    fill: { color: C.white },
    line: { color: C.line, width: 0.7 },
  });
  slide.addText(num, {
    x, y: y + 0.18, w: 1.85, h: 0.36,
    fontSize: 23, bold: true, color, align: "center", margin: 0,
  });
  slide.addText(label, {
    x: x + 0.08, y: y + 0.66, w: 1.7, h: 0.22,
    fontSize: 8.2, color: C.muted, align: "center", margin: 0,
  });
}

let s;

// 1
s = pptx.addSlide();
s.background = { color: C.deep };
s.addShape(pptx.ShapeType.rect, { x: 0, y: 0, w: W, h: H, fill: { color: C.deep }, line: { color: C.deep } });
s.addShape(pptx.ShapeType.rect, { x: 0, y: 0, w: W, h: 0.18, fill: { color: C.coral }, line: { color: C.coral } });
s.addText("cpp-ai-agent", { x: 0.72, y: 1.15, w: 8.4, h: 0.7, fontSize: 42, bold: true, color: C.white, margin: 0 });
s.addText("C++17 终端 AI 编程智能体", { x: 0.78, y: 1.98, w: 6.8, h: 0.34, fontSize: 18, color: "BFE7F2", margin: 0 });
s.addText("从模型对话到工具执行、安全确认、日志回放、Web 搜索与 MCP 扩展的完整演示闭环", {
  x: 0.8, y: 2.55, w: 7.4, h: 0.55, fontSize: 13.5, color: "D8E8F2", margin: 0,
});
metric(s, 8.8, 1.42, "M0-M8", "阶段闭环", C.coral);
metric(s, 10.85, 1.42, "36", "doctest 通过", C.teal);
metric(s, 8.8, 2.82, "6+", "内置工具", C.amber);
metric(s, 10.85, 2.82, "4", "成员分工", C.green);
s.addText("答辩汇报 · 2026-07-11", { x: 0.82, y: 6.72, w: 4, h: 0.22, fontSize: 10, color: "94A3B8", margin: 0 });

// 2
s = pptx.addSlide(); bg(s); title(s, "项目定位：最小但完整的 AI Coding Agent", "目标不是复刻商业产品，而是展示 Agent 的完整架构和可运行闭环", 2);
card(s, 0.72, 1.48, 3.7, 2.0, "课程实践目标", "用 C++17 实现一个终端智能体，能够接入大模型并执行受控本地工具。", C.teal);
card(s, 4.85, 1.48, 3.7, 2.0, "核心判断标准", "不仅能聊天，还能让模型提出 tool_calls，程序执行工具并把真实结果回填。", C.coral);
card(s, 8.98, 1.48, 3.7, 2.0, "交付形态", "命令行可运行、文档可解释、测试可验证、现场可演示。", C.green);
s.addShape(pptx.ShapeType.rect, { x: 0.72, y: 4.15, w: 11.96, h: 1.55, fill: { color: C.navy }, line: { color: C.navy } });
s.addText("一句话概括", { x: 1.05, y: 4.45, w: 1.9, h: 0.25, fontSize: 12, color: "93C5FD", bold: true, margin: 0 });
s.addText("cpp-ai-agent 是一个能把“用户需求 → 模型决策 → 工具执行 → 安全确认 → 结果回填 → 最终回答”跑通的 C++17 终端 Agent。", {
  x: 1.05, y: 4.85, w: 11.0, h: 0.38, fontSize: 14.5, color: C.white, margin: 0,
});

// 3
s = pptx.addSlide(); bg(s); title(s, "汇报主线", "先说明为什么做，再说明怎么做，最后用测试和演示证明它能跑", 3);
["项目目标", "核心架构", "工具闭环", "M0-M8", "演示验收", "团队分工"].forEach((t, i) => {
  const x = 0.78 + i * 2.08;
  s.addShape(pptx.ShapeType.ellipse, { x, y: 2.02, w: 1.02, h: 1.02, fill: { color: [C.teal, C.coral, C.green, C.amber, C.cyan, C.navy][i] }, line: { color: "FFFFFF", width: 1.2 } });
  s.addText(String(i + 1), { x, y: 2.34, w: 1.02, h: 0.22, fontSize: 15, color: C.white, bold: true, align: "center", margin: 0 });
  s.addText(t, { x: x - 0.35, y: 3.25, w: 1.75, h: 0.25, fontSize: 11, bold: true, color: C.ink, align: "center", margin: 0 });
  if (i < 5) {
    s.addShape(pptx.ShapeType.line, { x: x + 1.07, y: 2.53, w: 0.9, h: 0, line: { color: C.line, width: 1.4, beginArrowType: "none", endArrowType: "triangle" } });
  }
});
card(s, 0.92, 4.42, 3.6, 1.35, "核心观点", "项目价值在完整闭环，而不是单个命令堆砌。", C.coral);
card(s, 4.88, 4.42, 3.6, 1.35, "演示策略", "每个命令都对应架构中的一个模块和测试点。", C.teal);
card(s, 8.84, 4.42, 3.6, 1.35, "答辩落点", "M0-M8 体现从能运行到能扩展、能解释、能复现。", C.green);

// 4
s = pptx.addSlide(); bg(s); title(s, "阶段路线：M0-M8 完整闭环", "从工程骨架到扩展能力，再到跨环境稳定性", 4);
addDiagram(s, "01-milestone-roadmap.svg", 0.9, 1.35, 11.55, 4.65);

// 5
s = pptx.addSlide(); bg(s); title(s, "系统架构：AgentLoop 是中枢", "main.cpp 装配模块，AgentLoop 连接模型、工具、安全、日志和扩展能力", 5);
addDiagram(s, "02-agent-architecture.svg", 0.78, 1.22, 7.9, 4.95);
card(s, 9.05, 1.35, 3.2, 1.05, "LlmClient", "负责 OpenAI-compatible 请求、SSE 流式输出和 tool_calls 解析。", C.teal);
card(s, 9.05, 2.68, 3.2, 1.05, "ToolRegistry", "注册本地工具并生成模型可读的 tools schema。", C.coral);
card(s, 9.05, 4.01, 3.2, 1.05, "PermissionManager", "按 Safe / Write / Dangerous 分级，驱动确认流程。", C.green);
card(s, 9.05, 5.34, 3.2, 1.05, "JsonLogger", "记录执行过程，支撑 /history、/replay 和 /load。", C.amber);

// 6
s = pptx.addSlide(); bg(s); title(s, "核心闭环：模型不是直接操作系统", "模型提出工具调用，程序负责安全执行，再把结果交给模型继续推理", 6);
addDiagram(s, "03-tool-call-loop.svg", 1.0, 1.25, 11.35, 4.55);
s.addText("这条闭环解释了项目和普通聊天程序的区别：AI 的输出会转化为受控动作，并且每一步都有权限与日志。", {
  x: 1.08, y: 6.15, w: 11.1, h: 0.32, fontSize: 12.5, color: C.ink, bold: true, align: "center", margin: 0,
});

// 7
s = pptx.addSlide(); bg(s); title(s, "M0-M5：核心 Agent 交付", "先把智能体最小闭环跑通，再讨论外部能力", 7);
[
  ["M0/M0.5", "工程骨架", "CMake、vcpkg、Preset、测试目录、配置文件"],
  ["M1", "模型接入", "OpenAI-compatible chat/completions、Provider 配置"],
  ["M2", "工具系统", "ITool、ToolRegistry、read/write/edit/list/run"],
  ["M3", "AgentLoop", "tools schema、tool_calls、工具结果回填"],
  ["M4", "安全日志", "权限确认、危险命令、JSONL、history/replay"],
  ["M5", "演示增强", "doctor、demo、ui、skills、交付文档"],
].forEach((row, i) => {
  const x = i % 3 === 0 ? 0.8 : i % 3 === 1 ? 4.7 : 8.6;
  const y = i < 3 ? 1.42 : 4.0;
  card(s, x, y, 3.25, 1.55, `${row[0]} · ${row[1]}`, row[2], [C.teal, C.coral, C.green, C.amber, C.cyan, C.navy][i]);
});

// 8
s = pptx.addSlide(); bg(s); title(s, "M6-M8：扩展能力与工程稳定", "让 Agent 更接近真实使用场景，也更适合课程答辩现场复现", 8);
card(s, 0.78, 1.35, 3.8, 3.65, "M6 · Web 搜索", "新增 /search 和 web_search 工具；Bing RSS 优先，DuckDuckGo/Bing 链接兜底；支持 WEB_SEARCH_PROXY_URL；修复 Windows 中文参数。", C.teal);
card(s, 4.78, 1.35, 3.8, 3.65, "M7 · MCP 最小客户端", "实现 stdio initialize、tools/list、tools/call；新增内置测试 server；McpToolAdapter 将外部工具接入 AgentLoop。", C.coral);
card(s, 8.78, 1.35, 3.8, 3.65, "M8 · 跨环境稳定", "AGENTS.md 项目规范注入；Visual Studio generator 默认构建；/load 修复；GitHub/GitLab 同步。", C.green);
s.addText("扩展不是为了堆功能，而是证明架构有边界、有接口、有复现路径。", {
  x: 1.05, y: 5.75, w: 11.1, h: 0.28, fontSize: 13.2, bold: true, color: C.ink, align: "center", margin: 0,
});

// 9
s = pptx.addSlide(); bg(s); title(s, "关键模块一：LLM 与 Agent 主循环", "负责同模型沟通、解析 tool_calls、维护上下文窗口", 9);
card(s, 0.75, 1.32, 3.45, 1.38, "LlmClient", "OpenAI-compatible API；SSE 流式输出；tool_calls 增量累积。", C.teal);
card(s, 4.55, 1.32, 3.45, 1.38, "AgentLoop", "发送 tools schema；分发工具调用；回填 tool 消息；最大轮次保护。", C.coral);
card(s, 8.35, 1.32, 3.45, 1.38, "ContextManager", "保留 system prompt；按 Token 预算管理上下文；避免消息无限增长。", C.green);
commandBlock(s, 1.02, 3.35, 11.15, 1.75, [
  "用户需求 -> AgentLoop",
  "AgentLoop -> LlmClient(chatStream + tools schema)",
  "LLM -> assistant content 或 tool_calls",
  "tool result -> tool message -> LLM 再推理",
]);
s.addText("负责人：王博轩", { x: 0.85, y: 6.0, w: 2.8, h: 0.26, fontSize: 12.5, bold: true, color: C.ink, margin: 0 });

// 10
s = pptx.addSlide(); bg(s); title(s, "关键模块二：工具、安全与日志", "把模型请求变成受控动作，并保证可确认、可追溯", 10);
[
  ["工具注册", "read_file / write_file / edit_file / list_dir / run_command / web_search"],
  ["路径沙箱", "所有文件工具限制在 workspace 内，防止越界读取或写入。"],
  ["风险分级", "Safe / Write / Dangerous，不同风险触发不同权限策略。"],
  ["日志回放", "JSONL 记录 user / assistant / tool_call / tool_result，可 history/replay/load。"],
].forEach((row, i) => {
  const x = i % 2 === 0 ? 0.8 : 6.85;
  const y = i < 2 ? 1.45 : 3.85;
  card(s, x, y, 5.35, 1.55, row[0], row[1], [C.teal, C.coral, C.green, C.amber][i]);
});
s.addText("负责人：康艺凡", { x: 0.85, y: 6.35, w: 2.8, h: 0.26, fontSize: 12.5, bold: true, color: C.ink, margin: 0 });

// 11
s = pptx.addSlide(); bg(s); title(s, "关键模块三：交互、Skill、MCP 与搜索", "让 Agent 具备更好的演示入口和外部扩展能力", 11);
[
  ["Console UI", "ANSI 语义化输出；Conversation / Tools / Status / Input 概览；流式打字机效果。"],
  ["Skill", "code_review、cpp_debug、project_summary、test_writer；allowed_tools 白名单双重约束。"],
  ["MCP", "stdio MCP 握手、工具发现、工具调用；支持外部 server 配置。"],
  ["Web 搜索", "Bing RSS 优先；代理配置；中文查询参数修复；失败时提供兜底链接。"],
].forEach((row, i) => {
  card(s, 0.85 + i * 3.1, 1.5, 2.62, 3.9, row[0], row[1], [C.teal, C.coral, C.green, C.amber][i]);
});
s.addText("负责人：魏宇鑫", { x: 0.9, y: 6.12, w: 2.8, h: 0.26, fontSize: 12.5, bold: true, color: C.ink, margin: 0 });

// 12
s = pptx.addSlide(); bg(s); title(s, "测试与验收：36 个 doctest 全部通过", "用自动化测试和手动演示命令覆盖核心路径", 12);
addDiagram(s, "05-test-coverage.svg", 0.82, 1.18, 7.1, 4.65);
metric(s, 8.42, 1.42, "100%", "测试通过率", C.green);
metric(s, 10.55, 1.42, "36", "doctest 用例", C.teal);
metric(s, 8.42, 2.92, "10", "测试文件", C.coral);
metric(s, 10.55, 2.92, "M0-M8", "验收范围", C.amber);
commandBlock(s, 8.25, 4.58, 3.95, 1.18, [
  "cmake --preset msvc-vcpkg-debug",
  "cmake --build --preset msvc-vcpkg-debug",
  "ctest -C Debug --output-on-failure",
]);

// 13
s = pptx.addSlide(); bg(s); title(s, "现场演示路线", "每个命令对应一个可解释的架构能力", 13);
addDiagram(s, "04-demo-flow.svg", 0.78, 1.15, 7.25, 4.72);
commandBlock(s, 8.45, 1.25, 3.85, 4.55, [
  "ai-agent.exe /doctor",
  "主对话输入 /",
  "请读取 README.md 并总结",
  "请创建 temp-demo.txt",
  "ai-agent.exe /search \"MCP 是什么\"",
  "ai-agent.exe /skills",
  "ai-agent.exe /mcp-demo",
  "ai-agent.exe /history",
]);

// 14
s = pptx.addSlide(); bg(s); title(s, "风险与预案", "答辩现场最怕临时环境问题，所以提前准备替代路径", 14);
[
  ["网络不稳定", "搜索可能超时", "Bing RSS 优先、代理配置、兜底搜索链接"],
  ["模型配置错误", "LLM 无法响应", "/doctor 检查 base_url、model、key 和代理"],
  ["构建环境差异", "新电脑编译失败", "默认 Visual Studio generator，NMake 作为备用"],
  ["MCP 外部服务不可用", "外部 server 启动失败", "使用内置 MCP demo 完成协议握手演示"],
].forEach((row, i) => {
  const y = 1.28 + i * 1.18;
  s.addShape(pptx.ShapeType.rect, { x: 0.85, y, w: 11.7, h: 0.82, fill: { color: C.white }, line: { color: C.line, width: 0.7 } });
  pill(s, 1.05, y + 0.22, row[0], [C.coral, C.amber, C.teal, C.green][i]);
  s.addText(row[1], { x: 2.65, y: y + 0.24, w: 2.5, h: 0.18, fontSize: 9.5, bold: true, color: C.ink, margin: 0 });
  s.addText(row[2], { x: 5.25, y: y + 0.22, w: 6.8, h: 0.24, fontSize: 9.2, color: C.muted, margin: 0 });
});

// 15
s = pptx.addSlide(); bg(s); title(s, "团队分工", "四条责任线对应代码模块、测试文件和答辩问题", 15);
[
  ["樊浩", "总体集成与演示", "项目定位、M0-M8 总览、GitHub/GitLab 同步、现场排错", C.navy],
  ["王博轩", "LLM 与 Agent 主循环", "LlmClient、AgentLoop、tool_calls、ContextManager、AGENTS.md", C.teal],
  ["康艺凡", "工具、安全与日志", "ToolRegistry、本地工具、权限确认、路径沙箱、JSONL 回放", C.coral],
  ["魏宇鑫", "交互与扩展能力", "Console UI、Skill、MCP、Web 搜索、扩展边界说明", C.green],
].forEach((row, i) => {
  const x = 0.78 + i * 3.12;
  s.addShape(pptx.ShapeType.rect, { x, y: 1.35, w: 2.64, h: 4.35, fill: { color: C.white }, line: { color: C.line, width: 0.7 } });
  s.addShape(pptx.ShapeType.rect, { x, y: 1.35, w: 2.64, h: 0.7, fill: { color: row[3] }, line: { color: row[3] } });
  s.addText(row[0], { x, y: 1.56, w: 2.64, h: 0.2, fontSize: 13, bold: true, color: C.white, align: "center", margin: 0 });
  s.addText(row[1], { x: x + 0.18, y: 2.35, w: 2.28, h: 0.32, fontSize: 12, bold: true, color: C.ink, align: "center", margin: 0 });
  s.addText(row[2], { x: x + 0.22, y: 3.05, w: 2.18, h: 1.7, fontSize: 8.7, color: C.muted, fit: "shrink", align: "center", valign: "mid", margin: 0.02 });
});

// 16
s = pptx.addSlide();
s.background = { color: C.deep };
s.addShape(pptx.ShapeType.rect, { x: 0, y: 0, w: W, h: H, fill: { color: C.deep }, line: { color: C.deep } });
s.addText("总结", { x: 0.82, y: 0.98, w: 3, h: 0.48, fontSize: 30, bold: true, color: C.white, margin: 0 });
s.addText("cpp-ai-agent 已完成从“能运行”到“能演示、能测试、能扩展、能解释”的阶段闭环。", {
  x: 0.85, y: 1.82, w: 9.8, h: 0.42, fontSize: 17, color: "D8E8F2", margin: 0,
});
[
  ["完整闭环", "模型决策、工具调用、安全确认、结果回填、日志追踪"],
  ["工程可信", "CMake/vcpkg、Visual Studio generator、36 个 doctest"],
  ["可扩展", "Web 搜索、Skill、MCP、AGENTS.md 项目规范"],
].forEach((row, i) => {
  card(s, 0.9 + i * 4.05, 3.0, 3.45, 1.7, row[0], row[1], [C.coral, C.teal, C.green][i]);
});
s.addText("Q & A", { x: 0.9, y: 5.92, w: 3.2, h: 0.5, fontSize: 30, bold: true, color: C.white, margin: 0 });
s.addText("谢谢老师和同学", { x: 0.94, y: 6.52, w: 3.2, h: 0.25, fontSize: 12, color: "94A3B8", margin: 0 });

pptx.writeFile({ fileName: out });
