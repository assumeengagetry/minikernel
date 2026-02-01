# MicroKernel - Meson Build System
# 使用 Meson 和 Conan 重构的内核构建系统

project('microkernel', 'c',
    version : '0.1.0',
    license : 'MIT',
    default_options : [
        'c_std=gnu99',
        'warning_level=2',
        'werror=false',
        'optimization=2',
        'debug=true',
        'b_pie=false',
        'b_staticpic=false',
    ]
)

# =============================================================================
# 架构配置
# =============================================================================
arch = get_option('arch')
kernel_name = 'MicroKernel'

message('Building for architecture: ' + arch)

# =============================================================================
# 工具链配置
# =============================================================================
cc = meson.get_compiler('c')
objcopy = find_program('objcopy', required : true)
objdump = find_program('objdump', required : true)
qemu = find_program('qemu-system-x86_64', required : false)
grub_mkrescue = find_program('grub-mkrescue', required : false)

# =============================================================================
# 内核编译标志 (freestanding)
# =============================================================================
kernel_c_args = [
    '-ffreestanding',
    '-nostdlib',
    '-nostdinc',
    '-fno-builtin',
    '-fno-stack-protector',
    '-mno-red-zone',
    '-mno-mmx',
    '-mno-sse',
    '-mno-sse2',
    '-mno-sse3',
    '-mno-3dnow',
    '-m64',
    '-mcmodel=kernel',
    '-fno-pic',
    '-fno-pie',
    '-Wno-pedantic',
    '-Wno-unused-parameter',
    '-Wno-unused-variable',
    '-Wno-missing-prototypes',
    '-Wno-implicit-function-declaration',
]

kernel_link_args = [
    '-nostdlib',
    '-static',
    '-Wl,--build-id=none',
    '-Wl,-z,max-page-size=0x1000',
    '-Wl,-z,common-page-size=0x1000',
]

# =============================================================================
# 头文件路径
# =============================================================================
kernel_inc = include_directories(
    'include',
    'kernel/include',
)

# =============================================================================
# 源文件定义
# =============================================================================

# 内核核心源文件 (C)
# Note: sched.c and sched_fair.c are excluded for now due to incomplete dependencies
kernel_c_sources = files(
    'src/kernel/main.c',
    'src/kernel/shell.c',
    'kernel/mm/buddy.c',
)

# 架构相关的汇编文件 (GAS 语法，使用 GCC 编译)
kernel_asm_sources = files(
    'arch/x86_64/boot/boot.S',
    'arch/x86_64/cpu/switch.S',
    'kernel/interrupt/interrupt.S',
)

# 链接脚本
kernel_ld_script = meson.current_source_dir() / 'arch' / arch / 'boot' / 'kernel.ld'

# =============================================================================
# 内核可执行文件
# =============================================================================
kernel_elf = executable('kernel.elf',
    kernel_c_sources,
    kernel_asm_sources,
    include_directories : kernel_inc,
    c_args : kernel_c_args,
    link_args : kernel_link_args + ['-T', kernel_ld_script],
    link_depends : kernel_ld_script,
    pie : false,
    install : true,
    install_dir : 'boot',
)

# =============================================================================
# 生成内核二进制文件
# =============================================================================
kernel_bin = custom_target('kernel.bin',
    input : kernel_elf,
    output : 'kernel.bin',
    command : [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
    build_by_default : true,
    install : true,
    install_dir : 'boot',
)

# =============================================================================
# 反汇编输出 (可选)
# =============================================================================
kernel_disasm = custom_target('kernel.dis',
    input : kernel_elf,
    output : 'kernel.dis',
    command : [objdump, '-d', '@INPUT@'],
    capture : true,
    build_by_default : false,
)

kernel_symbols = custom_target('kernel.sym',
    input : kernel_elf,
    output : 'kernel.sym',
    command : [objdump, '-t', '@INPUT@'],
    capture : true,
    build_by_default : false,
)

# =============================================================================
# QEMU 运行目标
# =============================================================================
if qemu.found()
    run_target('qemu',
        command : [qemu, '-kernel', kernel_bin, '-m', '512M', '-serial', 'stdio'],
        depends : kernel_bin,
    )

    run_target('debug',
        command : [qemu, '-kernel', kernel_elf, '-m', '512M', '-serial', 'stdio', '-s', '-S'],
        depends : kernel_elf,
    )
endif

# =============================================================================
# ISO 生成 (可选)
# =============================================================================
if grub_mkrescue.found()
    # ISO 生成需要额外的脚本支持
    message('grub-mkrescue found, ISO generation available')
endif

# =============================================================================
# 子目录 (用户空间程序)
# =============================================================================
# subdir('user')
# subdir('tests')

# =============================================================================
# 摘要信息
# =============================================================================
summary({
    'Architecture': arch,
    'Kernel Name': kernel_name,
    'Version': meson.project_version(),
    'C Compiler': cc.get_id() + ' ' + cc.version(),
    'QEMU': qemu.found() ? qemu.full_path() : 'Not found',
}, section : 'Build Configuration')
