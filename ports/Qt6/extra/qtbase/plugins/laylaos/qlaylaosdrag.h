// Copyright (C) 2026 Mohammed Isam
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#ifndef QLAYLAOSDRAG_H
#define QLAYLAOSDRAG_H

#include <qpa/qplatformdrag.h>
#include <private/qsimpledrag_p.h>
#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

class QLaylaOSDrag : public QBasicDrag
{
    Q_OBJECT
public:
    QLaylaOSDrag();
    ~QLaylaOSDrag() override;

    void handleDragResponse(int response);

protected:
    void startDrag() override;
    void cancel() override;
    void endDrag() override;

    void move(const QPoint &globalPos, Qt::MouseButtons b, Qt::KeyboardModifiers mods) override;
    void drop(const QPoint &globalPos, Qt::MouseButtons b, Qt::KeyboardModifiers mods) override;
};

QT_END_NAMESPACE

#endif // QLAYLAOSDRAG_H
