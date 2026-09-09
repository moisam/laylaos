#include <QPainter>
#include <QGridLayout>
#include <QStyle>
#include <QKeyEvent>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <cmath>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "desktop_window.h"
#include "desktop_entry_parser.h"
#include "qlaylaosclientevents.h"

#define GLOB                        __global_gui_data


DesktopWindow::DesktopWindow(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint);

    // Allow this container to accept arrow keys input
    setFocusPolicy(Qt::StrongFocus);

    // Handle display size change
    QScreen *screen = QGuiApplication::primaryScreen();
    if(screen)
    {
        this->setGeometry(screen->geometry());

        QObject::connect(screen, &QScreen::geometryChanged, [this](const QRect& newGeometry)
        {
            this->setGeometry(newGeometry);
            this->update();
            this->loadEntries();
        });
    }
}

void DesktopWindow::setWallpaper(const QString& path, int aspect)
{
    if(path == nullptr) return;

    if(path != m_wallpaperPath)
    {
        m_wallpaperPath = path;
        m_wallpaper.load(path);
        m_backgroundIsImage = true;
    }

    m_wallpaperAspect = aspect;
    update();
}

void DesktopWindow::setWallpaperAspect(int aspect)
{
    m_wallpaperAspect = aspect;
    update();
}

void DesktopWindow::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);

    if(!m_backgroundIsImage || m_wallpaper.isNull())
    {
        painter.fillRect(rect(), m_backgroundColor);
        QWidget::paintEvent(event);
        return;
    }

    int scrW = width();
    int scrH = height();
    int imgW = m_wallpaper.width();
    int imgH = m_wallpaper.height();

    painter.fillRect(rect(), m_blackColor);

    switch(m_wallpaperAspect)
    {
        case DESKTOP_BACKGROUND_STRETCHED:
        {
            painter.drawPixmap(rect(), m_wallpaper);
            break;
        }

        case DESKTOP_BACKGROUND_TILES:
        {
            // QBrush natively supports seamless tiling textures
            QBrush brush(m_wallpaper);
            painter.fillRect(rect(), brush);
            break;
        }

        case DESKTOP_BACKGROUND_SCALED:
        {
            painter.fillRect(rect(), Qt::black);

            // Scale keeping aspect ratio, fitting inside screen bounds
            QPixmap scaledImg = m_wallpaper.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

            int x = (scrW - scaledImg.width()) / 2;
            int y = (scrH - scaledImg.height()) / 2;
            painter.drawPixmap(x, y, scaledImg);
            break;
        }

        case DESKTOP_BACKGROUND_ZOOMED:
        {
            QPixmap zoomedImg = m_wallpaper.scaled(size(), 
                                                   Qt::KeepAspectRatioByExpanding,
                                                   Qt::SmoothTransformation);

            int sx = (zoomedImg.width() - scrW) / 2;
            int sy = (zoomedImg.height() - scrH) / 2;
            
            painter.drawPixmap(rect(), zoomedImg, QRect(sx, sy, scrW, scrH));
            break;
        }

        case DESKTOP_BACKGROUND_CENTERED:
        default:
        {
            // Fill background canvas first
            painter.fillRect(rect(), Qt::black);

            // Calculate center offsets
            int x = (scrW - imgW) / 2;
            int y = (scrH - imgH) / 2;
            painter.drawPixmap(x, y, m_wallpaper);
            break;
        }
    }

    QWidget::paintEvent(event);
}

