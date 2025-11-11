BUILD_DIR = build

TESTS = test_simple_gc test_visualizer test_stack_scan test_memory_pools test_compaction test_memory_pressure test_gc_large test_gc_mark test_gc_sweep test_trace test_debug test_generational test_cardtable test_barrier test_gen_integration

.PHONY: all build test test-verbose example clean

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && $(MAKE)

define run_ctest
	@cd $(BUILD_DIR) && ctest -R $(subst -,_,$(1)) $(2)
endef

test-%: build
	$(call run_ctest,$*,--output-on-failure)

test-%-verbose: build
	$(call run_ctest,$*,-V)

test: build $(addprefix test-,$(subst test_,,$(TESTS)))

test-verbose: build $(addprefix test-,$(addsuffix -verbose,$(subst test_,,$(TESTS))))

clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf test/CMakeFiles test/cmake_install.cmake test/CTestTestfile.cmake test/Makefile
