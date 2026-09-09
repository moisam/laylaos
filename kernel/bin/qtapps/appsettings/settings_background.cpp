#include "settings_base.h"
#include "settings_single_instance_lock.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QFileDialog>
#include <QColorDialog>
#include <QMessageBox>

#include "../../desktop/include/event.h"
#include "../../desktop/desktop/desktop.h"

#define APPICON_PATH            "/usr/share/gui/icons/image2.png"

const char *LOCK_FILE = "/tmp/desktop-settings-bg.lock";

class BackgroundWindow : public SettingsBaseWindow
{
public:
    explicit BackgroundWindow(QWidget *parent = nullptr);

private:
    void updatePreviewSwatch(const QString& hexColor);
    void sendWallpaperRequest();
    void sendColorRequest(uint32_t color);
    void updatePathDisplay(const QString& fullPath);
    void getDesktopWinid();

    QWidget *m_colorPreview = nullptr;
    QComboBox *m_aspectCombo = nullptr;
    QLabel *m_pathDisplayLabel = nullptr;

    QString m_selectedColorHex = "#16A085";
    QString m_currentImagePath = "";
    winid_t m_desktopWinid = 0;

    const std::vector<int> m_aspectTokens =
    {
        DESKTOP_BACKGROUND_CENTERED,
        DESKTOP_BACKGROUND_TILES,
        DESKTOP_BACKGROUND_SCALED,
        DESKTOP_BACKGROUND_STRETCHED,
        DESKTOP_BACKGROUND_ZOOMED,
    };
};

void BackgroundWindow::getDesktopWinid()
{
    size_t tmpsz = sizeof(struct event_t);
    std::vector<uint8_t> tmp(tmpsz);
    struct event_t *ev = reinterpret_cast<struct event_t *>(tmp.data());
    struct event_t *ev2;
    uint32_t seqid;

    if(m_desktopWinid != 0)
    {
        return;
    }

    seqid = __next_seqid();
    ev->seqid = seqid;
    ev->type = REQUEST_GET_ROOT_WINID;
    ev->src = TO_WINID(GLOB.mypid, 0);
    ev->dest = GLOB.server_winid;

    //qDebug() << "getDesktopWinid: serverfd" << GLOB.serverfd << ", serverid" << GLOB.server_winid << ", seqid" << seqid;

    direct_write(GLOB.serverfd, reinterpret_cast<void *>(ev), tmpsz);

    if(!(ev2 = get_server_reply(seqid)))
    {
        return;
    }

    if(ev2->type == EVENT_ERROR)
    {
        free(ev2);
        return;
    }

    m_desktopWinid = ev2->winattr.winid;
    free(ev2);
}

BackgroundWindow::BackgroundWindow(QWidget *parent)
    : SettingsBaseWindow("Desktop Background", parent)
{
    setFixedSize(400, 210);
    setWindowIcon(QIcon(APPICON_PATH));
    getDesktopWinid();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(12);

    auto *imageTitle = new QLabel("Wallpaper Background:", this);
    imageTitle->setStyleSheet("QLabel { font-weight: bold; font-size: 13px; color: #0D6C60; border-bottom: 1px solid #2D2D2D; padding-bottom: 4px; }");
    layout->addWidget(imageTitle);

    auto *pathLayout = new QHBoxLayout();
    auto *selectBtn = new QPushButton("Browse...", this);

    m_pathDisplayLabel = new QLabel("No image selected", this);
    m_pathDisplayLabel->setStyleSheet("color: #7F8C8D; font-style: italic; border: 1px solid #2D2D2D; padding: 4px; border-radius: 3px;");
    m_pathDisplayLabel->setFixedWidth(200);

    pathLayout->addWidget(new QLabel("Image path:", this));
    pathLayout->addWidget(m_pathDisplayLabel);
    pathLayout->addWidget(selectBtn, 1);
    layout->addLayout(pathLayout);

    auto *aspectLayout = new QHBoxLayout();
    auto *aspectLabel = new QLabel("Aspect Ratio:", this);
    m_aspectCombo = new QComboBox(this);
    m_aspectCombo->addItems({"Centered (Default)", "Tiled", "Scaled", "Stretched", "Zoomed"});
    m_aspectCombo->setCurrentIndex(0);

    aspectLayout->addWidget(aspectLabel);
    aspectLayout->addWidget(m_aspectCombo, 1);
    layout->addLayout(aspectLayout);
    layout->addSpacing(5);

    auto *colorTitle = new QLabel("Solid Color:", this);
    colorTitle->setStyleSheet("QLabel { font-weight: bold; font-size: 13px; color: #0D6C60; border-bottom: 1px solid #2D2D2D; padding-bottom: 4px; }");
    layout->addWidget(colorTitle);

    auto *colorLayout = new QHBoxLayout();
    auto *pickColorBtn = new QPushButton("Choose color...", this);

    m_colorPreview = new QWidget(this);
    m_colorPreview->setFixedSize(36, 24);
    updatePreviewSwatch(m_selectedColorHex);

    colorLayout->addWidget(new QLabel("Active Color:", this));
    colorLayout->addWidget(m_colorPreview);
    colorLayout->addWidget(pickColorBtn, 1);
    layout->addLayout(colorLayout);

    layout->addStretch(1);

    QObject::connect(selectBtn, &QPushButton::clicked, [this]()
    {
        QString file = QFileDialog::getOpenFileName(this, "Select Desktop Wallpaper", BACKGROUNDS_DIR_PATH, "Images (*.png *.jpg *.jpeg)");

        if(!file.isEmpty())
        {
            m_currentImagePath = file;
            updatePathDisplay(file);
            sendWallpaperRequest();
        }
    });

    QObject::connect(m_aspectCombo, &QComboBox::currentIndexChanged, [this](int index)
    {
        if(!m_currentImagePath.isEmpty())
        {
            sendWallpaperRequest();
        }
    });

    QObject::connect(pickColorBtn, &QPushButton::clicked, [this]()
    {
        QColor initialColor(m_selectedColorHex);
        QColor chosenColor = QColorDialog::getColor(initialColor, this, "Choose Background Color");

        if(chosenColor.isValid())
        {
            m_selectedColorHex = chosenColor.name(QColor::HexRgb).toUpper();
            updatePreviewSwatch(m_selectedColorHex);

            m_currentImagePath = "";
            m_pathDisplayLabel->setText("No image selected");
            m_pathDisplayLabel->setStyleSheet("color: #7F8C8D; font-style: italic; border: 1px solid #2D2D2D; padding: 4px; border-radius: 3px;");

            uint32_t rgba = (chosenColor.red() << 24) |
                            (chosenColor.green() << 16) |
                            (chosenColor.blue() << 8) | 0xFF;
            sendColorRequest(rgba);
        }
    });
}

void BackgroundWindow::updatePathDisplay(const QString& fullPath)
{
    if(!m_pathDisplayLabel) return;

    QFontMetrics metrics(m_pathDisplayLabel->font());
    int availableWidth = m_pathDisplayLabel->width() - 8;
    QString elidedPath = metrics.elidedText(fullPath, Qt::ElideLeft, availableWidth);

    m_pathDisplayLabel->setText(elidedPath);

    m_pathDisplayLabel->setStyleSheet("color: #1F1F1F; font-weight: bold; border: 1px solid #3A3A3A; padding: 4px; border-radius: 3px;");
}

/*
 * Tell the desktop task to set background to the given color.
 */
void BackgroundWindow::sendColorRequest(uint32_t color)
{
    size_t tmpsz = sizeof(struct event_desktop_bg_t) + sizeof(uint32_t);
    std::vector<uint8_t> tmp(tmpsz);
    struct event_desktop_bg_t *evres = 
            reinterpret_cast<struct event_desktop_bg_t *>(tmp.data());

    if(m_desktopWinid == 0)
    {
        QMessageBox::critical(this, "Error", "Failed to get desktop window id.");
        return;
    }

    evres->seqid = __next_seqid();
    evres->type = REQUEST_SET_DESKTOP_BACKGROUND;
    evres->src = TO_WINID(GLOB.mypid, 0);
    evres->dest = m_desktopWinid;
    evres->datasz = sizeof(uint32_t);
    *((uint32_t *)evres->data) = color;
    evres->bg_is_image = 0;
    evres->valid_reply = 1;         // so the desktop app would not filter it out

    direct_write(GLOB.serverfd, reinterpret_cast<void *>(evres), tmpsz);
}


/*
 * Tell the desktop task to set background to the given image.
 */
void BackgroundWindow::sendWallpaperRequest()
{
    if(m_currentImagePath.isEmpty()) return;

    int activeIndex = m_aspectCombo->currentIndex();
    int aspectToken = DESKTOP_BACKGROUND_CENTERED;

    if(activeIndex >= 0 && activeIndex < static_cast<int>(m_aspectTokens.size()))
    {
        aspectToken = m_aspectTokens[activeIndex];
    }
    else
    {
        qDebug() << "Invalid aspect index:" << activeIndex;
        QMessageBox::critical(this, "Error", "Please select an aspect ratio from the list.");
        return;
    }

    QByteArray byteBuffer = m_currentImagePath.toUtf8();
    const char *c_str = byteBuffer.constData();
    size_t pathlen = byteBuffer.size();
    size_t tmpsz = sizeof(struct event_desktop_bg_t) + pathlen + 1;
    std::vector<uint8_t> tmp(tmpsz);
    struct event_desktop_bg_t *evres = 
            reinterpret_cast<struct event_desktop_bg_t *>(tmp.data());

    if(m_desktopWinid == 0)
    {
        QMessageBox::critical(this, "Error", "Failed to get desktop window id.");
        return;
    }

    evres->seqid = __next_seqid();
    evres->src = TO_WINID(GLOB.mypid, 0);
    evres->dest = m_desktopWinid;
    evres->datasz = pathlen + 1;
    evres->bg_is_image = 1;
    evres->type = REQUEST_SET_DESKTOP_BACKGROUND;
    evres->bg_image_aspect = aspectToken;
    evres->valid_reply = 1;         // so the desktop app would not filter it out
    memcpy(evres->data, c_str, pathlen);
    evres->data[pathlen] = '\0';

    direct_write(GLOB.serverfd, reinterpret_cast<void *>(evres), tmpsz);
}

void BackgroundWindow::updatePreviewSwatch(const QString& hexColor)
{
    if(!m_colorPreview) return;

    m_colorPreview->setStyleSheet(
        QString("QWidget { background-color: %1; border: 1px solid #3A3A3A; border-radius: 3px; }").arg(hexColor)
    );
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Check for a running instance
    if(!SingleInstanceLock::grabSingleInstanceLock(QString(LOCK_FILE)))
    {
        return 0;
    }

    BackgroundWindow bgWindow;
    bgWindow.show();

    // Ensure the lock file is released when the window closes
    QObject::connect(&app, &QApplication::aboutToQuit, []()
    {
        SingleInstanceLock::releaseSingleInstanceLock(QString(LOCK_FILE));
    });

    return app.exec();
}

