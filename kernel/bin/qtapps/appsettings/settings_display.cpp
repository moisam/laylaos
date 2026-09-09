#include "settings_base.h"
#include "settings_single_instance_lock.h"
#include <QVBoxLayout>
#include <QLabel>

#define APPICON_PATH            "/usr/share/gui/icons/monitor.png"

const char *LOCK_FILE = "/tmp/desktop-settings-display.lock";

class DisplayInfoWindow : public SettingsBaseWindow
{
public:
    explicit DisplayInfoWindow(QWidget *parent = nullptr);
};

DisplayInfoWindow::DisplayInfoWindow(QWidget *parent) : SettingsBaseWindow("Display Info", parent)
{
    setFixedSize(200, 150);
    setWindowIcon(QIcon(APPICON_PATH));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(10);

    layout->addWidget(new QLabel(QString("<b>Screen width:</b> %1").arg(GLOB.screen.w), this));
    layout->addWidget(new QLabel(QString("<b>Screen height:</b> %1").arg(GLOB.screen.h), this));
    layout->addWidget(new QLabel(QString("<b>Bytes per pixel:</b> %1").arg(GLOB.screen.pixel_width), this));
    layout->addWidget(new QLabel(QString("<b>RGB mode:</b> %1").arg(GLOB.screen.rgb_mode ? "Yes" : "No"), this));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Check for a running instance
    if(!SingleInstanceLock::grabSingleInstanceLock(QString(LOCK_FILE)))
    {
        return 0;
    }

    DisplayInfoWindow dispWindow;
    dispWindow.show();

    // Ensure the lock file is released when the window closes
    QObject::connect(&app, &QApplication::aboutToQuit, []()
    {
        SingleInstanceLock::releaseSingleInstanceLock(QString(LOCK_FILE));
    });

    return app.exec();
}

