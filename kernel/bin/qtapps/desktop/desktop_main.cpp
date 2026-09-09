#include <QApplication>
#include <QScreen>
#include <sys/wait.h>
#include <sys/stat.h>
#include <gui/gui.h>
#include "desktop_window.h"

#define GLOB                        __global_gui_data


void sigchld_handler(int signum __attribute__((unused)))
{
    int        pid, st;
    int        saved_errno = errno;

    while((pid = waitpid(-1, &st, WNOHANG)) != 0)
    {
        if(errno == ECHILD)
        {
            break;
        }
    }

    errno = saved_errno;
}


int main(int argc, char *argv[])
{
    // this must be called before Qt initialises the system layer
    setenv("QT_LAYLAOS_THEME", "dark", 1);

    QApplication app(argc, argv);
    //QScreen *primaryScreen = QGuiApplication::primaryScreen();
    DesktopWindow desktop;
    struct sigaction act;
    struct stat st;

    // if we launch an application (e.g. when the user double clicks an icon
    // on the desktop), we need to be ready to reap the zombie task when it
    // exits
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = sigchld_handler;
    act.sa_flags = SA_RESTART;
    (void)sigaction(SIGCHLD, &act, NULL);

    // load default background if it is available
    // if this fails, the default background color is automatically used
    if(stat(BACKGROUNDS_DIR_PATH "/desktop-background9.jpeg", &st) != -1)
    {
        desktop.setWallpaper(QString(BACKGROUNDS_DIR_PATH "/desktop-background9.jpeg"));
    }

    // this must be called after Qt creates the main window
    set_dark_color_theme();

    // show desktop wallpaper first
    desktop.show();

    // then load desktop entries
    desktop.loadEntries();

    if(!fork())
    {
        char *argv[] = { (char *)"/bin/desktop/desktop-panel", NULL };
        execvp((char *)"/bin/desktop/desktop-panel", argv);
        exit(EXIT_FAILURE);
    }

    return app.exec();
}

