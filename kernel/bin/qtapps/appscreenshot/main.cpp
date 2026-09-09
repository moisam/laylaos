#include <QApplication>
#include "screenshotwidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ScreenshotWidget widget;
    widget.show();

    return app.exec();
}

