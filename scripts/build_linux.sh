#!/usr/bin/env bash
# =============================================================================
# cpp-ai-agent Linux 构建 & 测试脚本
# =============================================================================
# 用法:
#   bash scripts/build_linux.sh [debug|release]
#
# 示例:
#   bash scripts/build_linux.sh debug      # Debug 构建 + 测试
#   bash scripts/build_linux.sh release    # Release 构建 + 打包
# =============================================================================

set -euo pipefail

# ---- 参数解析 ------------------------------------------------------------
BUILD_TYPE="${1:-debug}"

case "$BUILD_TYPE" in
    debug)   PRESET="linux-gcc-debug"   ;;
    release) PRESET="linux-gcc-release" ;;
    *)
        echo "用法: $0 [debug|release]"
        echo "  debug   - Debug 构建 + CTest 测试"
        echo "  release - Release 构建 + 打包 tar.gz"
        exit 1
        ;;
esac

# ---- 配置 ----------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build/$PRESET"
VCPKG_ROOT="${VCPKG_ROOT:-/opt/vcpkg}"

export VCPKG_ROOT

# ---- 颜色输出 ------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
err()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }
step()  { echo -e "${CYAN}[STEP]${NC}  $*"; }

# ---- 前置检查 ------------------------------------------------------------
command -v cmake &>/dev/null || err "cmake 未找到，先执行 scripts/setup_linux.sh"
command -v g++   &>/dev/null || err "g++ 未找到，先执行 scripts/setup_linux.sh"

if [[ ! -d "$VCPKG_ROOT" ]]; then
    err "VCPKG_ROOT ($VCPKG_ROOT) 不存在，先执行 scripts/setup_linux.sh"
fi

cd "$PROJECT_DIR"
info "项目目录: $PROJECT_DIR"
info "构建类型: $BUILD_TYPE (preset: $PRESET)"
info "VCPKG_ROOT: $VCPKG_ROOT"

# =========================================================================
# Step 1: CMake 配置
# =========================================================================
step "CMake 配置..."
cmake --preset "$PRESET"
info "配置完成"

# =========================================================================
# Step 2: 构建
# =========================================================================
step "CMake 构建 (parallel: $(nproc) jobs)..."
START_TIME=$(date +%s)

cmake --build --preset "$PRESET" -j "$(nproc)"

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))
info "构建完成，耗时 ${DURATION}s"

# 检查产物
BINARY="$BUILD_DIR/ai-agent"
if [[ -f "$BINARY" ]]; then
    info "产物: $BINARY"
    file "$BINARY"
else
    warn "未找到预期的产物 ai-agent，构建目录中有:"
    ls "$BUILD_DIR/" 2>/dev/null | head -20 || true
fi
echo ""

# =========================================================================
# Step 3: 测试 (仅 Debug)
# =========================================================================
if [[ "$BUILD_TYPE" == "debug" ]]; then
    step "运行 CTest..."
    cd "$BUILD_DIR"
    ctest --output-on-failure -C Debug || warn "部分测试未通过，请查看上方日志"
    cd "$PROJECT_DIR"
    echo ""
fi

# =========================================================================
# Step 4: 打包 (仅 Release)
# =========================================================================
if [[ "$BUILD_TYPE" == "release" ]]; then
    step "打包 Release 制品..."
    VERSION="${CI_COMMIT_SHORT_SHA:-$(git rev-parse --short HEAD 2>/dev/null || echo 'dev')}"
    PACKAGE_NAME="ai-agent-v${VERSION}-linux-x64"
    PACKAGE_FILE="${PACKAGE_NAME}.tar.gz"

    rm -rf "/tmp/${PACKAGE_NAME}"
    mkdir -p "/tmp/${PACKAGE_NAME}/bin"
    mkdir -p "/tmp/${PACKAGE_NAME}/config"

    # 复制产物
    if [[ -f "$BUILD_DIR/ai-agent" ]]; then
        cp "$BUILD_DIR/ai-agent" "/tmp/${PACKAGE_NAME}/bin/"
    fi
    # 复制配置文件
    cp "$PROJECT_DIR/config/"*.json "/tmp/${PACKAGE_NAME}/config/" 2>/dev/null || true
    cp "$PROJECT_DIR/.env.example" "/tmp/${PACKAGE_NAME}/" 2>/dev/null || true
    cp "$PROJECT_DIR/README.md" "/tmp/${PACKAGE_NAME}/" 2>/dev/null || true

    # 打包
    cd /tmp
    tar -czf "$PROJECT_DIR/$PACKAGE_FILE" "$PACKAGE_NAME"
    rm -rf "/tmp/${PACKAGE_NAME}"
    cd "$PROJECT_DIR"

    info "制品: $PACKAGE_FILE ($(du -h "$PACKAGE_FILE" | cut -f1))"
fi

# =========================================================================
# 汇总
# =========================================================================
echo ""
echo "=============================================="
echo "  构建完成"
echo "=============================================="
echo "  类型:       $BUILD_TYPE"
echo "  Preset:     $PRESET"
echo "  构建目录:   $BUILD_DIR"
echo "=============================================="
