from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout

class MarketEngineConan(ConanFile):
    name = "MarketEngine"

    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("rapidcsv/8.84") # csv parser lib
        self.requires("gtest/1.17.0")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.27]")

    def layout(self):
        cmake_layout(self)
