from conan import ConanFile
from conan.tools.cmake import CMake
from conan.tools.build import check_min_cppstd

class CompressorRecipe(ConanFile):
    name="melon"
    version="0.5"
    license = "BSL-1.0"
    description="A modern and efficient graph library using C++20 ranges and concepts."
    homepage = "https://github.com/fhamonic/melon.git"
    #url = ""
    package_type = "header-library"
    generators = "CMakeToolchain", "CMakeDeps"
    build_policy = "missing"
    
    exports_sources = "include/*"
    no_copy_source = True

    def requirements(self):
        self.requires("range-v3/0.12.0")
        self.requires("fmt/10.1.1")
        self.requires("gtest/1.14.0")

    def build_requirements(self):
        self.tool_requires("cmake/3.27.1")
        
    def build(self):
        check_min_cppstd(self, 20, True)
        cmake = CMake(self)
        cmake.configure({"BUILD_TESTS":"ON"})
        cmake.build()
