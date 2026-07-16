#!/usr/bin/env bash
# =============================================================================
# cpp-ai-agent Linux 构建环境初始化脚本
# =============================================================================
# 用法:
#   chmod +x scripts/setup_linux.sh
#   sudo bash scripts/setup_linux.sh
#
# 功能：一键安装 GCC/CMake/Ninja/vcpkg/Ansible，可重复执行（幂等）
# 适用：Ubuntu / Debian
# =============================================================================

set -euo pipefail

# ---- 配置 ----------------------------------------------------------------
GCC_VERSION="${GCC_VERSION:-13}"
CMAKE_VERSION="${CMAKE_VERSION:-3.28.3}"
VCPKG_DIR="${VCPKG_DIR:-/opt/vcpkg}"
VCPKG_TRIPLET="${VCPKG_TRIPLET:-x64-linux}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# ---- 颜色输出 ------------------------------------------------------------
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
err()   { echo -e "${RED}[ERROR]${NC} $*"; exit 1; }
step()  { echo -e "${CYAN}[STEP]${NC}  $*"; }

# ---- 权限检查 ------------------------------------------------------------
if [[ $EUID -ne 0 ]]; then
    err "请用 root 权限运行: sudo bash scripts/setup_linux.sh"
fi

# ---- 检测操作系统 ------------------------------------------------------------
if [[ ! -f /etc/os-release ]]; then
    err "不支持的操作系统（需要 Ubuntu/Debian）"
fi
. /etc/os-release
info "检测到系统: $ID $VERSION_ID"

# =========================================================================
# Step 1: 安装系统软件包
# =========================================================================
step "安装系统编译依赖..."
apt-get update -qq
apt-get install -y -qq --no-install-recommends \
    build-essential \
    gcc-${GCC_VERSION} \
    g++-${GCC_VERSION} \
    gdb \
    curl \
    wget \
    git \
    unzip \
    zip \
    tar \
    pkg-config \
    ninja-build \
    python3 \
    python3-pip \
    openssh-client \
    ca-certificates \
    libssl-dev \
    zlib1g-dev

# 设置 GCC 默认版本
if command -v "gcc-${GCC_VERSION}" &>/dev/null; then
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${GCC_VERSION} 100 2>/dev/null || true
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-${GCC_VERSION} 100 2>/dev/null || true
    update-alternatives --set gcc /usr/bin/gcc-${GCC_VERSION} 2>/dev/null || true
    update-alternatives --set g++ /usr/bin/g++-${GCC_VERSION} 2>/dev/null || true
fi

info "GCC: $(gcc --version | head -1)"
info "G++: $(g++ --version | head -1)"
info "Ninja: $(ninja --version 2>/dev/null || echo '未安装')"

# =========================================================================
# Step 2: 安装 CMake（版本 >= 3.20）
# =========================================================================
step "检查 CMake 版本..."
install_cmake() {
    if command -v cmake &>/dev/null; then
        local current_ver
        current_ver=$(cmake --version | head -1 | grep -oP '\d+\.\d+\.\d+' || echo "0.0.0")
        if [[ "$(printf '%s\n' "$CMAKE_VERSION" "$current_ver" | sort -V | head -1)" == "$CMAKE_VERSION" ]]; then
            info "CMake $current_ver 已满足要求 (>= $CMAKE_VERSION)，跳过"
            return
        fi
        warn "当前 CMake $current_ver 低于 $CMAKE_VERSION，升级中..."
    fi

    info "安装 CMake ${CMAKE_VERSION}..."
    local cmake_dir="cmake-${CMAKE_VERSION}-linux-x86_64"
    local cmake_archive="${cmake_dir}.tar.gz"
    wget -q "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${cmake_archive}" -O /tmp/${cmake_archive}
    tar -xzf /tmp/${cmake_archive} -C /opt/
    rm -f /tmp/${cmake_archive}

    ln -sf "/opt/${cmake_dir}/bin/cmake" /usr/local/bin/cmake
    ln -sf "/opt/${cmake_dir}/bin/ctest" /usr/local/bin/ctest
    ln -sf "/opt/${cmake_dir}/bin/cpack" /usr/local/bin/cpack

    info "CMake: $(cmake --version | head -1)"
}
install_cmake

