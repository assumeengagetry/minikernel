from conan import ConanFile
from conan.tools.meson import Meson, MesonToolchain
from conan.tools.files import copy
import os


class MicroKernelConan(ConanFile):
    """
    MicroKernel - Conan 包管理配置
    用于管理内核构建依赖和工具链配置
    """

    name = "microkernel"
    version = "0.1.0"
    license = "MIT"
    author = "MicroKernel Team"
    url = "https://github.com/microkernel/microkernel"
    description = "A minimal x86_64 microkernel implementation"
    topics = ("kernel", "os", "x86_64", "microkernel")

    # 构建配置
    settings = "os", "compiler", "build_type", "arch"

    # 构建选项
    options = {
        "arch": ["x86_64"],
        "kernel_debug": [True, False],
        "serial_debug": [True, False],
        "enable_tests": [True, False],
        "enable_userspace": [True, False],
        "max_cpus": ["ANY"],
    }

    default_options = {
        "arch": "x86_64",
        "kernel_debug": True,
        "serial_debug": True,
        "enable_tests": False,
        "enable_userspace": False,
        "max_cpus": "8",
    }

    # 导出源文件
    exports_sources = (
        "meson.build",
        "meson_options.txt",
        "src/*",
        "kernel/*",
        "arch/*",
        "include/*",
        "user/*",
        "tests/*",
        "scripts/*",
    )

    # 生成器
    generators = "MesonToolchain"

    def requirements(self):
        """
        定义项目依赖
        注意: 内核是 freestanding 环境，不能使用标准库
        这里主要定义开发和测试依赖
        """
        # 如果启用测试，添加测试框架（用于用户空间测试）
        if self.options.enable_tests:
            # 内核单元测试可能需要特殊的测试框架
            pass

    def build_requirements(self):
        """
        构建时依赖
        """
        # Meson 构建系统
        self.tool_requires("meson/1.3.0")
        # Ninja 构建后端
        self.tool_requires("ninja/1.11.1")

    def configure(self):
        """
        配置构建选项
        """
        # 内核使用纯 C，删除 C++ 相关设置
        del self.settings.compiler.libcxx
        del self.settings.compiler.cppstd

    def validate(self):
        """
        验证构建配置
        """
        # 目前仅支持 x86_64 架构
        if self.options.arch != "x86_64":
            raise ConanInvalidConfiguration(
                f"Architecture {self.options.arch} is not supported yet"
            )

    def generate(self):
        """
        生成构建文件
        """
        # 生成 Meson 工具链文件
        tc = MesonToolchain(self)

        # 传递选项给 Meson
        tc.project_options["arch"] = str(self.options.arch)
        tc.project_options["kernel_debug"] = self.options.kernel_debug
        tc.project_options["serial_debug"] = self.options.serial_debug
        tc.project_options["enable_tests"] = self.options.enable_tests
        tc.project_options["enable_userspace"] = self.options.enable_userspace
        tc.project_options["max_cpus"] = str(self.options.max_cpus)

        tc.generate()

        # 生成跨编译配置文件（用于 bare-metal 目标）
        self._generate_cross_file()

    def _generate_cross_file(self):
        """
        生成 Meson 跨编译配置文件
        """
        cross_file_content = f"""
# MicroKernel Cross-Compilation Configuration
# 自动生成 - 请勿手动修改

[binaries]
c = 'gcc'
ar = 'ar'
strip = 'strip'
ld = 'ld'

[built-in options]
c_args = [
    '-ffreestanding',
    '-nostdlib',
    '-nostdinc',
    '-fno-builtin',
    '-fno-stack-protector',
    '-mno-red-zone',
    '-mno-mmx',
    '-mno-sse',
    '-mno-sse2',
    '-m64',
    '-mcmodel=kernel',
    '-fno-pic'
]

c_link_args = [
    '-nostdlib',
    '-static',
    '-Wl,--build-id=none'
]

[host_machine]
system = 'none'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
"""
        cross_file_path = os.path.join(self.generators_folder, "cross_x86_64.ini")
        with open(cross_file_path, "w") as f:
            f.write(cross_file_content)

    def build(self):
        """
        构建项目
        """
        meson = Meson(self)
        meson.configure()
        meson.build()

    def package(self):
        """
        打包构建产物
        """
        # 复制许可证
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

        # 复制内核二进制文件
        copy(self, "kernel.elf", src=self.build_folder, dst=os.path.join(self.package_folder, "boot"))
        copy(self, "kernel.bin", src=self.build_folder, dst=os.path.join(self.package_folder, "boot"))

        # 复制头文件（供用户空间程序使用）
        copy(self, "*.h", src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"))
        copy(self, "*.h", src=os.path.join(self.source_folder, "kernel", "include"),
             dst=os.path.join(self.package_folder, "include", "kernel"))

    def package_info(self):
        """
        包信息
        """
        self.cpp_info.bindirs = ["boot"]
        self.cpp_info.includedirs = ["include"]
