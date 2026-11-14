#pragma once
#include "domain/item.h"
#include <QCalendarWidget>
#include <functional>

class QPushButton;

class Calendar : public QCalendarWidget {
  public:
    bool showCompleted = true, displayOutside = true;
    bool previewOnly = false;
    bool mouseDateSelection = false;
    std::function<void(bool)> dateGesture;
    int retentionHours = 48, maxDots = 3;
    QList<Item> items;
    std::function<void(qint64, QDate)> schedule;
    QPushButton *titleButton = nullptr, *todayButton = nullptr;
    explicit Calendar(QWidget *parent = nullptr);
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
    void chooseMonth();
};
