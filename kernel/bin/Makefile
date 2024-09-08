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

PROGS=dispman getty login chvt \
      cal more mount reboot ps umount free \
      init

# these should be installed from coreutils:
# echo cat ls

# extra setup for the x86-64 kernel
LIBAELF:=desktop/libaelf$(if $(HOST) == "x86_64",64,32).a


GUI_DIR:=desktop
GUISERVER_DIR:=$(GUI_DIR)/server
GUICLIENT_DIR:=$(GUI_DIR)/client
GUICOMMON_DIR:=$(GUI_DIR)/common
GUIWIDGET_DIR:=$(GUI_DIR)/widgets

GUISERVER_SRCS:=$(GUISERVER_DIR)/cursor.c \
                $(GUISERVER_DIR)/clipboard.c \
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
                $(GUISERVER_DIR)/theme.c \
                $(GUICOMMON_DIR)/event.c \
                $(GUICOMMON_DIR)/font.c \
                $(GUICOMMON_DIR)/global.c \
                $(GUICOMMON_DIR)/list.c $(GUICOMMON_DIR)/listnode.c \
                $(GUICOMMON_DIR)/rect.c \
                $(GUICOMMON_DIR)/deflate.c \
                $(GUI_DIR)/imglib/ico.c \
                $(GUI_DIR)/imglib/png.c \
                $(GUI_DIR)/imglib/png_crc.c \
                $(GUI_DIR)/hash.c $(GUI_DIR)/fnv1a.c \
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
             $(GUICOMMON_DIR)/gc-ttf.c \
             $(GUICOMMON_DIR)/global.c \
             $(GUICOMMON_DIR)/init.c \
             $(GUICOMMON_DIR)/list.c $(GUICOMMON_DIR)/listnode.c \
             $(GUICOMMON_DIR)/rect.c \
             $(GUICOMMON_DIR)/deflate.c \
             $(GUICLIENT_DIR)/button.c \
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
             $(GUICLIENT_DIR)/theme.c \
             $(GUICLIENT_DIR)/widget.c \
             $(GUI_DIR)/imglib/ico.c \
             $(GUI_DIR)/imglib/png.c \
             $(GUI_DIR)/imglib/png_crc.c \
             $(GUI_DIR)/imglib/jpeg.c
LIBGUI_OBJS:=$(patsubst %.c, %.o, $(LIBGUI_SRCS))

DESKTOP_SRCS:=$(GUI_DIR)/hash.c $(GUI_DIR)/ptrhash.c \
              $(GUI_DIR)/desktop/desktop_alt_tab.c \
              $(GUI_DIR)/desktop/desktop_entries.c \
              $(GUI_DIR)/desktop/desktop_background.c \
              $(GUI_DIR)/desktop/desktop.c
DESKTOP_OBJS:=$(patsubst %.c, %.o, $(DESKTOP_SRCS))

DESKTOP_BOTTOMPANEL_SRCS:=$(GUI_DIR)/panels/bottom-panel.c
DESKTOP_BOTTOMPANEL_OBJS:=$(patsubst %.c, %.o, $(DESKTOP_BOTTOMPANEL_SRCS))

DESKTOP_TOPPANEL_SRCS:=$(GUI_DIR)/list.c \
                       $(GUI_DIR)/panels/top-panel.c \
                       $(GUI_DIR)/panels/top-panel-widgets.c \
                       $(GUI_DIR)/panels/top-panel-widgets-menu.c
DESKTOP_TOPPANEL_OBJS:=$(patsubst %.c, %.o, $(DESKTOP_TOPPANEL_SRCS))

GUIWIDGET_SRCS:=$(shell find $(GUIWIDGET_DIR) -name '*.c')
GUIWIDGET_OBJS:=$(patsubst %.c, %, $(GUIWIDGET_SRCS))

TERMAPP_SRCS:=$(GUI_DIR)/app_lterm/term_input.c \
              $(GUI_DIR)/app_lterm/term.c \
              $(GUI_DIR)/app_lterm/main.c \
              $(GUI_DIR)/memsetw.c