void DesktopWindow::loadEntries()
{
    for(DesktopIcon* icon : m_icons)
    {
        if(icon)
        {
            icon->hide();
            icon->deleteLater(); 
        }
    }

    m_icons.clear();
    m_currentSelected = nullptr;

    std::vector<DesktopEntry> entries = DesktopEntryParser::parseAllEntries();

    int currentX = 30, currentY = 30, spacingY = 130;
    int screenBottomThreshold = height() - 150;

    for(const DesktopEntry& entry : entries)
    {
        if (!entry.showOnDesktop) continue;

        QString actualIconPath = DesktopEntryParser::resolveIconPath(entry.iconPath, entry.iconName);
        auto* iconWidget = new DesktopIcon(entry.name, entry.command, actualIconPath, this);
        iconWidget->move(currentX, currentY);
        iconWidget->show();

        m_icons.push_back(iconWidget);

        currentY += spacingY;

        if(currentY > screenBottomThreshold)
        {
            currentY = 30;
            currentX += 120;
        }
    }

    //if(!m_icons.empty()) setActiveSelection(m_icons[0]);
}

void DesktopWindow::setActiveSelection(DesktopIcon *selectedIcon)
{
    // Clear old selection
    if(m_currentSelected)
    {
        m_currentSelected->setSelected(false);
    }

    m_currentSelected = selectedIcon;

    if(m_currentSelected)
    {
        m_currentSelected->setSelected(true);
    }
}

void DesktopWindow::mousePressEvent(QMouseEvent *event)
{
    // If the user clicks raw desktop space, drop active selection
    if(m_currentSelected)
    {
        m_currentSelected->setSelected(false);
        m_currentSelected = nullptr;
    }

    setFocus(); // Re-grab focus
}

void DesktopWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if(m_currentSelected)
        {
            m_currentSelected->launch();
        }

        return;
    }

    switch(event->key())
    {
        case Qt::Key_Up:    navigateSpatially(0, -1); break;
        case Qt::Key_Down:  navigateSpatially(0, 1);  break;
        case Qt::Key_Left:  navigateSpatially(-1, 0); break;
        case Qt::Key_Right: navigateSpatially(1, 0);  break;
        default: QWidget::keyPressEvent(event);       break;
    }
}

void DesktopWindow::navigateSpatially(int dx, int dy)
{
    if(m_icons.empty()) return;
    
    // If nothing was selected, fallback to highlight node zero
    if(!m_currentSelected)
    {
        setActiveSelection(m_icons[0]);
        return;
    }

    QPoint curCenter = m_currentSelected->geometry().center();
    DesktopIcon *bestMatch = nullptr;
    double minMetric = std::numeric_limits<double>::max();

    for(DesktopIcon *icon : m_icons)
    {
        if(icon == m_currentSelected) continue;

        QPoint targetCenter = icon->geometry().center();
        int deltaX = targetCenter.x() - curCenter.x();
        int deltaY = targetCenter.y() - curCenter.y();

        // Use dot-product screening to check if the target icon is in the correct directional hemisphere
        bool correctDir = false;
        if(dx > 0 && deltaX > 20 && std::abs(deltaY) < std::abs(deltaX) * 1.5) correctDir = true;
        if(dx < 0 && deltaX < -20 && std::abs(deltaY) < std::abs(deltaX) * 1.5) correctDir = true;
        if(dy > 0 && deltaY > 20 && std::abs(deltaX) < std::abs(deltaY) * 1.5) correctDir = true;
        if(dy < 0 && deltaY < -20 && std::abs(deltaX) < std::abs(deltaY) * 1.5) correctDir = true;

        if(correctDir)
        {
            // Standard Euclidean distance algorithm metric check
            double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);

            if(distance < minMetric)
            {
                minMetric = distance;
                bestMatch = icon;
            }
        }
    }

    if(bestMatch)
    {
        setActiveSelection(bestMatch);
    }
}

