#include <QGuiApplication>
#include <QScreen>
#include <QPushButton>
#include <QMenu>
#include <QAction>
#include <QDateTime>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QMouseEvent>
#include <QEvent>
#include <QSettings>
#include <QCalendarWidget>
#include <QInputDialog>
#include <QMessageBox>

#include <map>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "panel_window.h"
#include "desktop_toast.h"
#include "desktop_entry_parser.h"
#include "qlaylaosclientevents.h"

#include <gui/gui.h>
#include "../../desktop/include/panels/bottom-panel.h"

#include "../../desktop/desktop/run_command.c"
#include "../../desktop/client/inlines.c"

#define GLOB                        __global_gui_data
#define MOUSE_DOUBLE_CLICK          0


/*********************************************************************
 *
 * Handle window creation, show, hide, title, icon and destroy events
 *
 *********************************************************************/

void PanelWindow::recalcButtonWidths()
{
    if(!m_taskbarButtons.empty())
    {
        int totalWidth = this->width();
        int menuWidth = 85; 
        int trayWidth = 140; 

        if(m_menuButton && m_menuButton->width() > 0)
        {
            menuWidth = m_menuButton->width();
        }

        if(m_trayWidget && m_trayWidget->width() > 0)
        {
            trayWidth = m_trayWidget->width();
        }

        /*
        for(const auto& pair : m_taskbarButtons)
        {
            // only include visible taskbar entries
            if(pair.second->isVisible()) visible++;
        }
        */

        int visible = static_cast<int>(m_taskbarButtons.size());
        // = left and right margins (5 * 2) + 
        //   left and right spacing (8 * 2) + 
        //   internal button spacing (4 * button count)
        int layoutMarginsAndSpacing = 26 + (4 * visible);
        int maxAvailableWidth = totalWidth - menuWidth - trayWidth - layoutMarginsAndSpacing;
        //qDebug() << maxAvailableWidth << totalWidth << menuWidth << trayWidth << layoutMarginsAndSpacing;

        int calculatedWidth = std::min(120, maxAvailableWidth / visible);
        //calculatedWidth = std::max(50, calculatedWidth);

        if(calculatedWidth != m_curButtonWidth)
        {
            m_curButtonWidth = calculatedWidth;

            for(const auto& pair : m_taskbarButtons)
            {
                // only include visible taskbar entries
                //if(pair.second->isVisible())
                {
                    pair.second->setFixedWidth(m_curButtonWidth);
                }
            }
        }
    }
}

void PanelWindow::childEvent(winid_t child_winid, uint32_t evtype)
{
    // Do not show systray windows in the taskbar
    //auto itb = m_trayButtons.find(child_winid);
    //if(itb != m_trayButtons.end()) return;

    //qDebug() << "childEvent: " << child_winid << evtype;
    auto it = m_taskbarButtons.find(child_winid);
    if(it != m_taskbarButtons.end())
    {
        if(evtype == EVENT_CHILD_WINDOW_DESTROYED)
        {
            if(it->second == m_lastFocusedChild) m_lastFocusedChild = nullptr;

            m_taskbarLayout->removeWidget(it->second);
            it->second->deleteLater();
            it = m_taskbarButtons.erase(it);
            recalcButtonWidths();
        }
        else if(evtype == EVENT_CHILD_WINDOW_RAISED)
        {
            if(m_lastFocusedChild != nullptr) m_lastFocusedChild->setChecked(false);
            it->second->setChecked(true);
            it->second->setVisible(true);
            m_lastFocusedChild = it->second;
            recalcButtonWidths();
        }
        else if(evtype == EVENT_CHILD_WINDOW_SHOWN)
        {
            it->second->setVisible(true);
            recalcButtonWidths();
        }
        else if(evtype == EVENT_CHILD_WINDOW_HIDDEN)
        {
            if(it->second == m_lastFocusedChild) m_lastFocusedChild = nullptr;
            it->second->setChecked(false);
        }
    }
    else if(evtype == EVENT_CHILD_WINDOW_CREATED)
    {
        auto* taskButton = new QPushButton(QString(), this);
        taskButton->setCheckable(true);
        taskButton->setFixedWidth(m_curButtonWidth);
        taskButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        taskButton->setStyleSheet("QPushButton { text-align: left; padding: 4px; }");
        taskButton->setVisible(false);

        QObject::connect(taskButton, &QPushButton::clicked, [this, child_winid, taskButton]()
        {
            if(m_lastFocusedChild != nullptr) m_lastFocusedChild->setChecked(false);
            m_lastFocusedChild = taskButton;
            simple_request(REQUEST_WINDOW_TOGGLE_STATE, GLOB.server_winid, child_winid);
        });

        m_taskbarLayout->addWidget(taskButton);
        m_taskbarButtons[child_winid] = taskButton;
    }
    //else qDebug() << "childEvent: " << child_winid << evtype;
}

