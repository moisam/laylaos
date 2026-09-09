#ifndef PREVIEW_DIALOG_H
#define PREVIEW_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPixmap>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileDialog>
#include <QDateTime>

class PreviewDialog : public QDialog
{
private:
    QPixmap m_pixmap;

public:
    explicit PreviewDialog(const QPixmap &pixmap, QWidget *parent = nullptr);

private:
    void copyToClipboard();
    void saveToFile();
};

#endif      /* PREVIEW_DIALOG_H */
