#pragma once
#include "domain/item.h"
#include <QListWidget>
#include <QPointer>
#include <functional>

// Suggests categories beside an input while a `#` tag is typed in it. The list is a child of the
// input's window, so the input keeps focus and the list stays above a top-most or translucent window.
class CategoryPopup : public QObject {
    QWidget *edit;
    std::function<QString()> textBeforeCursor;
    std::function<void(int tagStart, const Category &)> accept;
    QPointer<QListWidget> list;
    int tagStart = -1, dismissed = -1;
    QString typed;
    bool composing = false;
    void choose(const QListWidgetItem *item);

  public:
    CategoryPopup(QWidget *edit, std::function<QString()> textBeforeCursor,
                  std::function<void(int, const Category &)> accept);
    bool isOpen() const {
        return list && list->isVisible();
    }
    void refresh();
    void hide();

  protected:
    bool eventFilter(QObject *, QEvent *) override;
};