void PanelWindow::childEventTitleSet(winid_t child_winid, QString title)
{
    auto it = m_taskbarButtons.find(child_winid);

    if(it != m_taskbarButtons.end())
    {
        if(it->second->text() != title)
        {
            it->second->setText(title);
        }
    }
}

static QIcon createIconFromRawBytes(uint32_t *rawBytes, int width, int height)
{
    // Convert native RGBA to Qt ARGB
    int x, y;
    uint32_t *p32 = rawBytes;

    for(y = 0; y < height; y++)
    {
        for(x = 0; x < width; x++)
        {
            p32[x] = (p32[x] >> 8) | (p32[x] << 24);
        }

        p32 += width;
    }

    QImage rawImage(
        reinterpret_cast<const uchar*>(rawBytes),
        width,
        height,
        QImage::Format_ARGB32
    );

    QPixmap pixmap = QPixmap::fromImage(rawImage);

    return QIcon(pixmap);
}

void PanelWindow::childEventIconSet(winid_t child_winid, uint32_t restype, resid_t resid)
{
    auto iti = m_taskbarIconCache.find(child_winid);
    if(iti != m_taskbarIconCache.end())
    {
        // icon is already in cache
        return;
    }

    // get the icon from the server
    resid_t loaded_resid;
    struct bitmap32_t *bitmap = NULL, *tmp;

    if(!(bitmap = (struct bitmap32_t *)malloc(sizeof(struct bitmap32_t))))
    {
        return;
    }

    bitmap->width = 64;
    bitmap->height = 64;
    bitmap->data = NULL;

    if((loaded_resid = image_get(resid, bitmap)) == INVALID_RESID)
    {
        free(bitmap);
        return;
    }

    // ensure we got the right image
    if(loaded_resid != resid)
    {
        free(bitmap->data);
        free(bitmap);
        return;
    }

    QIcon newIcon = createIconFromRawBytes(bitmap->data, bitmap->width, bitmap->height);
    m_taskbarIconCache[child_winid] = newIcon;

    free(bitmap->data);
    free(bitmap);

    auto it = m_taskbarButtons.find(child_winid);
    if(it != m_taskbarButtons.end())
    {
        QPushButton *taskButton = it->second;
        taskButton->setIcon(newIcon);
        taskButton->setIconSize(QSize(18, 18));
    }
}


/************************************************************************
 *
 * Handle systray creation, show, hide, tooltip, icon and destroy events
 *
 ************************************************************************/

