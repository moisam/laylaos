#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QScreen>

#include "../../desktop/include/gui.h"

int g_lastCapacity = -1;
QString g_lastStatus = "";
std::map<QString, QIcon> g_batteryIconCache;

void populateBatteryIconCache()
{
    const QStringList iconKeys =
    {
        "battery-err",
        "battery-charging",
        "battery-100",
        "battery-90",
        "battery-80",
        "battery-70",
        "battery-60",
        "battery-50",
        "battery-40",
        "battery-25",
        "battery-10",
        "battery-0"
    };

    QString fallbackPath = DEFAULT_ICON_PATH "/battery-err.png";

    for(const QString& key : iconKeys)
    {
        QString preferredPath = QString(DEFAULT_ICON_PATH "/%1.png").arg(key);
        QString finalPath = QFile::exists(preferredPath) ? preferredPath : fallbackPath;
        
        g_batteryIconCache[key] = QIcon(finalPath);
    }
}

QString readBatteryFile(const QString& filename)
{
    QFile file("/proc/acpi/BAT0/" + filename);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) return "";

    QTextStream in(&file);
    QString result = in.readAll().trimmed();
    file.close();
    return result;
}

void updateBatteryTelemetry(QSystemTrayIcon *trayIcon)
{
    QString capacityStr = readBatteryFile("capacity");
    QString statusStr = readBatteryFile("status");

    int capacity = capacityStr.isEmpty() ? 100 : capacityStr.toInt();
    QString statusText = statusStr.isEmpty() ? "Discharging" : statusStr; 

    if(capacity == g_lastCapacity && statusText == g_lastStatus)
    {
        return; 
    }

    g_lastCapacity = capacity;
    g_lastStatus = statusText;

    bool isCharging = (statusText == "Charging");
    QString iconName = "battery-100";

    if(capacityStr.isEmpty())
    {
        iconName = "battery-err";
    }
    else if(isCharging)
    {
        iconName = "battery-charging";
    }
    else
    {
        if(capacity == 0)        iconName = "battery-0";
        else if(capacity <= 10)  iconName = "battery-10";
        else if(capacity <= 25)  iconName = "battery-25";
        else if(capacity <= 40)  iconName = "battery-40";
        else if(capacity <= 50)  iconName = "battery-50";
        else if(capacity <= 60)  iconName = "battery-60";
        else if(capacity <= 70)  iconName = "battery-70";
        else if(capacity <= 80)  iconName = "battery-80";
        else if(capacity <= 90)  iconName = "battery-90";
    }

    auto it = g_batteryIconCache.find(iconName);
    if(it != g_batteryIconCache.end())
    {
        trayIcon->setIcon(it->second);
    }

    trayIcon->setToolTip(QString("Battery: %1% (%2)").arg(QString::number(capacity), statusText));
}

int main(int argc, char *argv[])
{
    // this must be called before Qt initialises the system layer
    setenv("QT_LAYLAOS_THEME", "dark", 1);

    QApplication app(argc, argv);
    populateBatteryIconCache();

    QSystemTrayIcon *trayIcon = new QSystemTrayIcon();

    QMenu *trayMenu = new QMenu();
    QAction *infoAction = trayMenu->addAction("About...");
    trayMenu->addSeparator();
    QAction *quitAction = trayMenu->addAction("Quit");

    QObject::connect(quitAction, &QAction::triggered, [&app]()
    {
        app.quit();
    });

    QObject::connect(infoAction, &QAction::triggered, [trayMenu]()
    {
        auto *aboutBox = new QMessageBox(trayMenu);
        aboutBox->setWindowTitle("About Battery");
        aboutBox->setText("Built-in system battery widget for LaylaOS desktop.\nRunning on Qt6.");
        aboutBox->setIcon(QMessageBox::Information);
        aboutBox->ensurePolished();

        if(QScreen *screen = QGuiApplication::primaryScreen())
        {
            QRect scr = screen->geometry();
            int dx = scr.x() + (scr.width() - aboutBox->width()) / 2;
            int dy = scr.y() + (scr.height() - aboutBox->height()) / 2;
            aboutBox->move(dx, dy);
        }

        aboutBox->exec();
        aboutBox->deleteLater();
    });

    trayIcon->setContextMenu(trayMenu);

    updateBatteryTelemetry(trayIcon);
    trayIcon->show();

    // Update every 5 seconds
    QTimer *timer = new QTimer(trayIcon);
    QObject::connect(timer, &QTimer::timeout, [trayIcon]()
    {
        updateBatteryTelemetry(trayIcon);
    });
    timer->start(5000);

    return app.exec();
}

