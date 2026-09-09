#ifndef SELECTION_OVERLAY_H
#define SELECTION_OVERLAY_H

#include <QWidget>
#include <QScreen>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QRubberBand>
#include <QPainter>
#include <functional>

class SelectionOverlay : public QWidget
{
private:
    QPixmap m_fullScreenPixmap;
    QPoint m_startPoint;
    QRubberBand *m_rubberBand = nullptr;
    std::function<void(const QPixmap&)> m_onCaptureCallback;

public:
    explicit SelectionOverlay(std::function<void(const QPixmap&)> callback, QWidget *parent = nullptr);
    void captureBackground();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
};

#endif      /* SELECTION_OVERLAY_H */
