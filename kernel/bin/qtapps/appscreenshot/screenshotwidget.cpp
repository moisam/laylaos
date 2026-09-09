#include "screenshotwidget.h"
#include <QThread>
#include <QPixmap>
#include <QPainter>
#include <QIcon>

#ifdef __laylaos__
#include <gui/client/window.h>
#include <gui/client/screenshot.h>
#include <gui/kbd.h>
#include <emmintrin.h>
#endif

#define APPICON_PATH            "/usr/share/gui/icons/screenshot.png"


enum class IconType { Screen, Window, Selection };

inline QIcon createQuickIcon(IconType type)
{
    QPixmap pixmap(48, 48);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Set style: Dark grey, 3 pixels thick borders, rounded edges
    QPen pen(QColor("#2d2d2d"), 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if(type == IconType::Screen)
    {
        // Draw the Monitor Display box
        painter.drawRoundedRect(6, 8, 36, 24, 4, 4);
        // Draw the Stand / Base
        painter.drawLine(24, 32, 24, 38);
        painter.drawLine(16, 38, 32, 38);
    } 
    else if(type == IconType::Window)
    {
        // Draw the main Application window frame
        painter.drawRoundedRect(6, 8, 36, 32, 4, 4);
        // Draw the top title bar line divider
        painter.drawLine(6, 16, 42, 16);
    } 
    else if(type == IconType::Selection)
    {
        // Change pen style to Dotted/Dashed for selection area
        pen.setStyle(Qt::DashLine);
        pen.setWidth(2);
        painter.setPen(pen);
        painter.drawRect(14, 14, 28, 28);

        // Draw solid background shadow window
        pen.setStyle(Qt::SolidLine);
        painter.setPen(pen);
        painter.drawRoundedRect(6, 6, 24, 24, 2, 2);
    }

    painter.end();
    return QIcon(pixmap);
}

ScreenshotWidget::ScreenshotWidget(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("Screenshot");
    setWindowIcon(QIcon(APPICON_PATH));
    setFixedSize(200, 160);
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);

    auto *layout = new QVBoxLayout(this);

    auto *btnFull = new QPushButton("Grab Full Screen", this);
    btnFull->setIcon(createQuickIcon(IconType::Screen));
    btnFull->setIconSize(QSize(48, 48));

    connect(btnFull, &QPushButton::clicked, this, [this]() { captureFullScreen(); });

#ifdef __laylaos__
    auto *btnWindow = new QPushButton("Grab Active Window", this);
    btnWindow->setIcon(createQuickIcon(IconType::Window));
    btnWindow->setIconSize(QSize(48, 48));

    connect(btnWindow, &QPushButton::clicked, this, [this]() { captureActiveWindow(); });
#endif

    auto *btnSelection = new QPushButton("Grab Selection", this);
    btnSelection->setIcon(createQuickIcon(IconType::Selection));
    btnSelection->setIconSize(QSize(48, 48));

    connect(btnSelection, &QPushButton::clicked, this, [this]() { captureSelection(); });

    layout->addWidget(btnFull);

#ifdef __laylaos__
    layout->addWidget(btnWindow);
#endif

    layout->addWidget(btnSelection);

}

void ScreenshotWidget::processCapturedPixmap(const QPixmap &pixmap)
{
    if(pixmap.isNull()) return;

    PreviewDialog preview(pixmap, this);
    preview.exec();
}

void ScreenshotWidget::captureFullScreen()
{
    this->hide();
    QThread::msleep(250); 
    QGuiApplication::processEvents();

    QScreen *screen = QGuiApplication::primaryScreen();
    if(screen)
    {
        QPixmap screenshot = screen->grabWindow(0);
        this->show();
        processCapturedPixmap(screenshot);
    }
}

void ScreenshotWidget::captureSelection()
{
    this->hide();
    QThread::msleep(300);
    QGuiApplication::processEvents();

    auto *overlay = new SelectionOverlay([this](const QPixmap& snappedImage)
    {
        this->processCapturedPixmap(snappedImage);
    });

    overlay->setAttribute(Qt::WA_DeleteOnClose);
    overlay->captureBackground();
    overlay->show();

    connect(overlay, &QWidget::destroyed, this, [this]()
    {
        this->show();
    });
}


#ifdef __laylaos__

static void rgba_to_argb_sse(uint32_t *p, int width, int height)
{
    size_t total_pixels = (size_t)width * (size_t)height;
    size_t x = 0;

    __m128i v_alpha_mask = _mm_set1_epi32(0xFF000000);

    for(; x <= total_pixels - 4; x += 4)
    {
        __m128i pixels = _mm_loadu_si128((const __m128i *)&p[x]);
        __m128i shifted_pixels = _mm_srli_epi32(pixels, 8);
        __m128i final_argb = _mm_or_si128(shifted_pixels, v_alpha_mask);
        _mm_storeu_si128((__m128i *)&p[x], final_argb);
    }

    for(; x < total_pixels; x++)
    {
        p[x] = (p[x] >> 8) | 0xff000000;
    }
}

void ScreenshotWidget::captureActiveWindow()
{
    // Hide us so the server can cycle back to the previously active window
    this->hide();

    // Give server time to shift focus back
    QThread::msleep(350); 
    QGuiApplication::processEvents();

    QScreen *screen = QGuiApplication::primaryScreen();
    if(!screen) return;

    winid_t winid = get_input_focus();
    if(winid == 0) return;

    struct window_attribs_t attribs;
    if(!get_win_attribs(winid, &attribs)) return;

    const QImage::Format format = QImage::Format_ARGB32;
    uint32_t *rawbuf;
    int hasframe = !(attribs.flags & WINDOW_NODECORATION);
    int winw = attribs.w;
    int winh = attribs.h;

    if(hasframe)
    {
        winw += (2 * WINDOW_BORDERWIDTH);
        winh += WINDOW_TITLEHEIGHT + WINDOW_BORDERWIDTH;
    }

    rawbuf = screenshot_get(winid, 0, 0, winw, winh);

    if(!rawbuf) return;

    /* convert from native RGBA to ARGB pixels */
    rgba_to_argb_sse(rawbuf, winw, winh);

    QImage image((uchar*)rawbuf, winw, winh, winw * 4, format, [](void* info)
        {
            screenshot_free((uint32_t *)info);
        }, 
        rawbuf // Passed into the lambda as the 'info' pointer
    );

    QPixmap screenshot = QPixmap::fromImage(image);

    //QPixmap screenshot = screen->grabWindow(0, attribs.x, attribs.y, attribs.w, attribs.h);
    this->show();
    processCapturedPixmap(screenshot);
}

#endif

