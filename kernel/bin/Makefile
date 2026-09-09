##
## Makefile for building user utility programs
##


INCLUDEDIR:=$(SYSROOT)/usr/include
LIBRARYDIR:=$(SYSROOT)/usr/lib

CFLAGS:=-Wall -O2 --sysroot=$(SYSROOT) -isystem=/usr/local/include
CFLAGS:=$(CFLAGS) -D_POSIX_PRIORITY_SCHEDULING=1 -fPIC -DPIC
CFLAGS:=$(CFLAGS) $(if $(HOST) == "x86_64",-z max-page-size=0x1000 -mcmodel=large -D__x86_64__,)
CFLAGS:=$(CFLAGS) -I$(INCLUDEDIR)/freetype2

LDFLAGS_GUI:=-lfreetype

LIBCPATH:=$(SYSROOT)/usr/local/lib/libc.a
#CPPFLAGS:=-fprofile-arcs -ftest-coverage
CPPFLAGS?=

PROGS=dispman getty chvt \
      cal more reboot ps umount free \
      daemon init losetup getopt \
      gui-open

# these should be installed from coreutils:
# echo cat ls

# this should be installed from shadow:
# login

# extra setup for the x86-64 kernel
LIBAELF:=desktop/libaelf$(if $(HOST) == "x86_64",64,32).a


GUI_DIR:=desktop
GUISERVER_DIR:=$(GUI_DIR)/server
GUICLIENT_DIR:=$(GUI_DIR)/client
GUICOMMON_DIR:=$(GUI_DIR)/common
GUIWIDGET_DIR:=$(GUI_DIR)/widgets

GUISERVER_SRCS:=$(GUISERVER_DIR)/cursor.c \
                $(GUISERVER_DIR)/cursor-x11.c \
                $(GUISERVER_DIR)/clipboard.c \
                $(GUISERVER_DIR)/dragndrop.c \
                $(GUISERVER_DIR)/gc.c \
                $(GUISERVER_DIR)/gc-bitmap.c \
                $(GUISERVER_DIR)/gc-bitmap-stretch.c \
                $(GUISERVER_DIR)/gc-ttf.c \
                $(GUISERVER_DIR)/keys.c \
                $(GUISERVER_DIR)/resources.c \
                $(GUISERVER_DIR)/resources-sysicons.c \
                $(GUISERVER_DIR)/server-login.c \
                $(GUISERVER_DIR)/server-window.c \
                $(GUISERVER_DIR)/server-window-controlbox.c \
                $(GUISERVER_DIR)/server-window-mouse.c \
                $(GUISERVER_DIR)/screenshot.c \
                $(GUISERVER_DIR)/theme.c \
                $(GUICOMMON_DIR)/gc-grad-vert.c \
                $(GUICOMMON_DIR)/gc-gc.c \
                $(GUICOMMON_DIR)/event.c \
                $(GUICOMMON_DIR)/font.c \
                $(GUICOMMON_DIR)/global.c \
                $(GUICOMMON_DIR)/list.c $(GUICOMMON_DIR)/listnode.c \
                $(GUICOMMON_DIR)/rect.c \
                $(GUICOMMON_DIR)/deflate.c \
                $(GUI_DIR)/imglib/ico.c \
                $(GUI_DIR)/imglib/png.c \
                $(GUI_DIR)/imglib/png_crc.c \
                $(GUI_DIR)/curlib/x11cur.c \
                $(GUI_DIR)/hash.c $(GUI_DIR)/ptrhash.c $(GUI_DIR)/fnv1a.c \
                $(GUISERVER_DIR)/main.c
GUISERVER_OBJS:=$(patsubst %.c, %.o, $(GUISERVER_SRCS))