TERMAPP_OBJS:=$(patsubst %.c, %.o, $(TERMAPP_SRCS))

CALCAPP_SRCS:=$(GUI_DIR)/app_calc/main.c \
              $(GUI_DIR)/app_calc/help.c
CALCAPP_OBJS:=$(patsubst %.c, %.o, $(CALCAPP_SRCS))

KBDAPP_SRCS:=$(GUI_DIR)/app_keyboard/main.c
KBDAPP_OBJS:=$(patsubst %.c, %.o, $(KBDAPP_SRCS))

FILESAPP_SRCS:=$(GUI_DIR)/app_files/main.c \
               $(GUI_DIR)/app_files/help.c \
               $(GUI_DIR)/app_files/history.c \
               $(GUI_DIR)/app_files/properties.c
FILESAPP_OBJS:=$(patsubst %.c, %.o, $(FILESAPP_SRCS))

IMGVIEWAPP_SRCS:=$(GUI_DIR)/app_imgviewer/main.c \
                 $(GUI_DIR)/app_imgviewer/help.c \
                 $(GUI_DIR)/app_imgviewer/properties.c
IMGVIEWAPP_OBJS:=$(patsubst %.c, %.o, $(IMGVIEWAPP_SRCS))

SYSICONVIEWAPP_SRCS:=$(GUI_DIR)/app_sysicon_viewer/main.c
SYSICONVIEWAPP_OBJS:=$(patsubst %.c, %.o, $(SYSICONVIEWAPP_SRCS))

TETRISAPP_SRCS:=$(GUI_DIR)/app_tetris/main.c \
                $(GUI_DIR)/app_tetris/help.c
TETRISAPP_OBJS:=$(patsubst %.c, %.o, $(TETRISAPP_SRCS))

MINESWEEPERAPP_SRCS:=$(GUI_DIR)/app_minesweeper/main.c \
                $(GUI_DIR)/app_minesweeper/help.c \
                $(GUI_DIR)/app_minesweeper/options.c
MINESWEEPERAPP_OBJS:=$(patsubst %.c, %.o, $(MINESWEEPERAPP_SRCS))

CLOCKAPP_SRCS:=$(GUI_DIR)/app_clock/main.c
CLOCKAPP_OBJS:=$(patsubst %.c, %.o, $(CLOCKAPP_SRCS))

SETTINGSAPP_SRCS:=$(GUI_DIR)/app_settings/main.c \
                  $(GUI_DIR)/app_settings/show_background.c \
                  $(GUI_DIR)/app_settings/show_theme.c \
                  $(GUI_DIR)/app_settings/show_sysinfo.c \
                  $(GUI_DIR)/app_settings/show_display.c
SETTINGSAPP_OBJS:=$(patsubst %.c, %.o, $(SETTINGSAPP_SRCS))

PNGTEST_SRCS:= \
             $(GUI_DIR)/imglib/png_test.c
PNGTEST_OBJS:=$(patsubst %.c, %.o, $(PNGTEST_SRCS))

LIBGUI:=$(GUI_DIR)/libgui.a
LIBGUISO:=$(GUI_DIR)/libgui.so
GUIFLAGS=$(LIBGUI) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS_GUI)

GUI_PROGS:=$(GUI_DIR)/server/guiserver \
           $(GUI_DIR)/desktop/desktop \
           $(GUI_DIR)/panels/desktop-bottom-panel \
           $(GUI_DIR)/panels/desktop-top-panel \
           $(GUI_DIR)/ltermapp \
           $(GUI_DIR)/calcapp \
           $(GUI_DIR)/clockapp \
           $(GUI_DIR)/filesapp \
           $(GUI_DIR)/kbdapp \
           $(GUI_DIR)/imgviewapp \
           $(GUI_DIR)/sysiconviewapp \
           $(GUI_DIR)/tetrisapp \
           $(GUI_DIR)/minesweeperapp \
           $(GUI_DIR)/settingsapp \
           $(GUI_DIR)/gui_test \
           $(GUI_DIR)/png_test


