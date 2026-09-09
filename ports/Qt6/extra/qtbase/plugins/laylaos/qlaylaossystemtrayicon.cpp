// Copyright (C) 2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "qlaylaossystemtrayicon.h"

#include <QtGui/qguiapplication.h>
#include <QtGui/qpixmap.h>
#include <QtCore/qbuffer.h>
#include <QtCore/qdebug.h>
#include <qpa/qwindowsysteminterface.h>

#include <QtGui/private/qguiapplication_p.h>

#include <gui/rect.h>
#include <gui/window-defs.h>
#include <gui/client/window.h>
#include <gui/client/systray.h>

QT_BEGIN_NAMESPACE

// Match the winid of the dummy window to the instances
struct QLaylaOSSystemTrayIconEntry
{
    struct window_t *win;
    QLaylaOSSystemTrayIcon *trayIcon;
};

using LaylaOSTrayIconEntries = QList<QLaylaOSSystemTrayIconEntry>;

Q_GLOBAL_STATIC(LaylaOSTrayIconEntries, laylaOSTrayIconEntries)

static int indexOfWinid(struct window_t *win)
{
    const LaylaOSTrayIconEntries *entries = laylaOSTrayIconEntries();
    for (int i = 0, size = entries->size(); i < size; ++i) {
        if (entries->at(i).win == win)
            return i;
    }
    return -1;
}

void QLaylaOSSystemTrayIcon::dispatchClientEvent(struct event_t *ev)
{
    struct window_t *lwin;
    //qDebug() << ev->dest << win_for_winid(ev->dest);
    if ((lwin = win_for_winid(ev->dest)) == nullptr) return;

    const int index = indexOfWinid(lwin);
    //qDebug() << ev->dest << index;
    if (index >= 0) laylaOSTrayIconEntries()->at(index).trayIcon->clientEvent(ev);
}

void QLaylaOSSystemTrayIcon::clientEvent(struct event_t *ev)
{
    switch(ev->type)
    {
        case EVENT_SYSTRAY_CLICK:
        {
            int globalX = ev->mouse.x;
            int globalY = ev->mouse.y;

            //QCursor::setPos(globalX, globalY);

            if (ev->mouse.buttons == MOUSE_RBUTTON_DOWN) {
                QPlatformScreen* currentPlatformScreen = nullptr;

                if (QScreen* primaryScreen = QGuiApplication::primaryScreen()) {
                    currentPlatformScreen = primaryScreen->handle();
                }

                emit contextMenuRequested(QPoint(globalX, globalY), currentPlatformScreen);
                emit activated(Context);
            } else if (ev->mouse.buttons == MOUSE_MBUTTON_DOWN) {
                emit activated(MiddleClick);
            } else if (ev->mouse.buttons == MOUSE_LBUTTON_DOWN) {
                emit activated(Trigger);
            }

            break;
        }

        case EVENT_SYSTRAY_DOUBLE_CLICK:
            emit activated(DoubleClick);
            break;
    }
}

QLaylaOSSystemTrayIcon::QLaylaOSSystemTrayIcon()
{
}

QLaylaOSSystemTrayIcon::~QLaylaOSSystemTrayIcon()
{
    ensureCleanup();
}

QPlatformMenu *QLaylaOSSystemTrayIcon::createMenu() const
{
    return nullptr; 
}

void QLaylaOSSystemTrayIcon::updateMenu(QPlatformMenu *)
{
}

void QLaylaOSSystemTrayIcon::init()
{
    m_visible = true;

    if (ensureInstalled()) {
        systray_set_icon_visibility(m_win->winid, m_visible);

        if (!m_icon.isNull()) {
            updateIcon(m_icon);
        }
    }
}

void QLaylaOSSystemTrayIcon::cleanup()
{
    m_visible = false;
    ensureCleanup();
}

void QLaylaOSSystemTrayIcon::updateIcon(const QIcon &icon)
{
    m_icon = icon;

    if (icon.isNull()) return;

    if (ensureInstalled()) {
        QByteArray iconPixels = serializeIconAsPng(icon);
        systray_set_icon(m_win->winid, iconPixels.constData(), iconPixels.size());
    }
}

void QLaylaOSSystemTrayIcon::updateToolTip(const QString &tooltip)
{
    if (m_toolTip == tooltip) return;

    m_toolTip = tooltip;

    if (isInstalled()) {
        QByteArray byteBuffer = m_toolTip.toUtf8();
        const char *c_str = byteBuffer.constData();
        systray_set_tooltip(m_win->winid, c_str);
    }
}

QRect QLaylaOSSystemTrayIcon::geometry() const
{
    if (!isInstalled())
        return QRect();

    Rect rect;
    if (!systray_get_bounds(m_win->winid, &rect))
        return QRect();

    const QRect result = QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);

    return result;
}

bool QLaylaOSSystemTrayIcon::supportsMessages() const
{
    return true;
}

void QLaylaOSSystemTrayIcon::showMessage(const QString &title, const QString &msg, 
                                         const QIcon &icon, MessageIcon iconType, 
                                         int msecs)
{
    if (!isInstalled()) return;

    QByteArray titleBuffer = title.toUtf8();
    const char *title_c_str = titleBuffer.constData();

    QByteArray msgBuffer = msg.toUtf8();
    const char *msg_c_str = msgBuffer.constData();

    int typeInt = static_cast<int>(iconType);
    int duration = (msecs <= 0) ? 5000 : msecs;

    // Compress the icon to PNG bytes if a valid custom icon was provided
    if (!icon.isNull()) {
        QByteArray iconPixels = serializeIconAsPng(icon);

        systray_show_message(m_win->winid, title_c_str, msg_c_str,
                             iconPixels.constData(), iconPixels.size(), typeInt, duration);
    } else {
        systray_show_message(m_win->winid, title_c_str, msg_c_str, NULL, 0, typeInt, duration);
    }
}

QByteArray QLaylaOSSystemTrayIcon::serializeIconAsPng(const QIcon& icon)
{
    QPixmap pixmap = icon.pixmap(24, 24);

    QByteArray outputBuffer;
    QBuffer buffer(&outputBuffer);
    buffer.open(QIODevice::WriteOnly);

    // Save the 24x24 pixmap into the buffer as a compressed PNG
    pixmap.save(&buffer, "PNG");

    return outputBuffer; 
}

// Delay-install until an Icon exists
bool QLaylaOSSystemTrayIcon::ensureInstalled()
{
    if (!isInstalled()) {
        struct window_attribs_t attribs;

        attribs.gravity = 0;
        attribs.x = 0;
        attribs.y = 0;
        attribs.w = 1;
        attribs.h = 1;
        attribs.flags = WINDOW_SKIPTASKBAR;

        m_win = window_create(&attribs);

        if (Q_UNLIKELY(m_win == nullptr)) {
            qDebug() << "QLaylaOSSystemTrayIcon: failed to create window";
            return false;
        }

        QLaylaOSSystemTrayIconEntry entry{ m_win, this };
        laylaOSTrayIconEntries()->append(entry);

        if (!systray_add(m_win->winid)) {
            qDebug() << "QLaylaOSSystemTrayIcon: failed to add window";
            return false;
        }
    }

    return true;
}

void QLaylaOSSystemTrayIcon::ensureCleanup()
{
    if (isInstalled()) {
        const int index = indexOfWinid(m_win);
        if (index >= 0)
            laylaOSTrayIconEntries()->removeAt(index);
        systray_remove(m_win->winid);
        window_destroy(m_win);
        m_win = nullptr;
    }

    m_toolTip.clear();
}

QT_END_NAMESPACE
