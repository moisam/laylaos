#include <QApplication>

#include <dirent.h>
#include <sys/stat.h>
#include <gui/gui.h>
#include <gui/theme.h>
#include <gui/keys.h>
#include <gui/client/systray.h>
#include "panel_window.h"

#define GLOB                        __global_gui_data


void autostartSystemWidgets(void)
{
    const char *widgetDirectory = "/bin/desktop/widgets";

    DIR* dir = opendir(widgetDirectory);

    if(!dir)
    {
        qWarning() << "Failed to open widget directory:" << widgetDirectory;
        return;
    }

    struct dirent *entry;

    while((entry = readdir(dir)) != nullptr)
    {
        QString fileName = QString::fromUtf8(entry->d_name);
        if (fileName == "." || fileName == "..") continue;

        QString fullPath = QString("%1/%2").arg(widgetDirectory, fileName);
        QByteArray pathBuffer = fullPath.toUtf8();
        const char* binaryPath = pathBuffer.constData();

        struct stat fileStat;

        if(stat(binaryPath, &fileStat) == 0)
        {
            if(S_ISREG(fileStat.st_mode) && (fileStat.st_mode & S_IXUSR))
            {
                pid_t pid = fork();

                if(pid < 0)
                {
                    qWarning() << "Failed to fork process for widget:" << fileName;
                } 
                else if(pid == 0)
                {
                    char* const childArgv[] = { const_cast<char*>(binaryPath), nullptr };

                    //setenv("QT_LAYLAOS_THEME", "dark", 1);
                    execv(binaryPath, childArgv);
                    _exit(1); 
                } 
                else
                {
                    qDebug() << "Successfully spawned widget process:" << fileName << "with PID:" << pid;
                }
            }
        }
    }

    closedir(dir);
}


void initAltTab(winid_t mywinid)
{
    key_bind(mywinid, KEYCODE_TAB, MODIFIER_MASK_ALT, KEYBINDING_NOTIFY_ONCE);
    key_bind(mywinid, KEYCODE_LALT, 0, KEYBINDING_NOTIFY);
    key_bind(mywinid, KEYCODE_RALT, 0, KEYBINDING_NOTIFY);

    //key_bind(mywinid, KEYCODE_APPS, 0, KEYBINDING_NOTIFY_ONCE);
    key_bind(mywinid, KEYCODE_LGUI, 0, KEYBINDING_NOTIFY_ONCE);
    key_bind(mywinid, KEYCODE_CALC, 0, KEYBINDING_NOTIFY_ONCE);
}


int main(int argc, char *argv[])
{
    volatile int i = 10;

    // this must be called before Qt initialises the system layer
    setenv("QT_LAYLAOS_THEME", "dark", 1);

    QApplication app(argc, argv);
    PanelWindow panel;

    // this must be called after Qt creates the main window
    set_dark_color_theme();

    panel.show();

    struct window_t *win;
    winid_t mywinid = TO_WINID(GLOB.mypid, 0);
    register_window_event_listener(mywinid);
    register_systray_manager(mywinid);

    // wait until the server registers us as the systray manager
    while(i--)
    {
        if(systray_get_manager_winid() == mywinid)
        {
            break;
        }

        sched_yield();
    }

    if((win = win_for_winid(mywinid)))
    {
        window_set_attrib_xxx(win, WINDOW_NORAISE, 0);
    }

    initAltTab(mywinid);
    autostartSystemWidgets();

    return app.exec();
}

