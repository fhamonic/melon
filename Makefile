MAKEFLAGS += --no-print-directory

CPUS?=$(shell getconf _NPROCESSORS_ONLN || echo 1)

BUILD_DIR = build

.PHONY: all clean single-header doc

all: $(BUILD_DIR)
	@cd $(BUILD_DIR) && \
	cmake --build . --parallel $(CPUS)

$(BUILD_DIR):
	@mkdir $(BUILD_DIR) && \
	cd $(BUILD_DIR) && \
	conan install .. && \
	cmake -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_BUILD_TYPE=Debug -DWARNINGS=ON -DHARDCORE_WARNINGS=ON ..

test: all
	@cd $(BUILD_DIR) && \
	ctest --output-on-failure
	
clean:
	@rm -rf $(BUILD_DIR)

single-header: 
	@python3 -m quom --include_directory include include/melon/all.hpp melon.hpp.tmp && \
	mkdir -p single-header && \
	echo "/*" > single-header/melon.hpp && \
	cat LICENSE >> single-header/melon.hpp && \
	echo "*/" >> single-header/melon.hpp && \
	cat melon.hpp.tmp >> single-header/melon.hpp && \
	rm melon.hpp.tmp

cp-to-benchmark:
	cp ../melon/single-header/melon.hpp ../melon_benchmark/include/melon.hpp