BUILD_DIR = build

TESTS = test_simple_gc test_visualizer test_stack_scan test_memory_pools test_compaction test_memory_pressure test_gc_pool test_gc_large test_gc_mark

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

example-visualizer: build
	@./$(BUILD_DIR)/examples/visualizer_demo

example-stack-scan: build
	@./$(BUILD_DIR)/examples/stack_scan_demo

clean:
	@rm -rf $(BUILD_DIR)
