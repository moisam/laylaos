// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:critical reason:provides-trusted-directory-paths

#include "qstandardpaths.h"
#include <qdir.h>
#include <qfile.h>
#include <private/qfilesystemengine_p.h>
#include <errno.h>
#include <stdlib.h>

#ifndef QT_NO_STANDARDPATHS

#ifndef QT_BOOTSTRAPPED
#include <qcoreapplication.h>
#endif

QT_BEGIN_NAMESPACE

static void appendOrganizationAndApp(QString &path)
{
#ifndef QT_BOOTSTRAPPED
    const QString org = QCoreApplication::organizationName();
    if (!org.isEmpty())
        path += QLatin1Char('/') + org;
    const QString appName = QCoreApplication::applicationName();
    if (!appName.isEmpty())
        path += QLatin1Char('/') + appName;
#else
    Q_UNUSED(path);
#endif
}

QString QStandardPaths::writableLocation(StandardLocation type)
{
    switch (type) {
    case HomeLocation:
        return QDir::homePath();
    case TempLocation:
        return QDir::tempPath();
    case CacheLocation:
    case GenericCacheLocation:
    {
        QString cacheHome = QString();
        if (isTestModeEnabled())
            cacheHome = QDir::homePath() + QLatin1String("/.qttest/cache");
        if (cacheHome.isEmpty())
            cacheHome = QDir::homePath() + QLatin1String("/.cache");
        if (type == QStandardPaths::CacheLocation)
            appendOrganizationAndApp(cacheHome);
        return cacheHome;
    }
    case AppDataLocation:
    case AppLocalDataLocation:
    case GenericDataLocation:
    {
        QString dataHome = QString();
        if (isTestModeEnabled())
            dataHome = QDir::homePath() + QLatin1String("/.qttest/share");
        if (dataHome.isEmpty())
            dataHome = QDir::homePath() + QLatin1String("/.local/share");
        if (type == AppDataLocation || type == AppLocalDataLocation)
            appendOrganizationAndApp(dataHome);
        return dataHome;
    }
    case ConfigLocation:
    case GenericConfigLocation:
    case AppConfigLocation:
    {
        QString configHome = QString();
        if (isTestModeEnabled())
            configHome = QDir::homePath() + QLatin1String("/.qttest/config");
        if (configHome.isEmpty())
            configHome = QDir::homePath() + QLatin1String("/.config");
        if (type == AppConfigLocation)
            appendOrganizationAndApp(configHome);
        return configHome;
    }
    case DesktopLocation:
        return QDir::homePath() + QLatin1String("/Desktop");
    case DocumentsLocation:
        return QDir::homePath() + QLatin1String("/Documents");
    case PicturesLocation:
        return QDir::homePath() + QLatin1String("/Pictures");
    case FontsLocation:
        return writableLocation(GenericDataLocation) + QLatin1String("/fonts");
    case MusicLocation:
        return QDir::homePath() + QLatin1String("/Music");
    case MoviesLocation:
        return QDir::homePath() + QLatin1String("/Videos");
    case DownloadLocation:
        return QDir::homePath() + QLatin1String("/Downloads");
    case ApplicationsLocation:
        return writableLocation(GenericDataLocation) + QLatin1String("/applications");
    default:
        return QString();
    }
}

static QStringList dataDirs()
{
    QStringList dirs;

    dirs.append(QString::fromLatin1("/usr/local/share"));
    dirs.append(QString::fromLatin1("/usr/share"));

    return dirs;
}

static QStringList configDirs()
{
    QStringList dirs;

    dirs.append(QString::fromLatin1("/etc/qt6"));

    return dirs;
}

QStringList QStandardPaths::standardLocations(StandardLocation type)
{
    QStringList dirs;
    switch (type) {
    case ConfigLocation:
    case GenericConfigLocation:
        dirs = configDirs();
        break;
    case AppConfigLocation:
        dirs = configDirs();
        for (int i = 0; i < dirs.count(); ++i)
            appendOrganizationAndApp(dirs[i]);
        break;
    case GenericDataLocation:
        dirs = dataDirs();
        break;
    case ApplicationsLocation:
        dirs = dataDirs();
        for (int i = 0; i < dirs.count(); ++i)
            dirs[i].append(QLatin1String("/applications"));
        break;
    case AppDataLocation:
    case AppLocalDataLocation:
        dirs = dataDirs();
        for (int i = 0; i < dirs.count(); ++i)
            appendOrganizationAndApp(dirs[i]);
        break;
    case FontsLocation:
        dirs += QDir::homePath() + QLatin1String("/.fonts");
        break;
    default:
        break;
    }
    const QString localDir = writableLocation(type);
    dirs.prepend(localDir);
    return dirs;
}

QT_END_NAMESPACE

#endif // QT_NO_STANDARDPATHS
