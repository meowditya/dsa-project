# Makefile — Project 9 (Team O(n^2)). Owner: Architect.
#
#   make            build everything that exists so far (release flags)
#   make test       build + run every test
#   make sanitize   same tests, separate build with AddressSanitizer + UBSan
#   make all        build, test (later also demo and bench), strictly one after another
#   make clean
#
# Each build flavour has its own directory, so release, counting and sanitizer objects never mix.
# Later PRs add their programs to PROGRAMS / TESTS below; nothing else needs to change.

CXX       ?= g++
STD       := -std=c++17
WARN      := -Wall -Wextra -pedantic -Werror
INCLUDES  := -Iinclude
DEPFLAGS  := -MMD -MP

SAN       := -fsanitize=address,undefined
BUILD     ?= release
ifeq ($(BUILD),release)
  OPT := -O2 -DNDEBUG
else ifeq ($(BUILD),count)
  OPT := -O2 -DNDEBUG -DEQ_COUNTING=1
else ifeq ($(BUILD),sanitize)
  OPT := -O1 -g -fno-omit-frame-pointer $(SAN)
else
  $(error BUILD must be release, count or sanitize)
endif

OUT       := build/$(BUILD)
CXXFLAGS  := $(STD) $(WARN) $(OPT) $(INCLUDES) $(DEPFLAGS)
LDFLAGS   := $(if $(filter sanitize,$(BUILD)),$(SAN),)

# ---- programs: name -> source files (added PR by PR) ---------------------------------------
TESTS     := interface_contract_test
interface_contract_test_SRC := tests/interface_contract_test.cpp

PROGRAMS  := $(TESTS)
BINS      := $(addprefix $(OUT)/bin/,$(PROGRAMS))

.PHONY: build test sanitize all clean
build: $(BINS)

# one object per source file, dependency files generated alongside (-MMD -MP)
$(OUT)/obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

define LINK_RULE
$(OUT)/bin/$(1): $$(patsubst %.cpp,$(OUT)/obj/%.o,$$($(1)_SRC))
	@mkdir -p $$(dir $$@)
	$$(CXX) $$^ $$(LDFLAGS) -o $$@
endef
$(foreach p,$(PROGRAMS),$(eval $(call LINK_RULE,$(p))))

test: build
	@set -e; for t in $(TESTS); do echo "== $$t"; ./$(OUT)/bin/$$t; done

sanitize:
	$(MAKE) BUILD=sanitize test

# Sequential on purpose: tests, the demo and benchmarks must never run at the same time,
# even under `make -j`. (demo and bench are added by later PRs.)
all:
	$(MAKE) build
	$(MAKE) test

clean:
	rm -rf build

-include $(shell find build -name '*.d' 2>/dev/null)
