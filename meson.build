project(
    'microkernel',
    ['c', 'cpp'],
    version : '0.1.0',
    license : 'MIT',
    default_options : [
        'c_std=gnu99',
        'cpp_std=gnu++17',
        'warning_level=2',
        'werror=false',
        'optimization=2',
        'debug=true',
        'b_pie=false',
        'b_staticpic=false',
    ],
)

qemu = find_program('qemu-system-x86_64', required : true)
sh = find_program('sh', required : true)

kernel_common_args = [
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
]

kernel_cpp_args = kernel_common_args + [
    '-fno-exceptions',
    '-fno-rtti',
    '-fno-threadsafe-statics',
    '-fno-use-cxa-atexit',
    '-nostdinc++',
    '-DMICROKERNEL_VERSION="' + meson.project_version() + '"',
]

kernel_link_args = [
    '-nostdlib',
    '-static',
    '-Wl,--build-id=none',
    '-Wl,-z,max-page-size=0x1000',
    '-Wl,-z,common-page-size=0x1000',
]

kernel_sources = files(
    'src/kernel/main.cpp',
    'src/kernel/shell.cpp',
    'arch/x86_64/boot/boot.S',
)

kernel_linker_script = meson.current_source_dir() / 'arch/x86_64/boot/kernel.ld'

kernel_elf = executable(
    'kernel.elf',
    kernel_sources,
    include_directories : include_directories('kernel/include'),
    c_args : kernel_common_args,
    cpp_args : kernel_cpp_args,
    link_args : kernel_link_args + ['-T', kernel_linker_script],
    link_depends : kernel_linker_script,
    pie : false,
)

kernel_iso = custom_target(
    'kernel.iso',
    input : kernel_elf,
    output : 'kernel.iso',
    command : [
        sh,
        meson.current_source_dir() / 'scripts' / 'make_iso.sh',
        '@INPUT@',
        '@OUTPUT@',
    ],
    build_by_default : false,
)

run_target(
    'qemu',
    command : [
        qemu,
        '-cdrom', kernel_iso,
        '-m', '512M',
        '-display', 'none',
        '-serial', 'stdio',
        '-monitor', 'none',
        '-no-reboot',
        '-no-shutdown',
    ],
    depends : kernel_iso,
)

run_target(
    'debug',
    command : [
        qemu,
        '-cdrom', kernel_iso,
        '-m', '512M',
        '-display', 'none',
        '-serial', 'stdio',
        '-monitor', 'none',
        '-s',
        '-S',
        '-no-reboot',
        '-no-shutdown',
    ],
    depends : kernel_iso,
)
