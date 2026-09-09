#include <QApplication>
#include "paintmainwindow.h"

int main(int argc, char *argv[]) 
{
    QApplication app(argc, argv);
    PaintMainWindow window;
    window.show();
    return app.exec();
}

