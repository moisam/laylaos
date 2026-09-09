# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause

#####################################################################
## QLaylaOSIntegrationPlugin Plugin:
#####################################################################

qt_internal_add_plugin(QLaylaOSIntegrationPlugin
    OUTPUT_NAME laylaos
    PLUGIN_TYPE platforms
    DEFAULT_IF "laylaos" IN_LIST QT_QPA_PLATFORMS
    SOURCES
        main.cpp
        qlaylaosbuffer.cpp qlaylaosbuffer.h
        qlaylaosclipboard.cpp qlaylaosclipboard.h
        qlaylaoscursor.cpp qlaylaoscursor.h
        qlaylaosintegration.cpp qlaylaosintegration.h
        qlaylaoskeymapper.cpp qlaylaoskeymapper.h
        qlaylaosrasterbackingstore.cpp qlaylaosrasterbackingstore.h
        qlaylaosscreen.cpp qlaylaosscreen.h
        qlaylaossocketmonitor.cpp qlaylaossocketmonitor.h
        qlaylaoswindow.cpp qlaylaoswindow.h
        qlaylaosforeignwindow.cpp qlaylaosforeignwindow.h
        qlaylaosplatformtheme.cpp qlaylaosplatformtheme.h
        qlaylaosdrag.cpp qlaylaosdrag.h
        qlaylaosclientevents.h
    LIBRARIES
        Qt::Core
        Qt::CorePrivate
        Qt::Gui
        Qt::GuiPrivate
        gui
        freetype
)

## Scopes:
#####################################################################

qt_internal_extend_target(QLaylaOSIntegrationPlugin CONDITION QT_FEATURE_egl
    SOURCES
        qlaylaoseglwindow.cpp qlaylaoseglwindow.h
        qlaylaosglcontext.cpp qlaylaosglcontext.h
    LIBRARIES
        EGL::EGL
)

qt_internal_extend_target(QLaylaOSIntegrationPlugin CONDITION QT_FEATURE_systemtrayicon
    SOURCES
        qlaylaossystemtrayicon.cpp qlaylaossystemtrayicon.h
)

