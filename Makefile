# CONAN_PROFILE = clang
# CONAN_PROFILE = gcc14_c++23
CONAN_PROFILE = gcc15_c++26
# CONAN_PROFILE = gcc15_c++26_debug

.PHONY: build package clean

build:
	conan build . -b=missing -pr=${CONAN_PROFILE}

package:
	conan create . -u -b=missing -pr=${CONAN_PROFILE}

doc:
	zensical serve

check-format:
	find include test -name "*.hpp" -o -name "*.cpp" | xargs clang-format --dry-run -Werror
	
clean:
	@rm -rf CMakeUserPresets.json
	@rm -rf build