void DesktopWindow::customEvent(QEvent *event)
{
    QEvent::Type type = event->type();

    if(type == static_cast<QEvent::Type>(LaylaOSCustomEventType))
    {
        auto *customEvt = static_cast<LaylaOSCustomEvent *>(event);
        struct event_t *ev = customEvt->m_ev;

        switch(ev->type)
        {
            case REQUEST_GET_DESKTOP_BACKGROUND:
                if(m_backgroundIsImage)
                {
                    if(m_wallpaperPath.isEmpty())
                    {
                        struct event_t ev2;
                        ev2.type = EVENT_DESKTOP_BACKGROUND_INFO;
                        ev2.seqid = ev->seqid;
                        ev2.src = TO_WINID(GLOB.mypid, 0);
                        ev2.dest = ev->src;
                        ev2.err._errno = EINVAL;
                        ev2.valid_reply = 0;
                        direct_write(GLOB.serverfd, (void *)&ev2, sizeof(ev2));
                        break;
                    }

                    QByteArray byteBuffer = m_wallpaperPath.toUtf8();
                    const char *c_str = byteBuffer.constData();
                    size_t pathlen = byteBuffer.size();
                    size_t tmpsz = sizeof(struct event_desktop_bg_t) + pathlen + 1;
                    std::vector<uint8_t> tmp(tmpsz);
                    struct event_desktop_bg_t *evres = 
                            reinterpret_cast<struct event_desktop_bg_t *>(tmp.data());

                    evres->src = TO_WINID(GLOB.mypid, 0);
                    evres->dest = ev->src;
                    evres->datasz = pathlen + 1;
                    evres->seqid = ev->seqid;
                    evres->bg_is_image = 1;
                    evres->type = EVENT_DESKTOP_BACKGROUND_INFO;
                    evres->valid_reply = 1;

                    memcpy(evres->data, c_str, pathlen);
                    evres->data[pathlen] = '\0';

                    direct_write(GLOB.serverfd, reinterpret_cast<void *>(evres), tmpsz);
                }
                else
                {
                    size_t tmpsz = sizeof(struct event_desktop_bg_t) + 
                                   sizeof(uint32_t);
                    std::vector<uint8_t> tmp(tmpsz);
                    struct event_desktop_bg_t *evres = 
                            reinterpret_cast<struct event_desktop_bg_t *>(tmp.data());

                    evres->src = TO_WINID(GLOB.mypid, 0);
                    evres->dest = ev->src;
                    evres->datasz = sizeof(uint32_t);
                    *((uint32_t *)evres->data) = (m_backgroundColor.red() << 24) |
                                                 (m_backgroundColor.green() << 16) |
                                                 (m_backgroundColor.blue() << 8) |
                                                 0xFF;
                    evres->seqid = ev->seqid;
                    evres->bg_is_image = 0;
                    evres->type = EVENT_DESKTOP_BACKGROUND_INFO;
                    evres->valid_reply = 1;

                    direct_write(GLOB.serverfd, reinterpret_cast<void *>(evres), tmpsz);
                }
                break;

            case REQUEST_SET_DESKTOP_BACKGROUND:
            {
                struct event_desktop_bg_t *evres =
                            (struct event_desktop_bg_t *)ev;

                if(evres->bg_is_image)
                {
                    m_wallpaperPath = QString(evres->data);
                    m_backgroundIsImage = true;
                    m_wallpaperAspect = evres->bg_image_aspect;

                    if(m_wallpaperAspect < DESKTOP_BACKGROUND_FIRST_ASPECT ||
                       m_wallpaperAspect > DESKTOP_BACKGROUND_LAST_ASPECT)
                    {
                        // invalid value given, use default
                        m_wallpaperAspect = DESKTOP_BACKGROUND_CENTERED;
                    }

                    m_wallpaper.load(m_wallpaperPath);
                }
                else
                {
                    uint32_t rawcolor = *((uint32_t *)evres->data);
                    m_backgroundColor = QColor((rawcolor >> 24) & 0xFF,
                                               (rawcolor >> 16) & 0xFF,
                                               (rawcolor >> 8 ) & 0xFF,
                                               (rawcolor >> 0 ) & 0xFF);
                    m_backgroundIsImage = false;
                }

                update();
                break;
            }
        }

        free(ev);
    }
}