all: check-config $(PROGS) lspci strace $(LIBGUI) $(LIBGUISO) $(GUI_PROGS) widgets epilogue install

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
	echo "    Compiled: " $(PROGS) lspci strace $(GUI_PROGS)
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

strace: strace-arg-print.o strace-eoptions.o strace.o
	echo "    Linking   " $@
	$(CC) -o $@ strace-arg-print.o strace-eoptions.o strace.o $(CFLAGS) $(CPPFLAGS)

$(LIBGUI): $(LIBGUI_OBJS)
	echo "    Creating static lib " $@
	$(AR) rcs $@ $(LIBGUI_OBJS)

$(LIBGUISO): $(LIBGUI_OBJS)
	echo "    Creating shared lib " $@
	$(CC) $(CFLAGS) $(CPPFLAGS) -shared -fPIC -DPIC -o $@ $(LIBGUI_OBJS) -ljpeg

widgets:
	for i in $(GUIWIDGET_OBJS) ; do \
	    echo "    Compiling " $$i ; \
	    $(CC) $(CFLAGS) $(CPPFLAGS) -static-libgcc -shared -fPIC -DPIC -o $$i.so $$i.c ; \
	done


$(GUI_DIR)/server/guiserver: $(GUISERVER_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(GUISERVER_OBJS) $(LIBAELF) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS_GUI)

#$(GUI_DIR)/deflate.o: ../kernel/gunzip/deflate.c
#	echo "    Compiling " $<
#	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

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

$(GUI_DIR)/desktop/desktop: $(DESKTOP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(DESKTOP_OBJS) $(GUIFLAGS) -ljpeg

$(GUI_DIR)/panels/desktop-bottom-panel: $(DESKTOP_BOTTOMPANEL_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(DESKTOP_BOTTOMPANEL_OBJS) $(GUIFLAGS)

$(GUI_DIR)/panels/desktop-top-panel: $(DESKTOP_TOPPANEL_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(DESKTOP_TOPPANEL_OBJS) -Wl,-dynamic-list,$(GUI_DIR)/panels/top-panel-dynsym.syms -ldl $(GUIFLAGS)

$(GUI_DIR)/ltermapp: $(TERMAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(TERMAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/memsetw.o: ../../libk/string/memsetw.c
	echo "    Compiling " $<
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(GUI_DIR)/calcapp: $(CALCAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(CALCAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/filesapp: $(FILESAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(FILESAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/kbdapp: $(KBDAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(KBDAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/imgviewapp: $(IMGVIEWAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(IMGVIEWAPP_OBJS) $(GUIFLAGS) -ljpeg

$(GUI_DIR)/sysiconviewapp: $(SYSICONVIEWAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(SYSICONVIEWAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/tetrisapp: $(TETRISAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(TETRISAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/minesweeperapp: $(MINESWEEPERAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(MINESWEEPERAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/clockapp: $(CLOCKAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(CLOCKAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/settingsapp: $(SETTINGSAPP_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(SETTINGSAPP_OBJS) $(GUIFLAGS)

$(GUI_DIR)/gui_test: $(GUI_DIR)/gui_test.o
	echo "    Linking   " $@
	$(CC) -o $@ $< $(GUIFLAGS)

$(GUI_DIR)/png_test: $(PNGTEST_OBJS)
	echo "    Linking   " $@
	$(CC) -o $@ $(PNGTEST_OBJS) $(GUIFLAGS)

%.o: %.c
	echo "    Compiling " $<
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

%.o: %.S
	echo "    Compiling " $<
	$(CC) -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

clean:
	echo "    Removing *.o and executables"
	rm -f *.o
	rm -f $(PROGS) lspci strace $(GUI_PROGS)
	echo "    Removing " `find . -name '*.o'`
	rm -f `find . -name '*.o'`
	echo "    Removing guiserver desktop"
	rm -f $(GUI_DIR)/server/guiserver $(GUI_DIR)/desktop/desktop
	echo "    Removing widgets *.so"
	rm -f $(GUI_DIR)/widgets/*.so
	echo "    Removing shared and static ligbui"
	rm -f $(LIBGUI) $(LIBGUISO)

.SILENT:
