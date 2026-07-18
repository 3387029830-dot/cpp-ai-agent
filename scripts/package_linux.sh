#!/usr/bin/env bash
# =============================================================================
# cpp-ai-agent Linux 打包脚本
# =============================================================================
# 用法:
#   bash scripts/package_linux.sh
#
# 输出: build-output/ai-agent-linux.tar.gz
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build/linux-gcc-release"
OUTPUT_DIR="$PROJECT_DIR/build-output"
PACKAGE_DIR="$OUTPUT_DIR/ai-agent-linux"
VERSION="${CI_COMMIT_SHORT_SHA:-$(git rev-parse --short HEAD 2>/dev/null || echo 'dev')}"
PACKAGE_FILE="ai-agent-v${VERSION}-linux-x64.tar.gz"

export VCPKG_ROOT="${VCPKG_ROOT:-/opt/vcpkg}"

# ---- build ----
echo "=== Building Release ==="
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release -j "$(nproc)"

# ---- package ----
echo "=== Packaging ==="
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR/bin"
mkdir -p "$PACKAGE_DIR/config"
mkdir -p "$PACKAGE_DIR/web"

cp "$BUILD_DIR/ai-agent" "$PACKAGE_DIR/bin/" 2>/dev/null || {
    echo "[WARN] Release binary not found, trying Debug..."
    cp "$PROJECT_DIR/build/linux-gcc-debug/ai-agent" "$PACKAGE_DIR/bin/" 2>/dev/null
}

cp "$PROJECT_DIR/config/"*.json "$PACKAGE_DIR/config/" 2>/dev/null || true
cp -r "$PROJECT_DIR/web/"* "$PACKAGE_DIR/web/" 2>/dev/null || true
cp "$PROJECT_DIR/.env.example" "$PACKAGE_DIR/" 2>/dev/null || true
cp "$PROJECT_DIR/README.md" "$PACKAGE_DIR/" 2>/dev/null || true
cp "$PROJECT_DIR/AGENTS.md" "$PACKAGE_DIR/" 2>/dev/null || true

# ---- launcher ----
cat > "$PACKAGE_DIR/start-here.sh" << 'LAUNCHER'
#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo
echo "  cpp-ai-agent v0.2.0"
echo
echo "  用法:"
echo "    ./bin/ai-agent             # 终端 TUI 模式"
echo "    ./bin/ai-agent /web 8080  # 浏览器 Web 模式"
echo "    ./bin/ai-agent /doctor    # 诊断配置"
echo "    ./bin/ai-agent /demo      # 演示模式"
echo "    ./bin/ai-agent /skills    # 查看 Skill"
echo
echo "  先编辑 .env 填入你的 API Key 再启动！"
echo
exec bash
LAUNCHER
chmod +x "$PACKAGE_DIR/start-here.sh"

# ---- tarball ----
cd "$OUTPUT_DIR"
tar -czf "$PACKAGE_FILE" "ai-agent-linux"
rm -rf "$PACKAGE_DIR"
cd "$PROJECT_DIR"

echo
echo "=============================================="
echo "  Package ready"
echo "=============================================="
echo "  $OUTPUT_DIR/$PACKAGE_FILE  ($(du -h "$OUTPUT_DIR/$PACKAGE_FILE" | cut -f1))"
echo "=============================================="
