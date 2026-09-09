// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2016 Research In Motion
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef LAYLAOSAUDIOINPUT_H
#define LAYLAOSAUDIOINPUT_H

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

#include <QSocketNotifier>
#include <QIODevice>
#include <QElapsedTimer>
#include <QTimer>

#include <sys/audioio.h>

QT_BEGIN_NAMESPACE

class QLaylaOSAudioSource : public QPlatformAudioSource
{
    Q_OBJECT

public:
    explicit QLaylaOSAudioSource(QAudioDevice deviceInfo, const QAudioFormat &, QObject *parent);
    ~QLaylaOSAudioSource();

    void start(QIODevice*) override;
    QIODevice* start() override;
    void stop() override;
    void reset() override;
    void suspend() override;
    void resume() override;
    qsizetype bytesReady() const override;
    void setBufferSize(qsizetype ) override;
    qsizetype bufferSize() const  override;
    qint64 processedUSecs() const override;
    QAudio::State state() const override;

private slots:
    void userFeed();
    bool deviceReady();

private:
    friend class InputPrivate;

    bool open();
    void close();
    qint64 read(char *data, qint64 len);
    void changeState(QAudio::State state, QAudio::Error error);

    int m_fd;
    QIODevice *m_audioSource;
    QSocketNotifier *m_notifier;

    QAudio::State m_state;

    qint64 m_bytesRead;
    qint64 m_elapsedTimeOffset;
    qint64 m_totalTimeValue;

    int m_bytesAvailable;
    int m_bufferSize;
    int m_periodSize;

    bool m_pullMode;
};

class InputPrivate : public QIODevice
{
    Q_OBJECT
public:
    InputPrivate(QLaylaOSAudioSource *audio);

    qint64 readData(char *data, qint64 len) override;
    qint64 writeData(const char *data, qint64 len) override;

    qint64 bytesAvailable() const override;

    bool isSequential() const override;

    void trigger();

private:
    QLaylaOSAudioSource *m_audioDevice;
};

QT_END_NAMESPACE

#endif
