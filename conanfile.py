import os
from conan import ConanFile
from conan.tools.files import copy
from conan.tools.cmake import cmake_layout, CMake
from conan.tools.build import check_min_cppstd


class MelonConan(ConanFile):
    name = "melon"
    version = "1.0.0-alpha.1"

    license = "BSL-1.0"
    description = (
        "A modern and efficient graph library using C++20 ranges and concepts."
    )
    homepage = "https://github.com/fhamonic/melon"
    url = "https://github.com/fhamonic/melon.git"

    settings = "os", "arch", "compiler", "build_type"
    exports_sources = "include/*", "LICENSE", "test/*"
    no_copy_source = True
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("range-v3/0.12.0", transitive_headers=True)
        self.requires("fmt/[>=10.0.0]", transitive_headers=True)
        
        self.test_requires("gtest/[>=1.10.0 <cci]")
        # self.test_requires("mppp/1.0.3")

    def validate(self):
        check_min_cppstd(self, 20)

    def layout(self):
        cmake_layout(self)

    def build(self):
        if not self.conf.get("tools.build:skip_test", default=False):
            cmake = CMake(self)
            cmake.configure(build_script_folder="test")
            cmake.build()
            self.run(os.path.join(self.cpp.build.bindir, "melon_test"))

    def package(self):
        copy(
            self,
            "LICENSE",
            self.source_folder,
            os.path.join(self.package_folder, "licenses"),
        )
        copy(
            self,
            "*.hpp",
            os.path.join(self.source_folder, "include"),
            os.path.join(self.package_folder, "include"),
        )

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

    def package_id(self):
        self.info.clear()
