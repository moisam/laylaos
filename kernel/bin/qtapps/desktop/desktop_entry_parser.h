#ifndef DESKTOP_ENTRY_H
#define DESKTOP_ENTRY_H

#include <QString>
#include <QStringList>
#include <vector>
#include <gui/gui.h>

struct DesktopEntry
{
    QString name;
    QString command;
    QString iconPath;
    QString iconName;
    QString category = "Accessories"; // Fallback default category
    bool showOnDesktop = true;
};

class DesktopEntryParser
{
public:
    // Scans /usr/share/gui/desktop and returns a vector of all parsed valid entries
    static std::vector<DesktopEntry> parseAllEntries()
    {
        std::vector<DesktopEntry> validEntries;

        QDir desktopDir(DEFAULT_DESKTOP_PATH);
        if(!desktopDir.exists()) return validEntries;

        QStringList filters;
        filters << "*.entry";
        QStringList entryFiles = desktopDir.entryList(filters, QDir::Files);

        for(const QString& fileName : entryFiles)
        {
            QFile file(desktopDir.absoluteFilePath(fileName));
            if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

            QTextStream in(&file);
            bool hasValidHeader = false;
            DesktopEntry entry;

            while(!in.atEnd())
            {
                QString line = in.readLine().trimmed();
                if(line.isEmpty() || line.startsWith('#')) continue;

                if(line == "[Desktop Entry]")
                {
                    hasValidHeader = true;
                    continue;
                }

                int eqIdx = line.indexOf('=');
                if(eqIdx == -1) continue;

                QString key = line.left(eqIdx).trimmed();
                QString value = line.mid(eqIdx + 1).trimmed();

                if(key == "Name")           entry.name = value;
                else if(key == "Command")   entry.command = value;
                else if(key == "IconPath")  entry.iconPath = value;
                else if(key == "Icon")      entry.iconName = value;
                else if(key == "Category")  entry.category = value;
                else if(key == "ShowOnDesktop")
                {
                    if(value.toLower() == "no") entry.showOnDesktop = false;
                }
            }

            file.close();

            // Verify
            if(hasValidHeader && !entry.name.isEmpty() && !entry.command.isEmpty())
            {
                validEntries.push_back(entry);
            }
        }

        return validEntries;
    }

    static QString resolveIconPath(const QString& iconPath, const QString& iconName)
    {
        QString preferredPath;
        QString fallback = DEFAULT_ICON_PATH "/executable_window.png";

        if(iconName.isEmpty()) return fallback;

        if(iconPath.isEmpty() || iconPath.toLower() == "default")
             preferredPath = QString(DEFAULT_ICON_PATH "/%1.png").arg(iconName);
        else preferredPath = QString(iconPath + "/%1.png").arg(iconName);

        if(QFile::exists(preferredPath))
        {
            return preferredPath;
        }

        return fallback;
    }
};

#endif      /* DESKTOP_ENTRY_H */