void PanelWindow::systrayEvent(winid_t winid, uint32_t evtype)
{
    auto it = m_trayButtons.find(winid);

    if(it != m_trayButtons.end())
    {
        if(evtype == REQUEST_SYSTRAY_REMOVE)
        {
            m_trayLayout->removeWidget(it->second);
            it->second->deleteLater();
            it = m_trayButtons.erase(it);

            // Ensure the hidden systray window does not linger in our taskbar
            auto itb = m_taskbarButtons.find(winid);
            if(itb != m_taskbarButtons.end())
            {
                if(itb->second == m_lastFocusedChild) m_lastFocusedChild = nullptr;
                m_taskbarLayout->removeWidget(itb->second);
                itb->second->deleteLater();
                itb = m_taskbarButtons.erase(itb);
            }
        }
        else if(evtype == REQUEST_SYSTRAY_SHOW)
        {
            it->second->setVisible(true);
        }
        else if(evtype == REQUEST_SYSTRAY_HIDE)
        {
            it->second->setVisible(false);
        }
    }
    else if(evtype == REQUEST_SYSTRAY_ADD)
    {
        auto* trayButton = new QToolButton(this);
        trayButton->setFixedSize(24, 24);
        trayButton->setStyleSheet("background: transparent; border: none;");
        trayButton->setVisible(false);
        trayButton->installEventFilter(this);
        trayButton->setProperty("winid", QString::number(winid));

        m_trayButtons[winid] = trayButton;

        int clockIndex = m_trayLayout->indexOf(m_clockLabel);
        if (clockIndex == -1) clockIndex = 0;

        // Force the new widget BEFORE the clock
        m_trayLayout->insertWidget(clockIndex, trayButton);
    }
}

void PanelWindow::systrayBoundsEvent(winid_t winid, uint32_t seqid)
{
    auto it = m_trayButtons.find(winid);

    if(it != m_trayButtons.end())
    {
        QPoint localTopLeft(0, 0);
        QPoint globalTopLeft = it->second->mapToGlobal(localTopLeft);
        struct event_t ev;

        ev.rect.top = globalTopLeft.y();
        ev.rect.left = globalTopLeft.x();
        ev.rect.bottom = ev.rect.top + it->second->size().height();
        ev.rect.right = ev.rect.left + it->second->size().width();

        ev.type = EVENT_SYSTRAY_BOUNDS;
        ev.seqid = seqid;
        ev.src = TO_WINID(GLOB.mypid, 0);
        ev.dest = winid;
        ev.valid_reply = 1;

        direct_write(GLOB.serverfd, &ev, sizeof(struct event_t));
    }
}

void PanelWindow::systrayIconEvent(winid_t winid, QIcon icon)
{
    auto it = m_trayButtons.find(winid);

    if(it != m_trayButtons.end())
    {
        it->second->setIcon(icon);
    }
}

void PanelWindow::systrayTooltipEvent(winid_t winid, QString tooltip)
{
    auto it = m_trayButtons.find(winid);

    if(it != m_trayButtons.end())
    {
        if(it->second->toolTip() != tooltip)
        {
            it->second->setToolTip(tooltip);
        }
    }
}

void PanelWindow::systrayMsgEvent(QString title, QString msg, int type, int duration, QPixmap pixmap)
{
    auto *toast = new DesktopToast(title, msg, type, duration, pixmap);
    toast->show();
}


/*********************************************************************
 *
 * Dispatch different custom events
 *
 *********************************************************************/

void PanelWindow::customEvent(QEvent *event)
{
    QEvent::Type type = event->type();

    if(type == (QEvent::Type)LaylaOSChildEventType)
    {
        auto *customEvt = static_cast<LaylaOSChildEvent *>(event);
        this->childEvent(customEvt->m_childWinid, customEvt->m_evType);
    }
    else if(type == (QEvent::Type)LaylaOSChildTitleEventType)
    {
        auto *customEvt = static_cast<LaylaOSChildTitleEvent *>(event);
        this->childEventTitleSet(customEvt->m_childWinid, customEvt->m_title);
    }
    else if(type == (QEvent::Type)LaylaOSChildIconEventType)
    {
        auto *customEvt = static_cast<LaylaOSChildIconEvent *>(event);
        this->childEventIconSet(customEvt->m_childWinid, customEvt->m_restype, customEvt->m_resid);
    }
    else if(type == (QEvent::Type)LaylaOSSystrayRequestType)
    {
        auto *customEvt = static_cast<LaylaOSSystrayRequest *>(event);
        this->systrayEvent(customEvt->m_winid, customEvt->m_evType);
    }
    else if(type == (QEvent::Type)LaylaOSSystrayIconRequestType)
    {
        auto *customEvt = static_cast<LaylaOSSystrayIconRequest *>(event);
        this->systrayIconEvent(customEvt->m_winid, customEvt->m_icon);
    }
    else if(type == (QEvent::Type)LaylaOSSystrayTooltipRequestType)
    {
        auto *customEvt = static_cast<LaylaOSSystrayTooltipRequest *>(event);
        this->systrayTooltipEvent(customEvt->m_winid, customEvt->m_tooltip);
    }
    else if(type == (QEvent::Type)LaylaOSSystrayBoundsRequestType)
    {
        auto *customEvt = static_cast<LaylaOSSystrayBoundsRequest *>(event);
        this->systrayBoundsEvent(customEvt->m_winid, customEvt->m_seqid);
    }
    else if(type == (QEvent::Type)LaylaOSSystrayMsgRequestType)
    {
        auto *customEvt = static_cast<LaylaOSSystrayMsgRequest *>(event);
        this->systrayMsgEvent(customEvt->m_title, customEvt->m_msg,
                              customEvt->m_type, customEvt->m_duration, customEvt->m_pixmap);
    }
    else
    {
        QWidget::customEvent(event);
    }
}


/*********************************************************************
 *
 * Clock widget functions
 *
 *********************************************************************/

QString getSavedClockFormat()
{
    QString configPath = QDir::homePath() + "/.config/clock_widget.conf";
    QSettings settings(configPath, QSettings::IniFormat);
    return settings.value("Format", "hh:mm").toString(); // Default fallback
}

void saveClockFormat(const QString& format)
{
    QString configPath = QDir::homePath() + "/.config/clock_widget.conf";
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setValue("Format", format);
    settings.sync(); // Force immediate filesystem write
}

class ClockEventFilter : public QObject
{
private:
    QLabel *m_label;
    QCalendarWidget *m_cal;
    QWidget *m_panel;

public:
    ClockEventFilter(QLabel *label, QCalendarWidget *cal, QWidget *panel) 
            : QObject(panel), m_label(label), m_cal(cal), m_panel(panel) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if(watched == m_label)
        {
            if(event->type() == QEvent::MouseButtonPress)
            {
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                if(mouseEvent->button() == Qt::LeftButton)
                {
                    if(m_cal->isVisible())
                    {
                        m_cal->hide();
                    }
                    else
                    {
                        QScreen *screen = QGuiApplication::primaryScreen();
                        if (!screen) return false;

                        QRect screenGeom = screen->geometry();
                        int screenWidth = screenGeom.width();

                        QPoint clockGlobalPos = m_label->mapToGlobal(QPoint(0, 0));

                        int targetX = clockGlobalPos.x() + (m_label->width() / 2) - (m_cal->width() / 2);
                        int targetY = clockGlobalPos.y() - m_cal->height() - 5; // 5px padding above taskbar

                        int rightMarginPadding = 12;
                        if((targetX + m_cal->width()) > screenWidth)
                        {
                            targetX = screenWidth - m_cal->width() - rightMarginPadding;
                        }

                        if(targetX < 0) targetX = 10;

                        m_cal->move(targetX, targetY);
                        m_cal->show();
                        m_cal->raise();
                        m_cal->activateWindow();
                    }
                    return true;
                }
            }

            if(event->type() == QEvent::ContextMenu)
            {
                auto *contextMenu = new QMenu(m_panel);

                QAction *formatAction = contextMenu->addAction("Change Format...");
                QAction *aboutAction  = contextMenu->addAction("About Clock...");

                QObject::connect(formatAction, &QAction::triggered, [this]()
                {
                    // Define human-readable options for the user selection list
                    QStringList displayOptions;
                    displayOptions << "13:45 (Standard)"
                                   << "13:45:30 (With Seconds)"
                                   << "Mon 13:45 (With Day)"
                                   << "2026-08-24 13:45 (Date & Time)";

                    // Map the options directly to standard Qt DateTime strings
                    std::vector<QString> tokenMap =
                    {
                        "hh:mm",
                        "hh:mm:ss",
                        "ddd hh:mm",
                        "yyyy-MM-dd hh:mm"
                    };

                    // Look up current configuration to set active default list item
                    QString activeFormat = getSavedClockFormat();
                    int defaultItemIdx = 0; // Fallback default

                    for(size_t i = 0; i < tokenMap.size(); ++i)
                    {
                        if(tokenMap[i] == activeFormat)
                        {
                            defaultItemIdx = static_cast<int>(i);
                            break;
                        }
                    }

                    auto* inputDialog = new QInputDialog(m_panel);
                    inputDialog->setWindowTitle("Time Format");
                    inputDialog->setLabelText("Select preferred layout:");
                    inputDialog->setComboBoxItems(displayOptions);

                    if(defaultItemIdx >= 0 && defaultItemIdx < displayOptions.size())
                    {
                        inputDialog->setTextValue(displayOptions[defaultItemIdx]);
                    }

                    inputDialog->setOption(QInputDialog::UseListViewForComboBoxItems, false);
                    inputDialog->ensurePolished();

                    if(QScreen *screen = QGuiApplication::primaryScreen())
                    {
                        QRect scr = screen->geometry();
                        int dx = scr.x() + (scr.width() - inputDialog->width()) / 2;
                        int dy = scr.y() + (scr.height() - inputDialog->height()) / 2;
                        inputDialog->move(dx, dy);
                    }

                    if(inputDialog->exec() == QDialog::Accepted)
                    {
                        QString selectedText = inputDialog->textValue();
                        int selectedIndex = displayOptions.indexOf(selectedText);
                        if(selectedIndex >= 0 && selectedIndex < static_cast<int>(tokenMap.size()))
                        {
                            QString chosenToken = tokenMap[selectedIndex];
                            saveClockFormat(chosenToken);
                            m_label->setText(QDateTime::currentDateTime().toString(chosenToken));
                        }
                    }
                    inputDialog->deleteLater();
                });

                QObject::connect(aboutAction, &QAction::triggered, [this]()
                {
                    auto *aboutBox = new QMessageBox(m_panel);
                    aboutBox->setWindowTitle("About Clock");
                    aboutBox->setText("Built-in system clock widget for LaylaOS desktop.\nRunning on Qt6.");
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

                contextMenu->exec(QCursor::pos());
                contextMenu->deleteLater();
                return true;
            }
        }

        // Dismiss calendar popup automatically if it loses focus
        if(watched == m_cal && event->type() == QEvent::WindowDeactivate)
        {
            m_cal->hide();
        }

        return QObject::eventFilter(watched, event);
    }
};


/*********************************************************************
 *
 * Main window functions
 *
 *********************************************************************/

PanelWindow::PanelWindow(QWidget *parent) : QWidget(parent)
{
    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    int scrh, scrw;

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    // Make us stay at the bottom of the screen
    scrh = primaryScreen ? primaryScreen->geometry().height() : GLOB.screen.h;
    scrw = primaryScreen ? primaryScreen->geometry().width() : GLOB.screen.w;
    setGeometry(QRect(0, scrh - BOTTOMPANEL_HEIGHT, scrw, BOTTOMPANEL_HEIGHT));
    setFixedHeight(BOTTOMPANEL_HEIGHT);
    setFixedWidth(scrw);

    set_desktop_bounds(0, 0, scrh - BOTTOMPANEL_HEIGHT - 1, scrw - 1);

    setStyleSheet(
        "QPushButton::hover { background-color: #2F2F2F; border-color: #16A085; }"
        "QPushButton:checked { background-color: #117A65; border-color: #1ABC9C; font-weight: bold; color: #FFFFFF; border-bottom: 3px solid #1ABC9C; }"
        "QToolTip { background-color: #1F1F1F; color: #ECF0F1; border: 1px solid #16A085; border-radius: 4px; padding: 3px; }"
    );

    // Load the default app icon
    m_defaultAppIcon = QIcon(DEFAULT_ICON_PATH "/executable_window.png");

    // Create layout
    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 0, 5, 0);
    m_mainLayout->setSpacing(8);

    createApplicationMenu();

    m_taskbarWidget = new QWidget(this);
    m_taskbarLayout = new QHBoxLayout(m_taskbarWidget);
    m_taskbarLayout->setContentsMargins(0, 0, 0, 0);
    m_taskbarLayout->setSpacing(4);
    m_taskbarLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter); 

    // Add the taskbar widget to the main layout and let it expand to fill space
    m_mainLayout->addWidget(m_taskbarWidget, 1);

    createSystemTray();

    this->installEventFilter(this);

    m_switcher = new AltTabSwitcher();
    m_switcher->installEventFilter(this);

    // Handle display size change
    if(primaryScreen)
    {
        QObject::connect(primaryScreen, &QScreen::geometryChanged, [this, primaryScreen](const QRect& newGeometry)
        {
            int scrh, scrw;
            scrh = primaryScreen->geometry().height();
            scrw = primaryScreen->geometry().width();
            setGeometry(QRect(0, scrh - BOTTOMPANEL_HEIGHT, scrw, BOTTOMPANEL_HEIGHT));
            set_desktop_bounds(0, 0, scrh - BOTTOMPANEL_HEIGHT - 1, scrw - 1);
            setFixedWidth(scrw);
            recalcButtonWidths();
            this->update();
        });
    }
}

QIcon PanelWindow::getCachedIcon(const QString& iconPath, const QString& iconName)
{
    auto it = m_appMenuIconCache.find(iconName);
    if(it != m_appMenuIconCache.end())
    {
        return it->second;
    }

    QString path = DesktopEntryParser::resolveIconPath(iconPath, iconName);
    QIcon newlyLoadedIcon(path);
    m_appMenuIconCache[iconName] = newlyLoadedIcon;

    return newlyLoadedIcon;
}

void PanelWindow::createApplicationMenu()
{
    auto *rootMenu = new QMenu(this);

    m_menuButton = new QPushButton("Applications", this);
    m_menuButton->setFixedSize(105, 30);
    m_menuButton->setCheckable(true); 

    std::vector<DesktopEntry> entries = DesktopEntryParser::parseAllEntries();

    // Pivot table: Category Name -> Collection of entries belonging to it
    std::map<QString, std::vector<DesktopEntry>> categorizedMenu;

    for(const DesktopEntry& entry : entries)
    {
        QString category = entry.category.isEmpty() ? "Accessories" : entry.category;
        categorizedMenu[category].push_back(entry);
    }

    // Iterate to build submenus
    for(auto it = categorizedMenu.begin(); it != categorizedMenu.end(); ++it)
    {
        const QString& categoryName = it->first;
        const std::vector<DesktopEntry>& appList = it->second;

        // Skip category creation if there are no apps inside it
        if (appList.empty()) continue;

        QMenu* subMenu = rootMenu->addMenu(categoryName);

        for(const DesktopEntry& app : appList)
        {
            QAction *appAction = subMenu->addAction(app.name);

            appAction->setIcon(getCachedIcon(app.iconPath, app.iconName));

            QString cmd = app.command;
            
            QObject::connect(appAction, &QAction::triggered, [this, cmd]()
            {
                QByteArray byteBuffer = cmd.toUtf8();
                const char *c_str = byteBuffer.constData();

                // unset this for the app we are about to run
                unsetenv("QT_LAYLAOS_THEME");

                run_command((char *)c_str);

                // we need to restore this in case system theme changed later on
                setenv("QT_LAYLAOS_THEME", "dark", 1);
            });
        }
    }

    m_menuButton->setMenu(rootMenu);

    QObject::connect(rootMenu, &QMenu::aboutToHide, [this]()
    {
        m_menuButton->setChecked(false);
        m_menuButton->setAttribute(Qt::WA_UnderMouse, false);
        m_menuButton->update();
    });

    m_mainLayout->addWidget(m_menuButton);
}

void PanelWindow::createSystemTray()
{
    m_trayWidget = new QWidget(this);
    m_trayLayout = new QHBoxLayout(m_trayWidget);
    m_trayLayout->setContentsMargins(0, 0, 0, 0);
    m_trayLayout->setSpacing(4);
    m_trayLayout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // Clock widget
    m_clockLabel = new QLabel(m_trayWidget);
    m_clockLabel->setStyleSheet("font-weight: bold; color: #ECF0F1; padding-right: 4px; padding-left: 4px;");
    m_clockLabel->setContextMenuPolicy(Qt::CustomContextMenu);
    m_trayLayout->addWidget(m_clockLabel);

    // Floating Calendar Popup Window
    auto *calendarPopup = new QCalendarWidget(nullptr);
    calendarPopup->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    calendarPopup->setFixedSize(280, 220);
    calendarPopup->setStyleSheet(
        "QCalendarWidget { background-color: #1F1F1F; border: 1px solid #16A085; border-radius: 6px; }"
        "QCalendarWidget QWidget { color: #ECF0F1; alternate-background-color: #252525; }"
        "QCalendarWidget QAbstractItemView:enabled { background-color: #1F1F1F; selection-background-color: #16A085; selection-color: white; }"
    );

    auto *timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, [this]()
    {
        QString currentFormat = getSavedClockFormat();
        m_clockLabel->setText(QDateTime::currentDateTime().toString(currentFormat));
    });
    timer->start(1000);

    // Install the filter
    auto *clockFilter = new ClockEventFilter(m_clockLabel, calendarPopup, this);
    m_clockLabel->installEventFilter(clockFilter);
    calendarPopup->installEventFilter(clockFilter);

    m_mainLayout->addWidget(m_trayWidget);
}


/*********************************************************************
 *
 * Event handling
 *
 *********************************************************************/

bool PanelWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Handle ALT-Tab key press
    if(event->type() == QEvent::KeyPress)
    {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        //qDebug() << "eventFilter:" << keyEvent->key() << keyEvent->modifiers();

        if(keyEvent->key() == Qt::Key_Tab && (keyEvent->modifiers() & Qt::AltModifier))
        {
            this->handleAltTabKeyPressed();
            return true;
        }
        else if(keyEvent->key() == Qt::Key_Calculator)
        {
            QString cmd = QString("/bin/desktop/appcalc");
            QByteArray byteBuffer = cmd.toUtf8();
            const char *c_str = byteBuffer.constData();

            // unset this for the app we are about to run
            unsetenv("QT_LAYLAOS_THEME");

            run_command((char *)c_str);

            // we need to restore this in case system theme changed later on
            setenv("QT_LAYLAOS_THEME", "dark", 1);

            return true;
        }
        else if(keyEvent->key() == Qt::Key_Super_L)
        {
            if(m_menuButton && m_menuButton->menu())
            {
                if(m_menuButton->menu()->isVisible())
                {
                    m_menuButton->menu()->close();
                }
                else
                {
                    m_menuButton->showMenu(); 
                }

                return true;
            }
        }
    }

    // Handle ALT-Tab release
    if(event->type() == QEvent::KeyRelease)
    {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        if(keyEvent->key() == Qt::Key_Alt || keyEvent->key() == Qt::Key_Meta)
        {
            this->handleAltKeyReleased();
            return true;
        }
    }

    // Handle mouse events on taskbar buttons
    QString winidStr = watched->property("winid").toString();

    if(winidStr.isEmpty())
    {
        return QWidget::eventFilter(watched, event);
    }

    winid_t winid = static_cast<winid_t>(winidStr.toULongLong());

    if(event->type() == QEvent::MouseButtonDblClick)
    {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);

        if(mouseEvent->button() == Qt::LeftButton)
        {
            dispatchTrayClickEvent(winid, MOUSE_DOUBLE_CLICK, watched);
            return true;
        }
    }

    if(event->type() == QEvent::MouseButtonPress)
    {
        auto *mouseEvent = static_cast<QMouseEvent*>(event);
        mouse_buttons_t clickType;

        switch(mouseEvent->button())
        {
            case Qt::LeftButton:   clickType = MOUSE_LBUTTON_DOWN; break;
            case Qt::MiddleButton: clickType = MOUSE_MBUTTON_DOWN; break;
            case Qt::RightButton:  clickType = MOUSE_RBUTTON_DOWN; break;
            default: return false; // Ignore other buttons
        }

        dispatchTrayClickEvent(winid, clickType, watched);
        return true;
    }

    // None of the above -- pass over to QWidget
    return QWidget::eventFilter(watched, event);
}

void PanelWindow::dispatchTrayClickEvent(winid_t winid, mouse_buttons_t mbutton, QObject *widget)
{
    auto *button = qobject_cast<QWidget*>(widget);
    if (!button) return;

    // Calculate the absolute global on-screen position where the icon sits
    QPoint globalPoint = button->mapToGlobal(QPoint(0, 0));

    int spawnX = globalPoint.x();
    int spawnY = globalPoint.y(); 
    struct event_t ev;

    ev.mouse.buttons = mbutton;
    ev.mouse.x = spawnX;
    ev.mouse.y = spawnY;
    ev.seqid = 0;
    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = winid;
    ev.valid_reply = 1;
    ev.type = (mbutton == MOUSE_DOUBLE_CLICK) ? EVENT_SYSTRAY_DOUBLE_CLICK : EVENT_SYSTRAY_CLICK;

    direct_write(GLOB.serverfd, &ev, sizeof(struct event_t));
}

QIcon PanelWindow::getCachedIconForWinId(winid_t winid)
{
    auto iti = m_taskbarIconCache.find(winid);
    if(iti != m_taskbarIconCache.end())
    {
        return iti->second;
    }

    return m_defaultAppIcon;
}

void PanelWindow::handleAltTabKeyPressed()
{
    //qDebug() << "handleAltTabKeyPressed:" << m_isAltTabActive;
    if(!m_isAltTabActive)
    {
        m_isAltTabActive = true;
        m_altTabWindows.clear();

        for(const auto& pair : m_taskbarButtons)
        {
            // only include visible taskbar entries
            if(pair.second->isVisible())
            {
                AltTabWindowInfo win;
                win.id = pair.first;
                win.title = pair.second->text();
                win.icon = getCachedIconForWinId(pair.first); 
                m_altTabWindows.push_back(win);
            }
        }

        if(m_altTabWindows.empty())
        {
            m_isAltTabActive = false;
            return;
        }

        m_altTabCurrentIndex = (m_altTabWindows.size() > 1) ? 1 : 0;
        m_switcher->populateAndShow(m_altTabWindows, m_altTabCurrentIndex);
    }
    else
    {
        m_altTabCurrentIndex = (m_altTabCurrentIndex + 1) % m_altTabWindows.size();
        m_switcher->updateSelectedIndex(m_altTabCurrentIndex);
    }
}

void PanelWindow::handleAltKeyReleased()
{
    //qDebug() << "handleAltKeyReleased:" << m_isAltTabActive;
    if(m_isAltTabActive)
    {
        m_isAltTabActive = false;

        if(m_switcher && m_switcher->isVisible())
        {
            m_switcher->hide();
            winid_t targetWinId = m_switcher->getSelectedWindowId();

            if(targetWinId != 0)
            {
                simple_request(REQUEST_WINDOW_RAISE, GLOB.server_winid, targetWinId);
            }
        }
    }
}

