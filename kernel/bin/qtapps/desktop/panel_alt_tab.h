#ifndef PANEL_ALT_TAB_H
#define PANEL_ALT_TAB_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QScrollArea>
#include <vector>
#include <map>

#include "../../desktop/include/window-defs.h"

struct AltTabWindowInfo
{
    winid_t id;
    QString title;
    QIcon icon;
};

class AltTabSwitcher : public QWidget
{
public:
    explicit AltTabSwitcher(QWidget* parent = nullptr);
    
    void populateAndShow(const std::vector<AltTabWindowInfo>& windows, int initialIndex);
    void updateSelectedIndex(int index);
    winid_t getSelectedWindowId() const;

private:
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_contentWidget = nullptr;
    QHBoxLayout* m_contentLayout = nullptr;
    
    std::vector<AltTabWindowInfo> m_windowList;
    std::vector<QWidget*> m_tiles;
    int m_currentIndex = -1;
};

#endif      /* PANEL_ALT_TAB_H */
