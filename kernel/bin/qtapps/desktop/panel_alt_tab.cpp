#include "panel_alt_tab.h"
#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QStyle>

AltTabSwitcher::AltTabSwitcher(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);

    auto *masterLayout = new QVBoxLayout(this);
    masterLayout->setContentsMargins(10, 10, 10, 10);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setWidgetResizable(true);

    m_contentWidget = new QWidget(m_scrollArea);
    m_contentWidget->setObjectName("ContentCanvas");
    m_contentWidget->setStyleSheet("border: none; background: transparent;");

    m_contentLayout = new QHBoxLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(5, 5, 5, 5);
    m_contentLayout->setSpacing(12);
    m_contentLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_scrollArea->setWidget(m_contentWidget);
    masterLayout->addWidget(m_scrollArea);
}


void AltTabSwitcher::populateAndShow(const std::vector<AltTabWindowInfo>& windows, int initialIndex)
{
    m_windowList = windows;
    m_currentIndex = initialIndex;

    for(QWidget* tile : m_tiles)
    {
        m_contentLayout->removeWidget(tile);
        tile->deleteLater();
    }
    m_tiles.clear();

    if(windows.empty()) return;

    for(const auto& win : windows)
    {
        auto* tile = new QWidget(m_contentWidget);
        tile->setObjectName("Tile");
        tile->setFixedSize(110, 95);
        tile->setAttribute(Qt::WA_StyledBackground, true);

        auto* tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(8, 8, 8, 8);
        tileLayout->setSpacing(6);
        tileLayout->setAlignment(Qt::AlignCenter);

        auto* iconLabel = new QLabel(tile);
        iconLabel->setFixedSize(64, 64);
        iconLabel->setPixmap(win.icon.pixmap(64, 64));
        iconLabel->setScaledContents(true);

        auto* titleLabel = new QLabel(win.title, tile);
        titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        titleLabel->setFont(QFont("Sans-Serif", 9));

        // Use eliding math implicitly if a title spans too wide for the box constraint
        QString elidedTitle = titleLabel->fontMetrics().elidedText(win.title, Qt::ElideRight, 94);
        titleLabel->setText(elidedTitle);

        tileLayout->addWidget(iconLabel, 0, Qt::AlignCenter);
        tileLayout->addWidget(titleLabel, 0, Qt::AlignCenter);

        m_contentLayout->addWidget(tile);
        m_tiles.push_back(tile);
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if(!screen) return;

    int screenWidth = screen->geometry().width();
    int screenHeight = screen->geometry().height();

    // Sum up spacing widths: (Tiles * 110) + (Gaps * 12) + Padding (30)
    int requiredWidth = (windows.size() * 110) + ((windows.size() - 1) * 12) + 30;

    // Cap the maximum width window constraint to 85% of screen
    int maxWidthBound = static_cast<int>(screenWidth * 0.85);
    int finalWidth = std::min(requiredWidth, maxWidthBound);
    int finalHeight = 125;

    // Align layout center on screen
    int spawnX = (screenWidth - finalWidth) / 2;
    int spawnY = (screenHeight - finalHeight) / 2;

    setGeometry(spawnX, spawnY, finalWidth, finalHeight);
    updateSelectedIndex(m_currentIndex);

    show();
    raise();
}


void AltTabSwitcher::updateSelectedIndex(int index)
{
    //qDebug() << "updateSelectedIndex: i " << index << m_tiles.size();
    if(index < 0 || index >= static_cast<int>(m_tiles.size())) return;

    m_currentIndex = index;

    for(int i = 0; i < static_cast<int>(m_tiles.size()); ++i)
    {
        QWidget *tile = m_tiles[i];

        if(i == m_currentIndex)
        {
            tile->setObjectName("TileSelected");
            tile->setStyleSheet("background-color: #117A65; border-radius: 6px;");
        }
        else
        {
            tile->setObjectName("Tile");
            tile->setStyleSheet("background-color: #252525; border-radius: 6px;");
        }

        // Force Qt standard engine style cache reloading
        tile->style()->unpolish(tile);
        tile->style()->polish(tile);

        tile->update();
    }

    // Center the targeted item within the scroll viewport container
    QWidget* currentTile = m_tiles[m_currentIndex];
    int tileLeft = currentTile->pos().x();
    int tileWidth = currentTile->width();

    int viewportWidth = m_scrollArea->viewport()->width();
    int targetScrollX = tileLeft - (viewportWidth / 2) + (tileWidth / 2);

    m_scrollArea->horizontalScrollBar()->setValue(targetScrollX);
}


winid_t AltTabSwitcher::getSelectedWindowId() const
{
    if(m_currentIndex >= 0 && m_currentIndex < static_cast<int>(m_windowList.size()))
    {
        return m_windowList[m_currentIndex].id;
    }

    return 0;
}

