#pragma once
#include "domain/item.h"
#include <QCalendarWidget>
#include <functional>

class QPushButton;

class Calendar : public QCalendarWidget {
  public:
    bool showCompleted = true, displayOutside = true;
    bool previewOnly = false;
    // Expanded cells list their items by name when at least two rows fit; otherwise they keep the dots.
    bool expanded = false;
    // Off where the calendar sits in a scrolling page: the wheel then belongs to that page.
    bool wheelChangesMonth = true;
    bool mouseDateSelection = false;
    std::function<void(bool)> dateGesture;
    int retentionHours = 48, maxDots = 3;
    QList<Item> items;
    std::function<void(qint64, QDate)> schedule;
    QPushButton *yearButton = nullptr, *monthButton = nullptr, *todayButton = nullptr;
    QPushButton *expandButton = nullptr;
    explicit Calendar(QWidget *parent = nullptr);
    void setExpanded(bool);
    QList<Item> itemsOn(const QDate &) const;
    void repaintCells() {
        updateCells();
        updateHeader();
    }

  protected:
    bool eventFilter(QObject *, QEvent *) override;
    void paintCell(QPainter *, const QRect &, const QDate &) const override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void dropEvent(QDropEvent *) override;
    QDate dateAt(const QPoint &p) const;

  private:
    QDate hoverDate, dropDate;
    void updateHeader();
};
