/***************************************************************************
**
** Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

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

    dirs.append(QString::fromLatin1("/etc/qt5"));

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
