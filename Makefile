# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++26 -g
DEPFLAGS := -MMD -MP

# Libraries to link
LDLIBS := -lreadline

# Directories
SRCDIR := src
BUILDDIR := build
BINDIR := bin
ASMDIR := asm

# Target executable
TARGET := $(BINDIR)/noeval

# Find all source files
SOURCES := $(wildcard $(SRCDIR)/*.cpp)

# Generate object file names in build directory
OBJECTS := $(SOURCES:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)

# Generate dependency file names
DEPS := $(OBJECTS:.o=.d)

# Generate assembly file names
ASMFILES := $(SOURCES:$(SRCDIR)/%.cpp=$(ASMDIR)/%.s)

# Default target
all: $(TARGET)

# Generate assembly files
assembly: $(ASMFILES)

# Generate assembly files from source
$(ASMDIR)/%.s: $(SRCDIR)/%.cpp | $(ASMDIR)
	$(CXX) $(CXXFLAGS) -S $< -o $@

# Link target executable
$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) $(OBJECTS) -o $@ $(LDLIBS)

# Compile source files to object files with dependency generation
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(DEPFLAGS) -c $< -o $@

# Create directories if they don't exist
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(ASMDIR):
	mkdir -p $(ASMDIR)

# Include dependency files (ignore if they don't exist)
-include $(DEPS)

# Clean build artifacts
clean:
	rm -rf $(BUILDDIR) $(BINDIR) $(ASMDIR)

# Phony targets
.PHONY: all clean assembly