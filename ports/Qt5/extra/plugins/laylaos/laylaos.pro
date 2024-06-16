TARGET = qlaylaos

QT += core-private gui-private eventdispatcher_support-private \
      fontdatabase_support-private service_support-private

SOURCES = \
    main.cpp \
    qlaylaosbuffer.cpp \
    qlaylaosclipboard.cpp \
    qlaylaoscursor.cpp \
    qlaylaosintegration.cpp \
    qlaylaoskeymapper.cpp \
    qlaylaosrasterbackingstore.cpp \
    qlaylaosscreen.cpp \
    qlaylaoswindow.cpp \
    qlaylaoseventlooper.cpp

HEADERS = \
    main.h \
    qlaylaosbuffer.h \
    qlaylaosclipboard.h \
    qlaylaoscursor.h \
    qlaylaosintegration.h \
    qlaylaoskeymapper.h \
    qlaylaosrasterbackingstore.h \
    qlaylaosscreen.h \
    qlaylaoswindow.h \
    qlaylaoseventlooper.h

LIBS += -lgui -lfreetype

OTHER_FILES += laylaos.json

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QLaylaOSIntegrationPlugin
!equals(TARGET, $$QT_DEFAULT_QPA_PLUGIN): PLUGIN_EXTENDS = -
load(qt_plugin)
