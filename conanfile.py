from conan import ConanFile
from conan.tools.files import copy


class JesonConan(ConanFile):
    name = "jeson"
    version = "0.1.0"
    description = "Lightweight, typed JSON for embedded and desktop C++20"
    license = "MIT"
    homepage = "https://github.com/mijelsma/jeson"
    url = "https://github.com/mijelsma/jeson"
    package_type = "header-library"
    settings = "os", "arch", "compiler", "build_type"
    exports_sources = "include/*", "LICENSE"
    no_copy_source = True

    def package(self):
        copy(self, "*.hpp", src=self.source_folder + "/include", dst=self.package_folder + "/include")
        copy(self, "LICENSE", src=self.source_folder, dst=self.package_folder + "/licenses")

    def package_id(self):
        self.info.clear()

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "jeson")
        self.cpp_info.set_property("cmake_target_name", "jeson::jeson")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

