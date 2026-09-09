#ifndef SCREENSHOT_WIDGET_H
#define SCREENSHOT_WIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QScreen>
#include <QGuiApplication>
#include <QPixmap>
#include <QThread>
#include <QWindow>
#include "selectionoverlay.h"
#include "previewdialog.h"

class ScreenshotWidget : public QWidget
{
public:
    explicit ScreenshotWidget(QWidget *parent = nullptr);

private:
    void processCapturedPixmap(const QPixmap &pixmap);
    void captureFullScreen();

#ifdef __laylaos__
    void captureActiveWindow();
#endif

    void captureSelection();
};

#endif      /* SCREENSHOT_WIDGET_H */
