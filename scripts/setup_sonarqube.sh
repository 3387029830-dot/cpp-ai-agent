#!/bin/bash
# ──────────────────────────────────────────────────
# SonarQube C++ 分析工具安装脚本
# 在 Linux VM 上运行: sudo bash scripts/setup_sonarqube.sh
# 免费方案: cppcheck + sonar-cxx 社区插件
# ──────────────────────────────────────────────────

set -e

TOOLS_DIR="/opt/sonar-tools"
echo "=== Installing SonarQube C++ analysis tools to ${TOOLS_DIR} ==="

mkdir -p "${TOOLS_DIR}"

# ── cppcheck (C++ 静态分析引擎) ──
if ! command -v cppcheck &> /dev/null; then
    echo "[1/3] Installing cppcheck..."
    apt-get install -y cppcheck
    echo "[OK] cppcheck installed"
else
    echo "[1/3] cppcheck already installed: $(cppcheck --version)"
fi

# ── SonarScanner CLI ──
SC_DIR="${TOOLS_DIR}/sonar-scanner"
if [ ! -f "${SC_DIR}/bin/sonar-scanner" ]; then
    echo "[2/3] Downloading sonar-scanner-cli..."
    curl -L -o /tmp/sc.zip "https://binaries.sonarsource.com/Distribution/sonar-scanner-cli/sonar-scanner-cli-6.2.1.4610-linux-x64.zip"
    unzip -o /tmp/sc.zip -d "${TOOLS_DIR}"
    rm -f /tmp/sc.zip
    EXTRACTED=$(ls -d "${TOOLS_DIR}"/sonar-scanner-* 2>/dev/null | head -1)
    if [ -n "${EXTRACTED}" ] && [ "${EXTRACTED}" != "${SC_DIR}" ]; then
        mv "${EXTRACTED}" "${SC_DIR}"
    fi
    echo "[OK] sonar-scanner installed"
else
    echo "[2/3] sonar-scanner already installed, skipping"
fi

# ── sonar-cxx 插件（需手动放入 SonarQube 容器） ──
echo "[3/3] sonar-cxx plugin"
echo "  NOTE: sonar-cxx plugin jar 需要从 GitHub Releases 手动下载"
echo "  https://github.com/SonarOpenCommunity/sonar-cxx/releases"
echo "  下载 sonar-cxx-plugin-x.x.x.jar + cxx-sslr-toolkit-x.x.x.jar"
echo "  然后执行:"
echo "    sudo docker cp sonar-cxx-plugin-*.jar sonarqube:/opt/sonarqube/extensions/plugins/"
echo "    sudo docker cp cxx-sslr-toolkit-*.jar sonarqube:/opt/sonarqube/extensions/plugins/"
echo "    sudo docker restart sonarqube"
echo ""

# ── 验证 ──
echo "=== Verification ==="
echo "cppcheck:"
cppcheck --version 2>&1 || true
echo ""
echo "sonar-scanner:"
"${SC_DIR}/bin/sonar-scanner" --version 2>&1 || true
echo ""
echo "=== Done ==="
echo ""
echo "Next steps:"
echo "  1. Open SonarQube UI (http://<VM_IP>:9000)"
echo "  2. Administration → CXX → File suffixes → .cpp,.h,.hpp,.cxx,.cc"
echo "  3. Set GitLab CI/CD variables:"
echo "     - SONAR_HOST_URL=http://<VM_IP>:9000"
echo "     - SONAR_TOKEN=<your_token>"
