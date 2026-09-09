#include "previewdialog.h"

PreviewDialog::PreviewDialog(const QPixmap &pixmap, QWidget *parent)
    : QDialog(parent), m_pixmap(pixmap) 
{
    setWindowTitle("Screenshot Preview");
    setModal(true);

    auto *mainLayout = new QVBoxLayout(this);
    auto *buttonLayout = new QHBoxLayout();

    auto *imageLabel = new QLabel(this);
    imageLabel->setPixmap(m_pixmap.scaled(580, 460, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    imageLabel->setAlignment(Qt::AlignCenter);

    auto *btnClipboard = new QPushButton("Copy to Clipboard", this);
    auto *btnSave = new QPushButton("Save File...", this);
    auto *btnCancel = new QPushButton("Discard", this);

    buttonLayout->addWidget(btnClipboard);
    buttonLayout->addWidget(btnSave);
    buttonLayout->addWidget(btnCancel);

    mainLayout->addWidget(imageLabel);
    mainLayout->addLayout(buttonLayout);

    connect(btnClipboard, &QPushButton::clicked, this, [this]()
    {
        copyToClipboard();
        accept();
    });

    connect(btnSave, &QPushButton::clicked, this, [this]()
    {
        saveToFile();
        accept();
    });

    connect(btnCancel, &QPushButton::clicked, this, [this]()
    {
        reject();
    });
}

void PreviewDialog::copyToClipboard()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if(clipboard)
    {
        clipboard->setPixmap(m_pixmap);
    }
}

void PreviewDialog::saveToFile()
{
    QString defaultName = QString("Screenshot_%1.png")
                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    QString filePath = QFileDialog::getSaveFileName(this, "Save Screenshot",
                                                    defaultName, "Images (*.png *.jpg)");

    if(!filePath.isEmpty())
    {
        m_pixmap.save(filePath);
    }
}

