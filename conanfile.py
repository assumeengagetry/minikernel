from conan import ConanFile


class MyOSConan(ConanFile):
    name = "myos"
    version = "0.1"
    settings = "os", "arch", "build_type"
    exports_sources = (
        "meson.build",
        "meson.cross",
        "linker.ld",
        "grub/*",
        "scripts/*",
        "src/*",
    )

    def build(self):
        self.run("meson setup build --cross-file meson.cross --wipe", cwd=self.source_folder)
        self.run("meson compile -C build", cwd=self.source_folder)