LIBGUI_SRCS:=$(GUICOMMON_DIR)/event.c $(GUICOMMON_DIR)/next-event.c \
             $(GUICOMMON_DIR)/font.c \
             $(GUICLIENT_DIR)/colorchooser-dialog.c \
             $(GUICOMMON_DIR)/gc.c \
             $(GUICOMMON_DIR)/gc-arc.c \
             $(GUICOMMON_DIR)/gc-bitmap.c \
             $(GUICOMMON_DIR)/gc-bitmap-stretch.c \
             $(GUICOMMON_DIR)/gc-circle.c \
             $(GUICOMMON_DIR)/gc-line.c \
             $(GUICOMMON_DIR)/gc-poly.c \
             $(GUICOMMON_DIR)/gc-round-rect.c \
             $(GUICOMMON_DIR)/gc-ttf.c \
             $(GUICOMMON_DIR)/gc-gc.c \
             $(GUICOMMON_DIR)/gc-grad-vert.c \
             $(GUICOMMON_DIR)/global.c \
             $(GUICOMMON_DIR)/init.c \
             $(GUICOMMON_DIR)/list.c $(GUICOMMON_DIR)/listnode.c \
             $(GUICOMMON_DIR)/rect.c \
             $(GUICOMMON_DIR)/deflate.c \
             $(GUICLIENT_DIR)/button.c \
             $(GUICLIENT_DIR)/dragndrop.c \
             $(GUICLIENT_DIR)/imgbutton.c \
             $(GUICLIENT_DIR)/push-imgbutton.c \
             $(GUICLIENT_DIR)/listview.c \
             $(GUICLIENT_DIR)/file-selector.c \
             $(GUICLIENT_DIR)/gallery-view.c \
             $(GUICLIENT_DIR)/group-border.c \
             $(GUICLIENT_DIR)/combobox.c \
             $(GUICLIENT_DIR)/inputbox.c \
             $(GUICLIENT_DIR)/radio-button.c \
             $(GUICLIENT_DIR)/textbox.c \
             $(GUICLIENT_DIR)/statusbar.c \
             $(GUICLIENT_DIR)/scrollbar.c \
             $(GUICLIENT_DIR)/spinner.c \
             $(GUICLIENT_DIR)/toggle.c \
             $(GUICLIENT_DIR)/clipboard.c \
             $(GUICLIENT_DIR)/cursor.c \
             $(GUICLIENT_DIR)/client-window.c \
             $(GUICLIENT_DIR)/client-window-mouse.c \
             $(GUICLIENT_DIR)/kbd.c \
             $(GUICLIENT_DIR)/keys.c $(GUICLIENT_DIR)/key-names.c \
             $(GUICLIENT_DIR)/label.c \
             $(GUICLIENT_DIR)/menu.c \
             $(GUICLIENT_DIR)/messagebox.c \
             $(GUICLIENT_DIR)/messagebox-about.c \
             $(GUICLIENT_DIR)/messagebox-shortcuts.c \
             $(GUICLIENT_DIR)/misc.c \
             $(GUICLIENT_DIR)/mouse.c \
             $(GUICLIENT_DIR)/opensave-dialog.c \
             $(GUICLIENT_DIR)/resources.c \
             $(GUICLIENT_DIR)/request.c \
             $(GUICLIENT_DIR)/screen.c \
             $(GUICLIENT_DIR)/screenshot.c \
             $(GUICLIENT_DIR)/systray.c \
             $(GUICLIENT_DIR)/theme.c \
             $(GUICLIENT_DIR)/widget.c \
             $(GUI_DIR)/imglib/ico.c \
             $(GUI_DIR)/imglib/png.c \
             $(GUI_DIR)/imglib/png_crc.c \
             $(GUI_DIR)/imglib/jpeg.c \
             $(GUI_DIR)/curlib/x11cur.c
LIBGUI_OBJS:=$(patsubst %.c, %.o, $(LIBGUI_SRCS))

TERMAPP_SRCS:=$(GUI_DIR)/app_lterm/term_input.c \
              $(GUI_DIR)/app_lterm/term.c \
              $(GUI_DIR)/app_lterm/main.c \
              $(GUI_DIR)/memsetw.c
TERMAPP_OBJS:=$(patsubst %.c, %.o, $(TERMAPP_SRCS))

SYSICONVIEWAPP_SRCS:=$(GUI_DIR)/app_sysicon_viewer/main.c
SYSICONVIEWAPP_OBJS:=$(patsubst %.c, %.o, $(SYSICONVIEWAPP_SRCS))

TETRISAPP_SRCS:=$(GUI_DIR)/app_tetris/main.c \
                $(GUI_DIR)/app_tetris/help.c
TETRISAPP_OBJS:=$(patsubst %.c, %.o, $(TETRISAPP_SRCS))

MINESWEEPERAPP_SRCS:=$(GUI_DIR)/app_minesweeper/main.c \
                $(GUI_DIR)/app_minesweeper/help.c \
                $(GUI_DIR)/app_minesweeper/options.c
MINESWEEPERAPP_OBJS:=$(patsubst %.c, %.o, $(MINESWEEPERAPP_SRCS))

LIBGUI:=$(GUI_DIR)/libgui.a
LIBGUISO:=$(GUI_DIR)/libgui.so
GUIFLAGS=$(LIBGUI) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS_GUI)

GUI_PROGS:=$(GUI_DIR)/server/guiserver \
           $(GUI_DIR)/ltermapp \
           $(GUI_DIR)/sysiconviewapp \
           $(GUI_DIR)/tetrisapp \
           $(GUI_DIR)/minesweeperapp \
           $(GUI_DIR)/gui_test


all: check-config $(PROGS) lspci lsusb strace mount $(LIBGUI) $(LIBGUISO) $(GUI_PROGS) epilogue install

