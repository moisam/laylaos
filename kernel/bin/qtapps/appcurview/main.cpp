#include <QApplication>
#include "CursorViewer.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CursorViewer viewer;
    viewer.show();

    if(app.arguments().size() > 1)
    {
        QString argPath = app.arguments().at(1);
        viewer.processCommandLineArgument(argPath);
    }
    else
    {
        viewer.discoverCursorThemes();
    }

    return app.exec();
}

