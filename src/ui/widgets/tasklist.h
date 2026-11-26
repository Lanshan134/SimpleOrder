#pragma once
#include "domain/item.h"
#include <QListWidget>
#include <QMouseEvent>
#include <functional>

class QMenu;

// 昨天, 今天 or 明天, and empty for any other day.
QString relativeDay(const QDate &date, const QDate &today);
// Relative words for the days people scan for; urgency colours today (1), overdue (2) and tomorrow (3).
QString dateMarker(const QString &iso, const QDate &today, int *urgency);

class TaskList : public QListWidget {
    Qt::MouseButton pressedButton = Qt::LeftButton;
    qreal wheelRemainder = 0;

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
        QListWidget::mousePressEvent(event);
    }
    void startDrag(Qt::DropActions) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
};
