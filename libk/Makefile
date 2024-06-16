#HOST?=$(shell ../default-host.sh)
#HOSTARCH:=$(shell ../target-triplet-to-arch.sh $(HOST))
HOSTARCH:=$(if $(HOST) == "x86_64",x86_64,i386)

KERNEL_CFLAGS?=-O2 -g
KERNEL_CPPFLAGS?=
#LDFLAGS?=
#LIBS?=

#DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
INCLUDEDIR?=$(PREFIX)/include
LIBDIR?=$(EXEC_PREFIX)/lib

CFLAGS:=$(KERNEL_CFLAGS) $(if $(HOST) == "x86_64",-z max-page-size=0x1000 -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -D__x86_64__,)
CFLAGS:=$(CFLAGS) -Wall -Wextra -fno-builtin-memset -fno-builtin-memcpy -fno-tree-loop-distribute-patterns
CPPFLAGS:=$(KERNEL_CPPFLAGS) -D__is_laylaos_libc -Iinclude
LIBK_CFLAGS:=$(CFLAGS) -ffreestanding -fbuiltin -nostdlib
LIBK_CPPFLAGS:=$(CPPFLAGS) -DKERNEL

#ARCH_HEADERS:=exec.h reloc.h

CFLAGS:=$(CFLAGS) $(ARCH_CFLAGS)
CPFLAGS:=$(CPPFLAGS) $(ARCH_CPPFLAGS)
LIBK_CFLAGS:=$(LIBK_CFLAGS) $(KERNEL_ARCH_CFLAGS)
LIBK_CPFLAGS:=$(LIBK_CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)

FREEOBJS:=\
$(ARCH_FREEOBJS) \
string/memcmp.o \
string/memcpy.o \
string/memmove.o \
string/memset.o \
string/memsetw.o \
string/strcat.o \
string/strcmp.o \
string/strcpy.o \
string/strerror.o \
string/strlen.o \
string/strstr.o \
string/stpcpy.o \
stdlib/strtol.o \
stdlib/strtoul.o \
time/gmtime.o \
unix/basename.o

HOSTEDOBJS:=\
$(ARCH_HOSTEDOBJS) \

OBJS:=\
$(FREEOBJS) \
$(HOSTEDOBJS) \

LIBK_OBJS:=$(FREEOBJS:.o=.libk.o)

BINARIES=libk.a

all: $(BINARIES)

.PHONY: all clean install install-headers install-libs

libk.a: $(LIBK_OBJS)
	echo "    Linking   " $@
	$(KERNEL_AR) rcs $@ $(LIBK_OBJS)

%.o: %.c
	echo "    Compiling " $@
	$(KERNEL_CC) -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

%.o: %.S
	echo "    Compiling " $@
	$(KERNEL_CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

%.libk.o: %.c
	echo "    Compiling " $@
	$(KERNEL_CC) -c $< -o $@ -std=gnu11 $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)

%.libk.o: %.S
	echo "    Compiling " $@
	$(KERNEL_CC) -c $< -o $@ $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)

clean:
	echo "    Cleaning up"
	rm -f $(BINARIES) *.a
	rm -f $(OBJS) $(LIBK_OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) $(LIBK_OBJS:.o=.d) *.d */*.d */*/*.d

install: install-headers install-libs

install-headers:
	echo "    Installing header files"
	mkdir -p "$(DESTDIR)$(INCLUDEDIR)"
	cp -R --preserve=timestamps include/. "$(DESTDIR)$(INCLUDEDIR)/."
	#mkdir -p "$(DESTDIR)$(INCLUDEDIR)/machine"
	##echo "$(PWD)"
	#$(foreach var, $(ARCH_HEADERS), cp -fR --preserve=timestamps "arch/$(HOSTARCH)/include/$(var)" \
	#	"$(DESTDIR)$(INCLUDEDIR)/machine/$(var)";)

install-libs: $(BINARIES)
	echo "    Installing libraries"
	mkdir -p "$(DESTDIR)$(LIBDIR)"
	cp $(BINARIES) "$(DESTDIR)$(LIBDIR)"


-include $(OBJS:.o=.d)
-include $(LIBK_OBJS:.o=.d)

.SILENT:
