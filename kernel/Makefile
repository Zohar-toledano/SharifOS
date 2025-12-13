# Detect host and architecture
DEFAULT_HOST != ../scripts/default-host.sh
HOST         ?= DEFAULT_HOST
HOSTARCH     != ../scripts/target-triplet-to-arch.sh $(HOST)

# Basic flags
CFLAGS   ?= -O2 -g -fstack-protector
CPPFLAGS ?=
LDFLAGS  ?=
LIBS     ?=

# Install paths
DESTDIR     ?=
PREFIX      ?= /usr/local
EXEC_PREFIX ?= $(PREFIX)
BOOTDIR     ?= $(EXEC_PREFIX)/boot
INCLUDEDIR  ?= $(PREFIX)/include

# Core compiler flags
CFLAGS   += -ffreestanding -Wall -Wextra
CPPFLAGS += -D__is_kernel -Iinclude
LIBS     += -nostdlib -lk -lgcc

# Architecture-specific config
ARCHDIR := arch/$(HOSTARCH)
include $(ARCHDIR)/make.config

CFLAGS   += $(KERNEL_ARCH_CFLAGS)
CPPFLAGS += $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS  += $(KERNEL_ARCH_LDFLAGS)
LIBS     += $(KERNEL_ARCH_LIBS)

# Build directories
KBUILDDIR := $(BUILDDIR)/kernel

# Source discovery
KERNEL_SOURCES      := $(shell find ./kernel -type f \( -name '*.cpp' -o -name '*.c' \))
KERNEL_ARCH_SOURCES := $(shell find ./$(ARCHDIR) -type f \( -name '*.cpp' -o -name '*.c' -o -name '*.s' \))

KERNEL_OBJS := \
	$(addprefix $(KBUILDDIR)/, $(KERNEL_ARCH_OBJS)) \
	$(patsubst ./%, $(KBUILDDIR)/%.o, $(basename $(KERNEL_SOURCES)))

OBJS := \
	$(KBUILDDIR)/$(ARCHDIR)/crti.o \
	$(KBUILDDIR)/$(ARCHDIR)/crtbegin.o \
	$(KERNEL_OBJS) \
	$(KBUILDDIR)/$(ARCHDIR)/crtend.o \
	$(KBUILDDIR)/$(ARCHDIR)/crtn.o

LINK_LIST := \
	$(LDFLAGS) \
	$(KBUILDDIR)/$(ARCHDIR)/crti.o \
	$(KBUILDDIR)/$(ARCHDIR)/crtbegin.o \
	$(KERNEL_OBJS) \
	$(LIBS) \
	$(KBUILDDIR)/$(ARCHDIR)/crtend.o \
	$(KBUILDDIR)/$(ARCHDIR)/crtn.o


$(info LINK_LIST: $(LINK_LIST))
.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .cpp .s .c

all: $(KBUILDDIR)/sharifos.kernel

$(KBUILDDIR)/sharifos.kernel: $(OBJS) $(ARCHDIR)/linker.ld
	mkdir -p $(KBUILDDIR)
	$(CXX) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)

# Compilation rules
$(KBUILDDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -MD -std=gnu11 -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(KBUILDDIR)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(KBUILDDIR)/%.o: %.s
	mkdir -p $(dir $@)
	nasm -f elf32 $< -o $@

# GCC runtime objects
$(KBUILDDIR)/$(ARCHDIR)/crtbegin.o $(KBUILDDIR)/$(ARCHDIR)/crtend.o:
	mkdir -p $(dir $@)
	OBJ=`$(CXX) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp $$OBJ $@

# Clean build
clean:
	rm -f $(KBUILDDIR)/sharifos.kernel
	rm -f $(OBJS)
	rm -f $(OBJS:.o=.d)

# Installation
install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.
	mkdir -p $(DESTDIR)$(INCLUDEDIR)/arch
	cp -R --preserve=timestamps arch/$(HOSTARCH)/*.h $(DESTDIR)$(INCLUDEDIR)/arch/.

install-kernel: $(KBUILDDIR)/sharifos.kernel
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp $< $(DESTDIR)$(BOOTDIR)

# Include auto-generated dependencies
-include $(OBJS:.o=.d)