# =========================================================================
# Step 3: 安装 vcpkg
# =========================================================================
step "安装 vcpkg..."
install_vcpkg() {
    if [[ -d "$VCPKG_DIR" && -x "$VCPKG_DIR/vcpkg" ]]; then
        info "vcpkg 已安装在 $VCPKG_DIR，更新中..."
        cd "$VCPKG_DIR"
        git pull --ff-only 2>/dev/null || true
        ./bootstrap-vcpkg.sh -disableMetrics 2>/dev/null || true
    else
        info "克隆 vcpkg 到 $VCPKG_DIR..."
        git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
        cd "$VCPKG_DIR"
        ./bootstrap-vcpkg.sh -disableMetrics
    fi

    # 设置全局环境变量
    cat > /etc/profile.d/vcpkg.sh << 'VCPKGEOF'
export VCPKG_ROOT=/opt/vcpkg
export PATH=$VCPKG_ROOT:$PATH
VCPKGEOF
    chmod +x /etc/profile.d/vcpkg.sh

    export VCPKG_ROOT="$VCPKG_DIR"
    export PATH="$VCPKG_DIR:$PATH"

    info "vcpkg: $("$VCPKG_DIR/vcpkg" version 2>/dev/null | head -1)"
}
install_vcpkg

# =========================================================================
# Step 4: 预安装项目 vcpkg 依赖
# =========================================================================
if [[ -f "$PROJECT_DIR/vcpkg.json" ]]; then
    step "预安装项目 vcpkg 依赖 (triplet: $VCPKG_TRIPLET)..."
    cd "$PROJECT_DIR"
    "$VCPKG_DIR/vcpkg" install --triplet "$VCPKG_TRIPLET" 2>&1 | tail -5 || warn "部分依赖安装失败，CI 构建时会重试"
else
    warn "未找到 vcpkg.json，跳过依赖预安装"
fi

# =========================================================================
# Step 5: 安装 Ansible（可选，部署用）
# =========================================================================
step "安装 Ansible..."
install_ansible() {
    if command -v ansible-playbook &>/dev/null; then
        info "Ansible 已安装: $(ansible --version | head -1)"
        return
    fi
    apt-get install -y -qq ansible 2>/dev/null || \
        pip3 install ansible 2>/dev/null || \
        warn "Ansible 安装失败，可稍后手动安装"
}
install_ansible

# =========================================================================
# Step 6: 验证工具链
# =========================================================================
echo ""
echo "=============================================="
echo "  工具链验证"
echo "=============================================="
echo "  gcc:              $(gcc --version 2>/dev/null | head -1 || echo '未安装')"
echo "  g++:              $(g++ --version 2>/dev/null | head -1 || echo '未安装')"
echo "  cmake:            $(cmake --version 2>/dev/null | head -1 || echo '未安装')"
echo "  ctest:            $(ctest --version 2>/dev/null | head -1 || echo '未安装')"
echo "  ninja:            $(ninja --version 2>/dev/null || echo '未安装')"
echo "  git:              $(git --version 2>/dev/null || echo '未安装')"
echo "  ansible:          $(ansible --version 2>/dev/null | head -1 || echo '未安装')"
echo "  vcpkg:            $("$VCPKG_DIR/vcpkg" version 2>/dev/null | head -1 || echo '未安装')"
echo "  VCPKG_ROOT:       ${VCPKG_ROOT:-未设置}"
echo "=============================================="

info "Linux 构建环境初始化完成！"
echo ""
echo "下一步："
echo "  1. source /etc/profile.d/vcpkg.sh   # 加载 vcpkg 环境变量"
echo "  2. cd 到项目根目录"
echo "  3. cmake --preset linux-gcc-debug   # 配置"
echo "  4. cmake --build --preset linux-gcc-debug  # 构建"
echo "  5. ctest --preset linux-gcc-debug   # 测试"
