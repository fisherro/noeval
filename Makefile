# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++26 -g
DEPFLAGS := -MMD -MP

# Libraries to link
LDLIBS := -lreadline -lstdc++exp

# Directories
SRCDIR := src
BUILDDIR := build
BINDIR := bin
ASMDIR := asm

# Build variants. Each one has its own object directory and executable, so
# switching between them doesn't need a `make clean`. The default is release.
#
#   release   optimized (the one to use day to day)
#   debug     unoptimized, for stepping through in a debugger
#   sanitize  AddressSanitizer and UndefinedBehaviorSanitizer
#
# The per-variant flags are added to CXXFLAGS rather than replacing it, so
# `make CXXFLAGS=...` still works.
VARIANTS := release debug sanitize

release_TARGET := $(BINDIR)/noeval
release_CXXFLAGS := -O2
release_LDFLAGS :=

debug_TARGET := $(BINDIR)/noeval-debug
debug_CXXFLAGS := -O0
debug_LDFLAGS :=

SANITIZERS := -fsanitize=address,undefined -fno-sanitize-recover=undefined
sanitize_TARGET := $(BINDIR)/noeval-sanitize
sanitize_CXXFLAGS := -O1 -fno-omit-frame-pointer $(SANITIZERS)
sanitize_LDFLAGS := $(SANITIZERS)

# Find all source files
SOURCES := $(wildcard $(SRCDIR)/*.cpp)

# Generate assembly file names
ASMFILES := $(SOURCES:$(SRCDIR)/%.cpp=$(ASMDIR)/%.s)

# Default target
all: release

# Rules for one build variant: $(1) is its name.
define VARIANT_RULES
$(1)_OBJECTS := $$(SOURCES:$$(SRCDIR)/%.cpp=$$(BUILDDIR)/$(1)/%.o)

$(1): $$($(1)_TARGET)

# Link the variant's executable
$$($(1)_TARGET): $$($(1)_OBJECTS) | $$(BINDIR)
	$$(CXX) $$($(1)_LDFLAGS) $$($(1)_OBJECTS) -o $$@ $$(LDLIBS)

# Compile source files to object files with dependency generation
$$(BUILDDIR)/$(1)/%.o: $$(SRCDIR)/%.cpp | $$(BUILDDIR)/$(1)
	$$(CXX) $$(CXXFLAGS) $$($(1)_CXXFLAGS) $$(DEPFLAGS) -c $$< -o $$@

$$(BUILDDIR)/$(1):
	mkdir -p $$@

# Include dependency files (ignore if they don't exist)
-include $$($(1)_OBJECTS:.o=.d)
endef

$(foreach variant,$(VARIANTS),$(eval $(call VARIANT_RULES,$(variant))))

# Run the C++ tests and the library tests, then the GC tests with the
# collector running at every environment creation.
test: release
	$(release_TARGET) --tests
	NOEVAL_GC_STRESS=1 $(release_TARGET) --gc-tests

# The same tests under the sanitizers, with GC stress for all of them. The
# cycle collector clears the bindings of environments it considers garbage,
# so collecting often makes a use-after-free of one likely to show up.
# Collecting at every environment creation makes the library tests take
# more than nine minutes, so they collect at every thousandth, which takes
# about a minute.
test-sanitize: sanitize
	NOEVAL_GC_STRESS=1000 $(sanitize_TARGET) --tests
	NOEVAL_GC_STRESS=1 $(sanitize_TARGET) --gc-tests

# Generate assembly files
assembly: $(ASMFILES)

# Generate assembly files from source
$(ASMDIR)/%.s: $(SRCDIR)/%.cpp | $(ASMDIR)
	$(CXX) $(CXXFLAGS) $(release_CXXFLAGS) -S $< -o $@

# Create directories if they don't exist
$(BINDIR):
	mkdir -p $(BINDIR)

$(ASMDIR):
	mkdir -p $(ASMDIR)

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR) $(BINDIR) $(ASMDIR)

# Phony targets
.PHONY: all $(VARIANTS) test test-sanitize clean assembly
