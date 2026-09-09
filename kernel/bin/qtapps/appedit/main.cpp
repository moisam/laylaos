#include <QApplication>
#include <QStyleFactory>
#include <QTimer>
#include "editor.hpp"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

    Editor window;
    window.show();

    // Loop through command-line arguments
    for(int i = 1; i < argc; ++i)
    {
        QString fileArg = QString::fromLocal8Bit(argv[i]);
        if(!fileArg.isEmpty())
        {
            window.openFile(fileArg);
        }
    }

    // Force UI input focus onto the active editing canvas
    QTimer::singleShot(0, &window, [&window]()
    {
        if(auto* currentEdit = window.currentTextEdit())
        {
            currentEdit->setFocus(Qt::OtherFocusReason);
        }
    });

    return app.exec();
}

