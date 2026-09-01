# CONAN_PROFILE = clang
# CONAN_PROFILE = gcc14_c++23
CONAN_PROFILE = gcc15_c++26
# CONAN_PROFILE = gcc15_c++26_debug

.PHONY: build package clean check-format check-includes

build:
	conan build . -b=missing -pr=${CONAN_PROFILE}

package:
	conan create . -u -b=missing -pr=${CONAN_PROFILE}

doc:
	zensical serve

check-format:
	find include test -name "*.hpp" -o -name "*.cpp" | xargs clang-format --dry-run -Werror

# Public headers only: a test .cpp leaning on melon/all.hpp harms nobody, a
# header leaning on a consumer's include order does.
check-includes:
	python3 misc/tools/check_std_includes.py include
	
clean:
	@rm -rf CMakeUserPresets.json
	@rm -rf build
