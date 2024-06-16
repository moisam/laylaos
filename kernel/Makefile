#HOST?=$(shell ../default-host.sh)
#HOSTARCH:=$(shell ../target-triplet-to-arch.sh $(HOST))
HOSTARCH:=$(if $(HOST) == "x86_64",x86_64,i386)

#KERNEL_CFLAGS?=-O3 -g -Wall -Wextra -pedantic
#KERNEL_CPPFLAGS?=
#LDFLAGS?=
#LIBS?=

#DESTDIR?=

PREFIX?=/usr
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include

CFLAGS:=$(KERNEL_CFLAGS) -fno-omit-frame-pointer -ffreestanding -fbuiltin -Iinclude -DKERNEL -DCOMPILE_YEAR=$(shell date +%Y)
CPPFLAGS:=$(KERNEL_CPPFLAGS)

LDFLAGS:=$(LDFLAGS)
LIBS:=$(LIBS) -nostdlib -lk -lgcc

ARCHDIR:=arch/$(HOSTARCH)

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)


# extra setup for the x86-64 kernel
LIBS:=$(LIBS) bin/desktop/libaelf$(if $(HOST) == "x86_64",64,32).a
CFLAGS:=$(CFLAGS) $(if $(HOST) == "x86_64",-z max-page-size=0x1000 -mcmodel=large -mno-red-zone,)


KERNEL_OBJS:=\
$(KERNEL_ARCH_OBJS) \
$(patsubst %.c, %.o, $(shell find kernel/ mm/ drivers/ -name '*.c')) \
$(patsubst %.c, %.o, $(shell find dev/ fs/ syscall/ network/ gui/ -name '*.c')) \
$(patsubst %.c, %.o, $(shell find ../misc/ modules/ -name '*.c')) \
$(patsubst %.S, %.o, $(shell find kernel/ mm/ syscall/ -name '*.S')) \
#$(patsubst %.c, %.o, ../ld/fnv1a.c)

OBJS:=\
$(ARCHDIR)/crti.o \
$(ARCHDIR)/crtbegin.o \
$(KERNEL_OBJS) \
$(ARCHDIR)/crtend.o \
$(ARCHDIR)/crtn.o

LINK_LIST:=\
$(LDFLAGS) \
$(ARCHDIR)/crti.o \
$(ARCHDIR)/crtbegin.o \
$(KERNEL_OBJS) \
$(LIBS) \
$(ARCHDIR)/crtend.o \
$(ARCHDIR)/crtn.o \

all: laylaos.kernel System.map compile-modules

.PHONY: all clean install install-headers install-kernel install-modules

# First, create the kernel image.
# Next, pass the kernel image to 'nm' to extract the symbol table, which
# we pass to grep to filter the table, removing entries starting with __,
# and those starting with _ and ending in _r (e.g. _strerror_r).

# NOTE:
# gcc keeps compiling against libc.so, even if we pass the -static option,
# so we pass libc.a verbatim to the linker (this is a hack, but it works).

laylaos.kernel: $(OBJS) $(ARCHDIR)/linker.ld
	echo
	echo "-> Compiling kernel image for host $(HOST).."
	echo ""
	echo "   Using:  CC       = $(KERNEL_CC)"
	echo "           AR       = $(KERNEL_AR)"
	echo "           AS       = $(KERNEL_AS)"
	echo "           CFLAGS   = $(CFLAGS)"
	echo "           CPPFLAGS = $(CPPFLAGS)"
	echo "           HOST     = $(HOST)"
	$(KERNEL_CC) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)

System.map: laylaos.kernel
	echo
	echo "-> Extracting kernel symbol table.."
	nm $< -g | grep -v '.*__.*\|.*_.*_r$$' > $@

$(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o:
	OBJ=`$(KERNEL_CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

%.o: %.c
	$(KERNEL_CC) -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

%.o: %.S
	$(KERNEL_CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

clean: clean-modules
	rm -f laylaos.kernel
	rm -f System.map
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

install: install-headers install-kernel install-modules

install-headers:
	mkdir -p "$(DESTDIR)$(INCLUDEDIR)"
	mkdir -p "$(DESTDIR)$(INCLUDEDIR)/machine"
	cp -R --preserve=timestamps include/. "$(DESTDIR)$(INCLUDEDIR)/."

install-kernel: laylaos.kernel System.map install-modules
	mkdir -p "$(DESTDIR)$(BOOTDIR)"
	cp laylaos.kernel "$(DESTDIR)$(BOOTDIR)"
	cp System.map "$(DESTDIR)$(BOOTDIR)" || \
	    (echo "Failed to create System.map" && exit 1)

# Targets to make, clean and install kernel modules

compile-modules:
	#echo
	echo "-> Compiling kernel modules.."
	#cd test && make
	cd acpi && make

clean-modules:
	#cd test && make clean
	cd acpi && make clean

install-modules: compile-modules
	echo
	echo "-> Installing kernel modules.."
	mkdir -p "$(DESTDIR)$(BOOTDIR)/modules"
	#cp test/test "$(DESTDIR)$(BOOTDIR)/modules"
	cp acpi/build/acpica.o "$(DESTDIR)$(BOOTDIR)/modules"

-include $(OBJS:.o=.d)

.SILENT:
