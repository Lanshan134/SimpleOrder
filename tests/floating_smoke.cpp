#include "app/controller.h"
#include "wheelevent.h"

// Runs last: it replaces the test database's items and leaves the settings it changes behind.
void Controller::floatingSmokeTest(const QString &directory) {
    const auto require = [](bool ok, const QString &label) {
        if (!ok)
            throw std::runtime_error(label.toStdString());
    };
    const auto settle = [] {
        for (int pass = 0; pass < 4; ++pass)
            QApplication::processEvents();
    };
    const QList<int> counts{0, 1, 2, 3, 4, 5, 8};
    const QDate monday(2026, 11, 2);
    const auto dayWith = [&](int count) { return monday.addDays(counts.indexOf(count)); };
    dirty = false;
    selected = 0;
    inspector->hide();
    store.clearItems();
    for (int count : counts)
        for (int n = 0; n < count; ++n) {
            Item item;
            item.title = QString("当天事项 %1-%2").arg(count).arg(n + 1);
            item.date = dayWith(count).toString(Qt::ISODate);
            store.save(item);
        }
    settings.setValue("fontSize", 15);
    settings.setValue("opacity", 100);
    settings.setValue("textOpacity", 100);
    settings.setValue("frameless", true);
    applyFloatingAppearance();
    floating->show();
    stack->setCurrentIndex(1);
    refresh();
    auto page = floating->findChild<QScrollArea *>("floatingCalendarScroll");
    auto pageBar = page->verticalScrollBar(), listBar = floatDay->verticalScrollBar();
    const auto resize = [&](QSize size, bool scale) {
        settings.setValue("scaleWithWindow", scale);
        floating->resize(size);
        scaleFloating(true);
        settle();
    };

    // The day list follows the selected date, the window and new items (#16).
    for (bool scale : {false, true})
        for (QSize size : {QSize(390, 340), QSize(420, 480), QSize(420, 900), QSize(630, 720)}) {
            resize(size, scale);
            for (int count : counts) {
                setDate(dayWith(count));
                settle();
                const QString context = QString(" (%1x%2 scale=%3 items=%4)")
                                            .arg(size.width())
                                            .arg(size.height())
                                            .arg(scale)
                                            .arg(count);
                require(floatDay->count() == count &&
                            floatDay->height() == qBound(1, count, 4) * scaled(floating, ROW_HEIGHT),
                        "day list height follows its items" + context);
                require((listBar->maximum() > 0) == (count > 4), "only long day lists scroll" + context);
                pageBar->setValue(pageBar->maximum());
                settle();
                const int listBottom = floatDay->mapTo(page->viewport(), QPoint(0, floatDay->height())).y();
                require(qAbs(page->viewport()->height() - listBottom) <= 1,
                        "day list ends the calendar page" + context);
                auto bar = stack->currentWidget()->findChild<QWidget *>("quickAddBar");
                require(floating->rect().contains(QRect(bar->mapTo(floating, QPoint()), bar->size())),
                        "quick add stays reachable" + context);
            }
        }
    resize(QSize(420, 480), false);
    setDate(dayWith(1));
    settle();
    const int calendarWithOne = floatCal->height();
    setDate(dayWith(8));
    settle();
    require(calendarWithOne - floatCal->height() == 3 * scaled(floating, ROW_HEIGHT),
            "the calendar takes the rows a short day list gives up");
    setDate(dayWith(0));
    settle();
    floating->grab().save(directory + "/floating-day-empty.png");
    const QDate fresh = monday.addDays(20);
    for (int n = 1; n <= 5; ++n) {
        add(QString("新增事项 %1").arg(n), fresh);
        settle();
        require(floatDay->height() == qMin(n, 4) * scaled(floating, ROW_HEIGHT),
                "day list grows as items are added");
    }
    // The page shows the window's own translucent surface, not an opaque panel of its own.
    settings.setValue("opacity", 20);
    applyFloatingAppearance();
    settle();
    const QPoint gap = floatCal->parentWidget()->mapTo(floating, QPoint(4, floatCal->geometry().bottom() + 3));
    const auto translucent = floating->grab().toImage();
    require(translucent.pixelColor(gap).alpha() ==
                translucent.pixelColor(5, floating->height() / 2).alpha(),
            "calendar page background follows the window opacity");
    settings.setValue("opacity", 100);
    applyFloatingAppearance();

    // Over the floating calendar page a wheel event scrolls the list or the page, never the month (#15).
    auto grid = floatCal->findChild<QAbstractItemView *>();
    const auto months = [&] { return QDate(floatCal->yearShown(), floatCal->monthShown(), 1); };
    const auto prepare = [&](int count, bool atEnd) {
        setDate(dayWith(count));
        floatCal->setCurrentPage(monday.year(), monday.month());
        settle();
        pageBar->setValue(atEnd ? pageBar->maximum() : 0);
        listBar->setValue(atEnd ? listBar->maximum() : 0);
        settle();
    };
    // Delivered to whatever lies under the point, as the window system would.
    const auto wheelAt = [&](QWidget *widget, QPoint local, int angle) {
        const QPoint at = widget->mapTo(floating, local);
        require(QRect(page->viewport()->mapTo(floating, QPoint()), page->viewport()->size()).contains(at),
                "wheel target is on screen");
        auto target = floating->childAt(at);
        sendWheel(target, target->mapFrom(floating, at), angle);
        require(months() == monday.addDays(1 - monday.day()) && floatCal->selectedDate() == day &&
                    cal->monthShown() == monday.month(),
                "wheel over the floating calendar page keeps the month and the selected date");
    };
    for (QSize size : {QSize(390, 340), QSize(420, 480), QSize(420, 900)})
        for (int count : {0, 3, 8}) {
            resize(size, false);
            for (int angle : {-120, 120, -15})
                for (bool atEnd : {false, true}) {
                    prepare(count, atEnd);
                    const int before = pageBar->value(), listBefore = listBar->value();
                    wheelAt(grid->viewport(), QPoint(60, atEnd ? grid->viewport()->height() - 12 : 60), angle);
                    require(listBar->value() == listBefore &&
                                (angle < 0 ? pageBar->value() > before || before == pageBar->maximum()
                                           : pageBar->value() < before || before == 0),
                            "wheel over the grid scrolls the page");
                }
        }
    // The short page: the list moves first, the event that reaches its end moves only the list, the next one
    // moves only the page. The same holds on the way back.
    resize(QSize(390, 340), false);
    prepare(8, false);
    require(pageBar->maximum() > 60 && listBar->maximum() == 4 * scaled(floating, ROW_HEIGHT),
            "both the page and the day list scroll");
    for (int direction : {-1, 1}) {
        listBar->setValue(direction < 0 ? 0 : listBar->maximum());
        pageBar->setValue(pageBar->maximum() / 2);
        settle();
        const int pageStart = pageBar->value(), limit = direction < 0 ? listBar->maximum() : 0;
        for (int events = 0; listBar->value() != limit; ++events) {
            require(events < 5, "day list reaches its end");
            wheelAt(floatDay->viewport(), QPoint(60, 10), 120 * direction);
            require(pageBar->value() == pageStart, "a wheel event that moves the day list leaves the page");
        }
        wheelAt(floatDay->viewport(), QPoint(60, 10), 120 * direction);
        require(listBar->value() == limit && (direction < 0 ? pageBar->value() > pageStart
                                                            : pageBar->value() < pageStart),
                "at its end the day list hands the wheel to the page");
    }
    // A list without a range of its own, and the space around the widgets, scroll the page.
    prepare(3, true);
    wheelAt(floatDay->viewport(), QPoint(60, 10), 120);
    require(pageBar->value() < pageBar->maximum(), "wheel over a short day list scrolls the page");
    prepare(3, true);
    wheelAt(floatCal->parentWidget(), QPoint(60, floatCal->geometry().bottom() + 3), 120);
    require(pageBar->value() < pageBar->maximum(), "wheel between the grid and the list scrolls the page");
    prepare(3, false);
    wheelAt(floatCal->titleButton, QPoint(10, 8), -120);
    require(pageBar->value() > 0, "wheel over the month bar scrolls the page");
    // Touchpad steps are carried over instead of being rounded away.
    prepare(8, false);
    pageBar->setValue(pageBar->maximum());
    settle();
    for (int n = 0; n < 40; ++n)
        wheelAt(floatDay->viewport(), QPoint(60, 10), -1);
    require(qAbs(listBar->value() - scaled(floating, ROW_HEIGHT)) <= 1, "small wheel steps add up");
    // Months change through the buttons, and the manager calendar keeps its wheel.
    prepare(3, false);
    const auto arrows = floatCal->findChildren<QPushButton *>("calendarArrow");
    require(arrows.size() == 2, "month buttons exist");
    for (auto arrow : arrows) {
        const bool next = arrow->accessibleName() == "下个月";
        floatCal->setCurrentPage(monday.year(), monday.month());
        arrow->click();
        require(months() == QDate(2026, next ? 12 : 10, 1), "month buttons change the month");
    }
    cal->setCurrentPage(2026, 11);
    sendWheel(cal->findChild<QAbstractItemView *>()->viewport(), QPoint(60, 60), -120);
    require(cal->monthShown() == 12 && cal->selectedDate() == day, "manager calendar still turns on the wheel");
    cal->setCurrentPage(2026, 11);
    floatCal->setCurrentPage(2026, 11);
}
