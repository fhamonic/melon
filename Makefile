MAKEFLAGS += --no-print-directory

CPUS?=$(shell getconf _NPROCESSORS_ONLN || echo 1)

CC = g++
BUILD_DIR = build

.PHONY: all test clean single-header

all: $(BUILD_DIR)
	@cd $(BUILD_DIR) && \
	cmake --build . --parallel $(CPUS)

$(BUILD_DIR):
	@mkdir $(BUILD_DIR) && \
	cd $(BUILD_DIR) && \
	cmake -DCMAKE_CXX_COMPILER=$(CC) -DCMAKE_BUILD_TYPE=Debug ..

test: all
	@cd $(BUILD_DIR) && \
	ctest --output-on-failure
	
clean:
	@rm -rf $(BUILD_DIR)

single-header: 
	@python3 -m quom --include_directory include include/melon/all.hpp single-header/melon.hpp.tmp && \
	mkdir -p single-header && \
	echo "/*" > single-header/melon.hpp && \
	cat LICENSE >> single-header/melon.hpp && \
	echo "*/" >> single-header/melon.hpp && \
	cat single-header/melon.hpp.tmp >> single-header/melon.hpp && \
	rm single-header/melon.hpp.tmp