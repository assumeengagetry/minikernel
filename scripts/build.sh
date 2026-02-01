#!/bin/bash
#
# MicroKernel Build Script
# 使用 Meson 和 Conan 构建内核
#

set -e

# =============================================================================
# 配置变量
# =============================================================================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
CROSS_FILE="${PROJECT_ROOT}/cross/x86_64-none.ini"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# =============================================================================
# 辅助函数
# =============================================================================
info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# =============================================================================
# 检查依赖
# =============================================================================
check_dependencies() {
    info "检查构建依赖..."

    local missing=()

    # 检查必需工具
    command -v meson >/dev/null 2>&1 || missing+=("meson")
    command -v ninja >/dev/null 2>&1 || missing+=("ninja")
    command -v gcc >/dev/null 2>&1 || missing+=("gcc")
    command -v nasm >/dev/null 2>&1 || missing+=("nasm")
    command -v objcopy >/dev/null 2>&1 || missing+=("objcopy (binutils)")
    command -v objdump >/dev/null 2>&1 || missing+=("objdump (binutils)")

    # 检查可选工具
    if ! command -v conan >/dev/null 2>&1; then
        warn "conan 未安装，将使用直接 Meson 构建"
    fi

    if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
        warn "qemu-system-x86_64 未安装，无法运行内核"
    fi

    if [ ${#missing[@]} -ne 0 ]; then
        error "缺少以下依赖: ${missing[*]}"
    fi

    success "所有必需依赖已安装"
}

# =============================================================================
# 使用 Conan 安装依赖
# =============================================================================
conan_install() {
    info "使用 Conan 安装依赖..."

    cd "$PROJECT_ROOT"

    if command -v conan >/dev/null 2>&1; then
        conan install . --output-folder="$BUILD_DIR" --build=missing
        success "Conan 依赖安装完成"
    else
        warn "跳过 Conan 依赖安装 (conan 未安装)"
    fi
}

# =============================================================================
# 配置 Meson 构建
# =============================================================================
meson_setup() {
    info "配置 Meson 构建系统..."

    cd "$PROJECT_ROOT"

    # 如果构建目录已存在，先清理
    if [ -d "$BUILD_DIR" ] && [ "$1" == "--reconfigure" ]; then
        info "重新配置构建目录..."
        meson setup "$BUILD_DIR" --reconfigure --cross-file="$CROSS_FILE"
    elif [ ! -d "$BUILD_DIR" ]; then
        meson setup "$BUILD_DIR" --cross-file="$CROSS_FILE"
    else
        info "构建目录已存在，使用现有配置"
    fi

    success "Meson 配置完成"
}

# =============================================================================
# 构建内核
# =============================================================================
build_kernel() {
    info "开始构建内核..."

    cd "$BUILD_DIR"
    meson compile

    success "内核构建完成"
    info "输出文件:"
    echo "  - ${BUILD_DIR}/kernel.elf"
    echo "  - ${BUILD_DIR}/kernel.bin"
}

# =============================================================================
# 清理构建
# =============================================================================
clean() {
    info "清理构建目录..."

    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
        success "构建目录已清理"
    else
        info "构建目录不存在，无需清理"
    fi
}

# =============================================================================
# 运行 QEMU
# =============================================================================
run_qemu() {
    info "在 QEMU 中运行内核..."

    if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
        error "qemu-system-x86_64 未安装"
    fi

    local kernel_bin="${BUILD_DIR}/kernel.bin"
    if [ ! -f "$kernel_bin" ]; then
        error "内核二进制文件不存在: $kernel_bin (请先构建)"
    fi

    qemu-system-x86_64 \
        -kernel "$kernel_bin" \
        -m 512M \
        -serial stdio \
        -no-reboot \
        -no-shutdown
}

# =============================================================================
# 调试模式运行 QEMU
# =============================================================================
run_debug() {
    info "在调试模式下运行 QEMU (等待 GDB 连接)..."

    if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
        error "qemu-system-x86_64 未安装"
    fi

    local kernel_elf="${BUILD_DIR}/kernel.elf"
    if [ ! -f "$kernel_elf" ]; then
        error "内核 ELF 文件不存在: $kernel_elf (请先构建)"
    fi

    info "GDB 连接命令: gdb -ex 'target remote localhost:1234' ${kernel_elf}"

    qemu-system-x86_64 \
        -kernel "$kernel_elf" \
        -m 512M \
        -serial stdio \
        -s -S \
        -no-reboot \
        -no-shutdown
}

# =============================================================================
# 生成反汇编
# =============================================================================
disassemble() {
    info "生成反汇编输出..."

    local kernel_elf="${BUILD_DIR}/kernel.elf"
    local output="${BUILD_DIR}/kernel.dis"

    if [ ! -f "$kernel_elf" ]; then
        error "内核 ELF 文件不存在: $kernel_elf (请先构建)"
    fi

    objdump -d "$kernel_elf" > "$output"
    success "反汇编已保存到: $output"
}

# =============================================================================
# 显示帮助
# =============================================================================
show_help() {
    echo "MicroKernel 构建脚本"
    echo ""
    echo "用法: $0 [命令]"
    echo ""
    echo "命令:"
    echo "  all          完整构建 (检查依赖 + 配置 + 构建)"
    echo "  check        检查构建依赖"
    echo "  conan        使用 Conan 安装依赖"
    echo "  setup        配置 Meson 构建"
    echo "  build        构建内核"
    echo "  clean        清理构建目录"
    echo "  rebuild      清理并重新构建"
    echo "  qemu         在 QEMU 中运行内核"
    echo "  debug        调试模式运行 (等待 GDB)"
    echo "  disasm       生成反汇编输出"
    echo "  help         显示此帮助信息"
    echo ""
    echo "示例:"
    echo "  $0 all       # 完整构建"
    echo "  $0 rebuild   # 重新构建"
    echo "  $0 qemu      # 运行内核"
}

# =============================================================================
# 主入口
# =============================================================================
main() {
    case "${1:-all}" in
        all)
            check_dependencies
            conan_install
            meson_setup
            build_kernel
            ;;
        check)
            check_dependencies
            ;;
        conan)
            conan_install
            ;;
        setup)
            meson_setup "$2"
            ;;
        build)
            build_kernel
            ;;
        clean)
            clean
            ;;
        rebuild)
            clean
            check_dependencies
            meson_setup
            build_kernel
            ;;
        qemu|run)
            run_qemu
            ;;
        debug)
            run_debug
            ;;
        disasm)
            disassemble
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            error "未知命令: $1 (使用 'help' 查看可用命令)"
            ;;
    esac
}

main "$@"
