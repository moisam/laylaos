#ifndef SETTINGS_BASE_H
#define SETTINGS_BASE_H

#include <QWidget>

class SettingsBaseWindow : public QWidget
{
public:
    explicit SettingsBaseWindow(const QString& title, QWidget* parent = nullptr) : QWidget(parent)
    {
        setWindowTitle(title);

        setStyleSheet(
            "QWidget { font-family: 'Sans-Serif'; font-size: 12px; }"
        );
    }
};

#endif      /* SETTINGS_BASE_H */
