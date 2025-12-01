TARGET = qtmedia_laylaos_audio

QT += multimedia-private

HEADERS += laylaosaudioplugin.h \
           laylaosaudiodeviceinfo.h \
           laylaosaudioinput.h \
           laylaosaudiooutput.h \
           laylaosaudioutils.h

SOURCES += laylaosaudioplugin.cpp \
           laylaosaudiodeviceinfo.cpp \
           laylaosaudioinput.cpp \
           laylaosaudiooutput.cpp \
           laylaosaudioutils.cpp

OTHER_FILES += laylaos_audio.json

PLUGIN_TYPE = audio
load(qt_plugin)
