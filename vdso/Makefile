##


CFLAGS:=-O2 -Wall -Wextra -pedantic -D__laylaos__ -D__POSIX_VISIBLE -fPIC -DPIC -nostartfiles -nostdlib -fno-builtin
CFLAGS:=$(CFLAGS) --sysroot=$(SYSROOT) -isystem=/usr/local/include
CFLAGS:=$(CFLAGS) $(if $(HOST) == "x86_64",-z max-page-size=0x1000 -mcmodel=large -D__x86_64__,)

CPPFLAGS:=$(CPPFLAGS) -I../kernel/include
LDFLAGS:=$(LDFLAGS)

OBJS:=vdso.o

all: check-config vdso.so

.PHONY: all

check-config:
	if [ "$(HOST)" != "x86_64" ]; then  \
	    if [ "$(HOST)" != "i686" ]; then   \
	        echo; echo "Usage:"; echo "  make HOST={x86_64|i686}";    \
	        exit 1;     \
	    fi  \
	fi

%.o: %.c
	echo "    Compiling " $<
	$(CC) -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

vdso.so: vdso.o
	echo "    Linking " $@
	$(CC) -shared $(CFLAGS) -o $@ -T link.ld $<
	#$(CC) -shared -o $@ $(CFLAGS) $<

clean:
	echo "    Removing objects"
	/bin/rm -f *.o
	/bin/rm -f vdso.so

.SILENT:
