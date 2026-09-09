#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QFile>
#include <unistd.h>
#include <sys/types.h>

#define ICON_PATH               "/usr/share/gui/icons"
#define APPICON_PATH            ICON_PATH "/settings.png"


class ControlPanelDashboard : public QWidget
{
public:
    explicit ControlPanelDashboard(QWidget *parent = nullptr);

private:
    void launchBinaryAsync(const char *binaryPath);
    QPushButton *createPixmapTile(const QString& title, const QString& iconPath);
};


ControlPanelDashboard::ControlPanelDashboard(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("System Settings");
    setFixedSize(460, 300);
    setWindowIcon(QIcon(APPICON_PATH));

    setStyleSheet(
        "QWidget { font-family: 'Sans-Serif'; font-size: 12px; }"
        "QLabel { background: transparent; font-weight: bold; font-size: 13px; color: #0D6C60; border-bottom: 1px solid #2D2D2D; padding-bottom: 4px; }"
        "QPushButton { border: 1px solid #2E2E2E; border-radius: 6px; text-align: center; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { border-color: #16A085; background-color: #CDCFD4; }"
        "QPushButton:pressed { background-color: #16A085; }"
    );

    auto *masterLayout = new QVBoxLayout(this);
    masterLayout->setContentsMargins(20, 20, 20, 20);
    masterLayout->setSpacing(12);

    masterLayout->addWidget(new QLabel("Appearance", this));
    auto *appGrid = new QGridLayout();
    appGrid->setSpacing(15);

    auto *bgBtn = createPixmapTile("Backgrounds", ICON_PATH "/image2.png");
    auto *themeBtn = createPixmapTile("Color Themes", ICON_PATH "/theme.png");
    auto *mouseBtn = createPixmapTile("Mouse Pointers", ICON_PATH "/mouse.png");

    appGrid->addWidget(bgBtn, 0, 0);
    appGrid->addWidget(themeBtn, 0, 1);
    appGrid->addWidget(mouseBtn, 0, 2);
    masterLayout->addLayout(appGrid);

    masterLayout->addSpacing(20);

    masterLayout->addWidget(new QLabel("Info & Diagnostics", this));
    auto *infoGrid = new QGridLayout();
    infoGrid->setSpacing(15);

    auto *infoBtn = createPixmapTile("System Info", ICON_PATH "/settings2.png");
    auto *dispBtn = createPixmapTile("Display", ICON_PATH "/monitor.png");

    infoGrid->addWidget(infoBtn, 0, 0);
    infoGrid->addWidget(dispBtn, 0, 1);
    masterLayout->addLayout(infoGrid);

    masterLayout->addStretch(1);

    QObject::connect(bgBtn, &QPushButton::clicked, [this]() { launchBinaryAsync("/bin/desktop/desktop-settings-bg"); });
    QObject::connect(themeBtn, &QPushButton::clicked, [this]() { launchBinaryAsync("/bin/desktop/desktop-settings-theme"); });
    QObject::connect(mouseBtn, &QPushButton::clicked, [this]() { launchBinaryAsync("/bin/desktop/appcurview"); });
    QObject::connect(infoBtn, &QPushButton::clicked, [this]() { launchBinaryAsync("/bin/desktop/desktop-settings-sysinfo"); });
    QObject::connect(dispBtn, &QPushButton::clicked, [this]() { launchBinaryAsync("/bin/desktop/desktop-settings-display"); });
}

void ControlPanelDashboard::launchBinaryAsync(const char *binaryPath)
{
    pid_t pid = fork();

    if(pid == 0)
    {
        char *const childArgv[] = { const_cast<char*>(binaryPath), nullptr };
        execv(binaryPath, childArgv);
        _exit(1);
    }
}

QPushButton *ControlPanelDashboard::createPixmapTile(const QString& title, const QString& iconPath)
{
    auto *btn = new QPushButton(this);
    btn->setFixedSize(110, 90);

    auto *tileLayout = new QVBoxLayout(btn);
    tileLayout->setContentsMargins(6, 8, 6, 8);
    tileLayout->setSpacing(4);
    tileLayout->setAlignment(Qt::AlignCenter);

    auto *iconLabel = new QLabel(btn);
    iconLabel->setFixedSize(32, 32);
    iconLabel->setScaledContents(true);

    QPixmap pixmap;

    if(QFile::exists(iconPath))
    {
        QImage img(iconPath);
        if(img.format() != QImage::Format_ARGB32_Premultiplied)
        {
            img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        }
        pixmap = QPixmap::fromImage(img);
    }
    else
    {
        pixmap = QPixmap(32, 32);
        pixmap.fill(QColor("#16A085"));
    }

    iconLabel->setPixmap(pixmap);

    auto *textLabel = new QLabel(title, btn);
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->setStyleSheet("font-weight: bold; font-size: 11px; border: none; background: transparent;");

    tileLayout->addWidget(iconLabel, 0, Qt::AlignCenter);
    tileLayout->addWidget(textLabel, 0, Qt::AlignCenter);

    return btn;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ControlPanelDashboard panel;
    panel.show();
    return app.exec();
}

