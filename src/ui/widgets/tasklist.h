#pragma once
#include "domain/item.h"
#include <QListWidget>
#include <QMouseEvent>
#include <functional>

class QMenu;

class TaskList : public QListWidget {
    Qt::MouseButton pressedButton = Qt::LeftButton;
    // A click completes or edits only when it is pressed and released in the same zone of the row.
    bool pressedCheck = false, releasedCheck = false, pressedMovable = false;
    bool inCheck(const QPoint &p) const;

  public:
    std::function<void(qint64)> edit, contextEdit, complete, togglePin;
    QMenu *createItemMenu(qint64 id);
    explicit TaskList(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    void snapToRows();
    void fill(const QList<Item> &items, bool headings = false);
    void setPreviewOnly(bool enabled);

  protected:
    void mousePressEvent(QMouseEvent *event) override {
        pressedButton = event->button();
        pressedCheck = inCheck(event->pos());
        const auto row = itemAt(event->pos());
        pressedMovable = row && row->data(Qt::UserRole + 7).toBool();
        QListWidget::mousePressEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        releasedCheck = inCheck(event->pos());
        QListWidget::mouseReleaseEvent(event);
    }
    // The first click may already have reordered the rows; the second press must not act on another item.
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        pressedButton = Qt::NoButton;
        event->accept();
    }
    // Only a left press on an item with a date to move drags it to the calendar.
    void mouseMoveEvent(QMouseEvent *event) override {
        if (event->buttons() == Qt::NoButton || (pressedButton == Qt::LeftButton && pressedMovable))
            QListWidget::mouseMoveEvent(event);
    }
    void startDrag(Qt::DropActions) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
};
