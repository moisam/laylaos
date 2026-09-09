// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2016 Research In Motion
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef LAYLAOSAUDIOOUTPUT_H
#define LAYLAOSAUDIOOUTPUT_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "private/qaudiosystem_p.h"

#include "qlaylaosaudioutils_p.h"

#include <QElapsedTimer>
#include <QTimer>
#include <QIODevice>
#include <QSocketNotifier>

#include <sys/audioio.h>

QT_BEGIN_NAMESPACE

class LaylaOSPushIODevice;

class QLaylaOSAudioSink : public QPlatformAudioSink
{
    Q_OBJECT

public:
    explicit QLaylaOSAudioSink(QAudioDevice deviceInfo, const QAudioFormat &, QObject *parent);
    ~QLaylaOSAudioSink();

    void start(QIODevice *source) override;
    QIODevice *start() override;
    void stop() override;
    void reset() override;
    void suspend() override;
    void resume() override;
    qsizetype bytesFree() const override;
    void setBufferSize(qsizetype) override;
    qsizetype bufferSize() const override;
    qint64 processedUSecs() const override;
    QAudio::State state() const override;
    qint64 pushData(const char *data, qint64 len);

private slots:
    void pullData();

private:
    bool open();
    void close();
    void changeState(QAudio::State state, QAudio::Error error);

    void suspendInternal(QAudio::State suspendState);
    void resumeInternal();

    void updateState();

    qint64 write(const char *data, qint64 len);

    QIODevice *m_source;
    bool m_pushSource;
    QTimer *m_timer;

    QAudio::State m_state;
    QAudio::State m_suspendedInState;
    int m_periodSize;

    qint64 m_bytesWritten;

    int m_requestedBufferSize;
    int m_fd;
};

class LaylaOSPushIODevice : public QIODevice
{
    Q_OBJECT
public:
    explicit LaylaOSPushIODevice(QLaylaOSAudioSink *output);
    ~LaylaOSPushIODevice();

    qint64 readData(char *data, qint64 len) override;
    qint64 writeData(const char *data, qint64 len) override;

    bool isSequential() const override;

private:
    QLaylaOSAudioSink *m_output;
};

QT_END_NAMESPACE

#endif