check-config:
	if [ "$(HOST)" != "x86_64" ]; then  \
	    if [ "$(HOST)" != "i686" ]; then   \
	        echo; echo "Usage:"; echo "  make HOST={x86_64|i686}";    \
	        exit 1;     \
	    fi  \
	fi
	echo ""
	echo "Using:  CC       = $(CC)"
	echo "        AR       = $(AR)"
	echo "        AS       = $(AS)"
	echo "        CFLAGS   = $(CFLAGS)"
	echo "        CPPFLAGS = $(CPPFLAGS)"
	echo "        HOST     = $(HOST)"

epilogue:
	echo "    Compiled: " $(PROGS) lspci lsusb strace $(GUI_PROGS)
	echo "    Compiled: " $(LIBGUI) $(LIBGUISO)
	echo "    Compiled: " $(GUIWIDGET_OBJS)

install: install-gui-headers install-libs

install-gui-headers:
	echo "    Copying GUI header files (dest $(INCLUDEDIR)/gui/)"
	mkdir -p "$(INCLUDEDIR)/gui"
	cp -R --preserve=timestamps $(GUI_DIR)/include/. "$(INCLUDEDIR)/gui/."

install-libs:
	echo "    Copying static and shared GUI libraries (dest $(LIBRARYDIR)/)"
	mkdir -p "$(LIBRARYDIR)/"
	cp $(LIBGUI) $(LIBGUISO) "$(LIBRARYDIR)/"

lspci: pcilib.o lspci.o
	echo "    Linking   " $@
	$(CC) -o $@ lspci.o pcilib.o $(CFLAGS) $(CPPFLAGS)

lsusb: lsusb-lib.o lsusb.o
	echo "    Linking   " $@
	$(CC) -o $@ lsusb.o lsusb-lib.o $(CFLAGS) $(CPPFLAGS) -lusb-1.0

strace: strace-arg-print.o strace-eoptions.o strace.o
	echo "    Linking   " $@
	$(CC) -o $@ strace-arg-print.o strace-eoptions.o strace.o $(CFLAGS) $(CPPFLAGS)

mount: mount.o mount-fstype.o
	echo "    Linking   " $@
	$(CC) -o $@ mount.o mount-fstype.o $(CFLAGS) $(CPPFLAGS)

$(LIBGUI): $(LIBGUI_OBJS)
	echo "    Creating static lib " $@
	$(AR) rcs $@ $(LIBGUI_OBJS)

$(LIBGUISO): $(LIBGUI_OBJS)
	echo "    Creating shared lib " $@
	$(CC) $(CFLAGS) $(CPPFLAGS) -shared -fPIC -DPIC -o $@ $(LIBGUI_OBJS) -ljpeg -lfreetype
	patchelf --set-soname libgui.so $@

$(GUI_DIR)/server/guiserver: $(GUISERVER_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(GUISERVER_OBJS) $(LIBAELF) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS_GUI)

$(GUI_DIR)/hash.o: ../../misc/hash.c
	echo "    Compiling " $<
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(GUI_DIR)/fnv1a.o: ../../misc/fnv1a.c
	echo "    Compiling " $<
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(GUI_DIR)/ptrhash.o: ../../misc/ptrhash.c
	echo "    Compiling " $<
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(GUI_DIR)/list.o: ../../misc/list.c
	echo "    Compiling " $<
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(GUI_DIR)/ltermapp: $(TERMAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(TERMAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/memsetw.o: ../../libk/string/memsetw.c
	echo "    Compiling " $<
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(GUI_DIR)/sysiconviewapp: $(SYSICONVIEWAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(SYSICONVIEWAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/tetrisapp: $(TETRISAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(TETRISAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/minesweeperapp: $(MINESWEEPERAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(MINESWEEPERAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/gui_test: $(GUI_DIR)/gui_test.o
	echo "    Linking   " $@
	$(CC) -o $@ $< $(GUIFLAGS)

$(GUICOMMON_DIR)/gc-gc.o: $(GUICOMMON_DIR)/gc-gc.c
	echo "    Compiling " $<
	$(CC) -c $< -o $@ -mssse3 $(CFLAGS) $(CPPFLAGS)

%.o: %.c
	echo "    Compiling " $<
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

%.o: %.S
	echo "    Compiling " $<
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

clean:
	echo "    Removing *.o and executables"
	rm -f *.o
	rm -f $(PROGS) lspci lsusb strace mount $(GUI_PROGS)
	echo "    Removing " `find . -name '*.o'`
	rm -f `find . -name '*.o'`
	echo "    Removing guiserver desktop"
	rm -f $(GUI_DIR)/server/guiserver $(GUI_DIR)/desktop/desktop
	echo "    Removing widgets *.so"
	rm -f $(GUI_DIR)/widgets/*.so
	echo "    Removing shared and static ligbui"
	rm -f $(LIBGUI) $(LIBGUISO)

.SILENT:
