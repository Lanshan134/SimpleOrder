#include "ui/widgets/calendar.h"
#include "ui/widgets/widgettools.h"
#include "ui/theme/appearance.h"
#include <QtWidgets>

Calendar::Calendar(QWidget *parent) : QCalendarWidget(parent) {
    setFirstDayOfWeek(Qt::Monday);
    setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    setGridVisible(false);
    // One plain title with its controls beside it, instead of Qt's split month and year buttons.
    setNavigationBarVisible(false);
    setAcceptDrops(true);
    setMinimumSize(330, 250);
    auto table = findChild<QAbstractItemView *>();
    if (table) {
        table->setAcceptDrops(false);
        table->viewport()->setAcceptDrops(false);
        table->viewport()->installEventFilter(this);
        table->viewport()->setMouseTracking(true);
    }
    auto bar = new QWidget(this);
    auto barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(0, 0, 0, 4);
    barLayout->setSpacing(2);
    titleButton = button("", "calendarTitle");
    titleButton->setToolTip("选择年份和月份");
    todayButton = button("今天");
    auto previous = glyphButton(Glyph::Left, "上个月", "calendarArrow");
    auto next = glyphButton(Glyph::Right, "下个月", "calendarArrow");
    barLayout->addWidget(titleButton);
    barLayout->addStretch();
    barLayout->addWidget(todayButton);
    barLayout->addWidget(previous);
    barLayout->addWidget(next);
    if (auto column = qobject_cast<QVBoxLayout *>(layout()))
        column->insertWidget(0, bar);
    connect(titleButton, &QPushButton::clicked, this, [this] { chooseMonth(); });
    connect(todayButton, &QPushButton::clicked, this, [this] { setSelectedDate(QDate::currentDate()); });
    connect(previous, &QPushButton::clicked, this, &QCalendarWidget::showPreviousMonth);
    connect(next, &QPushButton::clicked, this, &QCalendarWidget::showNextMonth);
    connect(this, &QCalendarWidget::currentPageChanged, this, [this] { updateHeader(); });
    connect(this, &QCalendarWidget::selectionChanged, this, [this] { updateHeader(); });
    updateHeader();
    QTextCharFormat weekend;
    weekend.setForeground(theme::color("ink-soft"));
    setWeekdayTextFormat(Qt::Saturday, weekend);
    setWeekdayTextFormat(Qt::Sunday, weekend);
    QTextCharFormat header;
    header.setForeground(theme::color("ink-muted"));
    header.setBackground(Qt::transparent);
    header.setFontWeight(QFont::Normal);
    setHeaderTextFormat(header);
}
void Calendar::paintCell(QPainter *p, const QRect &r, const QDate &d) const {
    const bool outside = d.month() != monthShown();
    if (!displayOutside && outside)
        return;
    const qreal scale = widgetScale(this);
    auto table = findChild<QAbstractItemView *>();
    const bool isToday = d == QDate::currentDate(), isSelected = d == selectedDate();
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const QRectF cell = QRectF(r).adjusted(2 * scale, 2 * scale, -2 * scale, -2 * scale);
    if (isSelected || d == dropDate || (d == hoverDate && table)) {
        p->setPen(Qt::NoPen);
        p->setBrush(surfaceColor(this, theme::color(isSelected || d == dropDate ? "selection" : "row-hover"),
                                 "surfaceOpacity"));
        p->drawRoundedRect(cell, 6 * scale, 6 * scale);
    }
    // The view's outline is off in style.qss; mark the keyboard position and the drop target.
    if ((isSelected && table && table->hasFocus()) || d == dropDate) {
        p->setPen(QPen(theme::color("focus"), 2 * scale));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(QRectF(r).adjusted(scale, scale, -scale, -scale), 6 * scale, 6 * scale);
    }
    QFont numberFont = table ? table->font() : font();
    numberFont.setWeight(isToday ? QFont::DemiBold : QFont::Normal);
    p->setFont(numberFont);
    const QFontMetricsF metrics(numberFont);
    // The circle hugs the digits, so it stays clear of the marks below at every text size.
    // A calendar that never shows marks, like the date picker, centres its numbers instead of leaving room.
    const bool hasMarks = !items.isEmpty();
    const qreal radius =
        hasMarks ? qMin(metrics.capHeight() * .95, (r.height() - 11 * scale) / 2) : metrics.capHeight() * .95;
    const QPointF numberCentre(r.center().x() + .5,
                               hasMarks ? r.top() + qMax(radius + 3 * scale, r.height() * .38) : QRectF(r).center().y());
    if (isToday) {
        p->setPen(Qt::NoPen);
        p->setBrush(foregroundColor(this, theme::color("accent")));
        p->drawEllipse(numberCentre, radius, radius);
    }
    const auto weekdayInk = weekdayTextFormat(Qt::DayOfWeek(d.dayOfWeek())).foreground();
    p->setPen(foregroundColor(this, isToday      ? QColor(Qt::white)
                                    : outside    ? theme::color("ink-outside")
                                    : isSelected ? theme::color("ink")
                                    : weekdayInk.style() != Qt::NoBrush ? weekdayInk.color()
                                                                        : theme::color("ink")));
    p->drawText(QRectF(r.left(), numberCentre.y() - metrics.height() / 2, r.width(), metrics.height()),
                Qt::AlignCenter, QString::number(d.day()));
    p->restore();
    const QRect area(qRound(r.x() / scale), qRound(r.y() / scale), qRound(r.width() / scale),
                     qRound(r.height() / scale));
    QList<Item> hits;
    for (const auto &stored : items) {
        auto i = planState(stored, d);
        if ((!i.done ||
             (showCompleted && visibleInActive(i, QDateTime::currentDateTimeUtc(), retentionHours))) &&
            planOccurs(i, d) && (i.type == "task" || i.showCalendar))
            hits << i;
    }
    if (hits.isEmpty())
        return;
    int shown = hits.size() > maxDots ? qMax(0, maxDots - 1) : hits.size(),
        width = shown * 8 + (hits.size() > maxDots ? 20 : 0), left = area.center().x() - width / 2;
    p->save();
    p->scale(scale, scale);
    p->setRenderHint(QPainter::Antialiasing);
    for (int n = 0; n < shown; ++n) {
        auto i = hits[n];
        p->setOpacity(i.done ? 0.4 : 1.0);
        const QColor mark = foregroundColor(this, groupColor(i.group));
        p->setPen(QPen(mark, 1));
        p->setBrush(i.type == "plan" ? QBrush(Qt::NoBrush) : QBrush(mark));
        p->drawEllipse(QPointF(left + n * 8 + 3, area.bottom() - 4), 2.5, 2.5);
    }
    if (hits.size() > maxDots) {
        QFont font = p->font();
        font.setPointSize(7);
        p->setFont(font);
        p->setPen(foregroundColor(this, theme::color("ink-muted")));
        p->drawText(QRect(left + shown * 8, area.bottom() - 14, 25, 14), Qt::AlignVCenter,
                    "+" + QString::number(hits.size() - shown));
    }
    p->restore();
}
void Calendar::updateHeader() {
    if (!titleButton)
        return;
    titleButton->setText(QString("%1 年 %2 月").arg(yearShown()).arg(monthShown()));
    const QDate today = QDate::currentDate();
    todayButton->setEnabled(selectedDate() != today || yearShown() != today.year() ||
                            monthShown() != today.month());
}
void Calendar::chooseMonth() {
    QMenu menu(this);
    menu.addAction(QString("上一年（%1）").arg(yearShown() - 1),
                   [this] { setCurrentPage(yearShown() - 1, monthShown()); });
    menu.addAction(QString("下一年（%1）").arg(yearShown() + 1),
                   [this] { setCurrentPage(yearShown() + 1, monthShown()); });
    menu.addSeparator();
    for (int month = 1; month <= 12; ++month) {
        auto action = menu.addAction(QString("%1 月").arg(month),
                                     [this, month] { setCurrentPage(yearShown(), month); });
        action->setCheckable(true);
        action->setChecked(month == monthShown());
    }
    menu.exec(titleButton->mapToGlobal(QPoint(0, titleButton->height())));
}
QDate Calendar::dateAt(const QPoint &p) const {
    auto table = findChild<QAbstractItemView *>();
    if (!table)
        return {};
    auto idx = table->indexAt(table->viewport()->mapFrom(this, p));
    const int firstColumn = verticalHeaderFormat() == QCalendarWidget::NoVerticalHeader ? 0 : 1;
    if (!idx.isValid() || idx.row() < 1 || idx.column() < firstColumn)
        return {};
    int firstNumber = table->model()->index(1, firstColumn).data().toInt();
    QDate first(yearShown(), monthShown(), 1);
    if (firstNumber > 7)
        first = first.addMonths(-1);
    first = QDate(first.year(), first.month(), firstNumber);
    const auto result = first.addDays((idx.row() - 1) * 7 + idx.column() - firstColumn);
    return !displayOutside && result.month() != monthShown() ? QDate() : result;
}
bool Calendar::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto mouse = static_cast<QMouseEvent *>(event);
        auto viewport = qobject_cast<QWidget *>(watched);
        if (mouse->button() == Qt::LeftButton && viewport &&
            dateAt(viewport->mapTo(this, mouse->pos())).isValid()) {
            mouseDateSelection = true;
            if (dateGesture)
                dateGesture(true);
        }
    }
    if (event->type() == QEvent::MouseButtonRelease && mouseDateSelection) {
        QTimer::singleShot(0, this, [this] {
            mouseDateSelection = false;
            if (dateGesture)
                dateGesture(false);
        });
    }

    if (event->type() == QEvent::MouseMove || event->type() == QEvent::Leave) {
        QDate under;
        if (auto viewport = qobject_cast<QWidget *>(watched); viewport && event->type() == QEvent::MouseMove)
            under = dateAt(viewport->mapTo(this, static_cast<QMouseEvent *>(event)->pos()));
        if (under != hoverDate) {
            hoverDate = under;
            updateCells();
        }
    }
    if (!displayOutside &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
         event->type() == QEvent::MouseButtonDblClick)) {
        auto viewport = qobject_cast<QWidget *>(watched);
        auto mouse = static_cast<QMouseEvent *>(event);
        if (viewport && !dateAt(viewport->mapTo(this, mouse->pos())).isValid())
            return true;
    }
    return QCalendarWidget::eventFilter(watched, event);
}
void Calendar::dragEnterEvent(QDragEnterEvent *e) {
    if (previewOnly) {
        e->ignore();
        return;
    }
    if (e->mimeData()->hasFormat("application/x-lighttask-id"))
        e->acceptProposedAction();
}
void Calendar::dragMoveEvent(QDragMoveEvent *e) {
    if (previewOnly) {
        e->ignore();
        return;
    }
    const QDate target = e->mimeData()->hasFormat("application/x-lighttask-id") ? dateAt(e->pos()) : QDate();
    if (target != dropDate) {
        dropDate = target;
        updateCells();
    }
    if (target.isValid())
        e->acceptProposedAction();
    else
        e->ignore();
}
void Calendar::dragLeaveEvent(QDragLeaveEvent *) {
    dropDate = QDate();
    updateCells();
}
void Calendar::dropEvent(QDropEvent *e) {
    dropDate = QDate();
    updateCells();
    if (previewOnly) {
        e->ignore();
        return;
    }
    auto d = dateAt(e->pos());
    if (d.isValid() && e->mimeData()->hasFormat("application/x-lighttask-id")) {
        if (schedule)
            schedule(e->mimeData()->data("application/x-lighttask-id").toLongLong(), d);
        e->acceptProposedAction();
    }
}
