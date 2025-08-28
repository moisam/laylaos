CFLAGS=-O0
CPPFLAGS?=
#LDFLAGS?=
#LIBS?=

#DESTDIR?=
#PREFIX?=/usr/local
#EXEC_PREFIX?=$(PREFIX)
#BOOTDIR?=$(EXEC_PREFIX)/boot
#INCLUDEDIR?=$(PREFIX)/include

OSNAME:=$(shell uname -s)
RM_CMD:=$(if $(OSNAME) == "LaylaOS", /usr/bin/rm, /bin/rm)
CP_CMD:=$(if $(OSNAME) == "LaylaOS", /usr/bin/cp, /bin/cp)
LS_CMD:=$(if $(OSNAME) == "LaylaOS", /usr/bin/ls, /bin/ls)
MKDIR_CMD:=$(if $(OSNAME) == "LaylaOS", /usr/bin/mkdir, /bin/mkdir)

CFLAGS:=$(CFLAGS) -Wall -Wextra -Wbad-function-cast \
                  -Werror -Wformat=2 \
                  -Wmissing-declarations -Wmissing-prototypes \
                  -Wstrict-aliasing=0 -Wstrict-prototypes -Wswitch-default \
                  -Wpointer-arith -Wundef -Waddress -Waggregate-return \
                  -Winit-self -Winline -Wmissing-declarations \
                  -Wmissing-field-initializers -Wnested-externs \
                  -Wold-style-definition -Wno-format-nonliteral \
                  -Wredundant-decls \
                  -ffreestanding -D__laylaos__ -D_GNU_SOURCE -DBUILDING_ACPICA \
                  -DACPI_DISASSEMBLER -I. -DNO_PROCESSOR_LOCAL

CFLAGS:=$(CFLAGS) $(if $(HOST) == "x86_64",-z max-page-size=0x1000 -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -D__x86_64__,)

BUILDDIR?=build

SRCS:=\
module.c \
tools/acpidump/apdump.c tools/acpidump/apfiles.c \
$(shell find common/ components/ os_specific/ -name '*.c')

OBJS:=\
$(patsubst %.c, %.o, $(SRCS))

BUILDDIR_EXISTS:=$(shell if [ -d "$(BUILDDIR)" -a -e "$(BUILDDIR)/acpica.o" ]; then echo "yes"; fi)

ifeq ($(BUILDDIR_EXISTS),yes)
all: skip
else
all: prep acpica.o
endif

skip:
	echo
	echo "    Skipping building ACPI module for host $(HOST)"
	echo "    If you want to force rebuild it, remove the build dir:"
	echo "        $(BUILDDIR)"
	echo

clean:
	echo "    Cleaning up for host $(HOST)"
	if [ -e $(BUILDDIR) ]; then $(RM_CMD) -r $(BUILDDIR); fi

prep: clean
	echo "    Preparing build tree for host $(HOST)"
	$(MKDIR_CMD) $(BUILDDIR) && \
	$(CP_CMD) $(SRCS) $(BUILDDIR) && \
	$(CP_CMD) -r ../include/acpi/* $(BUILDDIR) && \
	$(CP_CMD) tools/acpidump/acpidump.h $(BUILDDIR) && \
	$(CP_CMD) os_specific/service_layers/dirent.h $(BUILDDIR) && \
	$(CP_CMD) os_specific/service_layers/ctype.h $(BUILDDIR)


acpica.o:
	echo "-> Compiling.."
	cd $(BUILDDIR); \
	for file in `$(LS_CMD) *.c`; do \
	echo "   CC " $$file; \
	$(KERNEL_CC) -c -fPIC -DPIC -nostartfiles -nostdlib -fno-builtin $$file -std=c99 $(CFLAGS) $(CPPFLAGS); \
	if [ $$? -ne 0 ]; then echo "Failed to compile"; exit 1; fi; \
	done
	# [ -e $(BUILDDIR)/$@ ] && $(RM_CMD) $(BUILDDIR)/$@
	echo "-> Linking for host $(HOST).."
	cd $(BUILDDIR); \
	$(KERNEL_CC) -shared -fPIC -DPIC -nostartfiles -nostdlib -fno-builtin -o $@ $(CFLAGS) `$(LS_CMD) *.o` -lgcc

.SILENT:
