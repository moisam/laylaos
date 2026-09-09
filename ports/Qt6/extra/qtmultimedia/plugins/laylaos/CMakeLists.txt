# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: BSD-3-Clause


qt_internal_add_plugin(QLaylaOSMediaPlugin
    OUTPUT_NAME laylaosmediaplugin
    PLUGIN_TYPE multimedia
    SOURCES
        qlaylaosformatinfo.cpp qlaylaosformatinfo_p.h
        qlaylaosmediaintegration.cpp qlaylaosmediaintegration_p.h
        mediaplayer/qlaylaosmediaplayer.cpp mediaplayer/qlaylaosmediaplayer_p.h
        mediaplayer/qlaylaosvideosink.cpp mediaplayer/qlaylaosvideosink_p.h
    INCLUDE_DIRECTORIES
        mediaplayer
    LIBRARIES
        Qt::MultimediaPrivate
        Qt::CorePrivate
        avformat
        avcodec
        avutil
        swscale
        swresample
)
