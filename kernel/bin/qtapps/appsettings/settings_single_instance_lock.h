#ifndef SINGLE_INSTANCE_LOCK_H
#define SINGLE_INSTANCE_LOCK_H

#include <QApplication>
#include <QFile>
#include <QWidget>
#include <unistd.h>

#include "../../desktop/include/gui.h"
#include "../../desktop/client/inlines.c"

#define GLOB                            __global_gui_data


class SingleInstanceLock
{
public:
    static bool grabSingleInstanceLock(const QString& lockfile)
    {
        if(QFile::exists(lockfile))
        {
            QFile file(lockfile);
            pid_t existingWinid = 0;

            if(file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                QTextStream in(&file);
                existingWinid = static_cast<pid_t>(in.readAll().trimmed().toLongLong());
                file.close();
            }

            simple_request(REQUEST_WINDOW_RAISE, GLOB.server_winid, existingWinid);
            return false;
        }

        QFile file(lockfile);
        if(file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream out(&file);
            out << TO_WINID(GLOB.mypid, 0);
            file.close();
            return true;
        }

        return false;
    }

    static void releaseSingleInstanceLock(const QString& lockfile)
    {
        QFile::remove(lockfile);
    }
};

#endif      /* SINGLE_INSTANCE_LOCK_H */
