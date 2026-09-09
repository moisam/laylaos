// Copyright (C) 2026 Mohammed Isam
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "qlaylaosdrag.h"
#include <QtGui/QDrag>
#include <QtCore/QMimeData>
#include <QtCore/QFile>
#include <QtCore/QDataStream>
#include <QtCore/QUuid>
#include <qpa/qplatformdrag.h>

#include <gui/gui.h>
#include <gui/keys.h>
#include <gui/kbd.h>
//#include <gui/mouse.h>
#include <gui/client/dragndrop.h>

#define GLOB            __global_gui_data

QT_BEGIN_NAMESPACE

static mouse_buttons_t qtMouseButtonsToNative(Qt::MouseButtons qtb)
{
    mouse_buttons_t lb = 0;

    if (qtb.testFlag(Qt::LeftButton)) lb |= MOUSE_LBUTTON_DOWN;
    if (qtb.testFlag(Qt::RightButton)) lb |= MOUSE_RBUTTON_DOWN;
    if (qtb.testFlag(Qt::MiddleButton)) lb |= MOUSE_MBUTTON_DOWN;

    return lb;
}

static char qtModsToNative(Qt::KeyboardModifiers qtmods)
{
    char modkeys = 0;

    if (qtmods & Qt::ShiftModifier) modkeys |= MODIFIER_MASK_SHIFT;
    if (qtmods & Qt::AltModifier) modkeys |= MODIFIER_MASK_ALT;
    if (qtmods & Qt::ControlModifier) modkeys |= MODIFIER_MASK_CTRL;

    return modkeys;
}

QLaylaOSDrag::QLaylaOSDrag()
    : QBasicDrag()
{
}

QLaylaOSDrag::~QLaylaOSDrag()
{
}

void QLaylaOSDrag::startDrag()
{
    //qDebug() << "QLaylaOSDrag::startDrag:";
    QBasicDrag::startDrag();

    QDrag *currentDrag = drag();

    if (!currentDrag || !currentDrag->mimeData()) return;

    QStringList formats = currentDrag->mimeData()->formats();
    int mimeCount = formats.size();
    //qDebug() << "QLaylaOSDrag::startDrag: mimeCount " << mimeCount;

    if (mimeCount == 0) {
        return;
    }

    // allocate (mimeCount + 1) slots to ensure room for a final NULL pointer sentinel
    std::vector<char*> cStringArray(mimeCount + 1, nullptr);

    std::vector<QByteArray> utf8Buffers;
    utf8Buffers.reserve(mimeCount);

    for (int i = 0; i < mimeCount; ++i) {
        //qDebug() << "QLaylaOSDrag::startDrag: [" << i << "] " << formats[i].toUtf8();
        utf8Buffers.push_back(formats[i].toUtf8());

        cStringArray[i] = const_cast<char*>(utf8Buffers[i].constData());
    }
    
    cStringArray[mimeCount] = nullptr;

    char **nativeMimeArray = cStringArray.data();

    drag_start(nativeMimeArray, mimeCount);
}

void QLaylaOSDrag::move(const QPoint &globalPos, Qt::MouseButtons b, Qt::KeyboardModifiers mods)
{
    //qDebug() << "QLaylaOSDrag::move:";
    QBasicDrag::moveShapedPixmapWindow(globalPos);

    drag_move(globalPos.x(), globalPos.y(), qtMouseButtonsToNative(b), qtModsToNative(mods));
    //qDebug() << "QLaylaOSDrag::move: focus " << window_get_under_mouse();
}

void QLaylaOSDrag::drop(const QPoint &globalPos, Qt::MouseButtons b, Qt::KeyboardModifiers mods)
{
    //qDebug() << "QLaylaOSDrag::drop:";
    QDrag *currentDrag = drag();

    if (!currentDrag || !currentDrag->mimeData()) {
        QBasicDrag::drop(globalPos, b, mods);
        return;
    }

    QMimeData *mimeData = currentDrag->mimeData();
    QStringList formats = mimeData->formats();

    QString tempPath = QString("/tmp/dnd_%1.dat").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    QFile file(tempPath);

    if (file.open(QIODevice::WriteOnly)) {
        QDataStream out(&file);
        out.setByteOrder(QDataStream::LittleEndian);

        // Serialize the total number of formats being exported
        out << static_cast<quint32>(formats.size());

        // Serialize each MIME format name and its corresponding raw payload bytes
        for (const QString &format : formats) {
            QByteArray formatNameBytes = format.toUtf8();
            QByteArray payloadBytes = mimeData->data(format);

            out << static_cast<quint32>(formatNameBytes.size());
            out.writeRawData(formatNameBytes.constData(), formatNameBytes.size());

            out << static_cast<quint32>(payloadBytes.size());
            out.writeRawData(payloadBytes.constData(), payloadBytes.size());
        }

        file.close();

        // Notify the window server that a drop occurred
        QByteArray nativePath = tempPath.toUtf8();

        drag_drop(globalPos.x(), globalPos.y(), qtMouseButtonsToNative(b), qtModsToNative(mods), nativePath.constData());
    }

    QBasicDrag::drop(globalPos, b, mods);
}

void QLaylaOSDrag::cancel()
{
    //qDebug() << "QLaylaOSDrag::cancel:";
    QBasicDrag::cancel();

    drag_cancel();
}

void QLaylaOSDrag::endDrag()
{
    //qDebug() << "QLaylaOSDrag::endDrag:";
    QBasicDrag::endDrag();
}

void QLaylaOSDrag::handleDragResponse(int response)
{
    setCanDrop(response != DRAG_RESPONSE_REJECT);
}

QT_END_NAMESPACE
