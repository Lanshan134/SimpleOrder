#include "app/controller.h"

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
}
