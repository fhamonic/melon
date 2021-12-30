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
	cmake -DCMAKE_CXX_COMPILER=g++-10 -DCMAKE_BUILD_TYPE=Release -DWARNINGS=ON -DHARDCORE_WARNINGS=ON -DCOMPILE_FOR_NATIVE=ON -DCOMPILE_WITH_LTO=ON ..

clean:
	@rm -rf $(BUILD_DIR)

single-header: single-header/milppp_cbc.hpp single-header/milppp_grb.hpp

single-header/milppp_cbc.hpp:
	@python3 -m quom --include_directory include include/milppp_cbc.hpp milppp_cbc.hpp.tmp && \
	mkdir -p single-header && \
	echo "/*" > single-header/milppp_cbc.hpp && \
	cat LICENSE >> single-header/milppp_cbc.hpp && \
	echo "*/" >> single-header/milppp_cbc.hpp && \
	cat milppp_cbc.hpp.tmp >> single-header/milppp_cbc.hpp && \
	rm milppp_cbc.hpp.tmp

single-header/milppp_grb.hpp:
	@python3 -m quom --include_directory include include/milppp_grb.hpp milppp_grb.hpp.tmp && \
	mkdir -p single-header && \
	echo "/*" > single-header/milppp_grb.hpp && \
	cat LICENSE >> single-header/milppp_grb.hpp && \
	echo "*/" >> single-header/milppp_grb.hpp && \
	cat milppp_grb.hpp.tmp >> single-header/milppp_grb.hpp && \
	rm milppp_grb.hpp.tmp
