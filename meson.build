project(
  'minikernel',
  ['c', 'cpp', ],
  version : '0.1.0',
  default_options : [
    'warning_level=2',
    'cpp_std=c++20',
    'b_staticpic=false',
  ]
)

linker_script = join_paths(meson.project_source_root(), 'linker.ld')
grub_cfg = files('grub/grub.cfg')

kernel_sources = files(
  'src/boot/boot.S',
  'src/kernel/kernel.cpp',
  'src/kernel/vga.cpp',
)

kernel = executable(
  'kernel.elf',
  kernel_sources,
  pie : false,
  c_args : [
    '-ffreestanding',
    '-fno-stack-protector',
    '-fno-pie',
    '-mno-red-zone',
    '-mcmodel=kernel',
       '-m64',    
  ],
  cpp_args : [
    '-ffreestanding',
    '-fno-exceptions',
    '-fno-rtti',
    '-fno-stack-protector',
    '-fno-pie',
    '-mno-red-zone',
    '-mcmodel=kernel',
       '-m64',    
  ],
  link_args : [
    '-nostdlib',
    '-static',
    '-Wl,-T,' + linker_script,
    '-Wl,--build-id=none',
  ],
  link_depends : linker_script,
)

custom_target(
  'minikernel.iso',
  input : [kernel, grub_cfg],
  output : 'minikernel.iso',
  command : [
    find_program('bash'),
    join_paths(meson.project_source_root(), 'scripts', 'mkiso.sh'),
    '@OUTPUT@',
    '@INPUT0@',
    '@INPUT1@',
  ],
  build_by_default : true,
)
