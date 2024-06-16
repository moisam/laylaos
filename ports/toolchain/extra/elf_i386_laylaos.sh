source_sh ${srcdir}/emulparams/elf_i386.sh
TEXT_START_ADDR=0x08048000
GENERATE_SHLIB_SCRIPT=yes
GENERATE_PIE_SCRIPT=yes
OUTPUT_FORMAT="elf32-i386"
SCRIPT_NAME=elf
MAXPAGESIZE="CONSTANT (MAXPAGESIZE)"
COMMONPAGESIZE="CONSTANT (COMMONPAGESIZE)"
ARCH=i386
MACHINE=
TEMPLATE_NAME=elf
NO_SMALL_DATA=yes
SEPARATE_GOTPLT="SIZEOF (.got.plt) >= 12 ? 12 : 0"
IREL_IN_PLT=
# These sections are placed right after .plt section.
OTHER_PLT_SECTIONS="
.plt.got      ${RELOCATING-0} : { *(.plt.got) }
.plt.sec      ${RELOCATING-0} : { *(.plt.sec) }
"

