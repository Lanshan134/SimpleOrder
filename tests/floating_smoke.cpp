#include "app/controller.h"
#include "wheelevent.h"

namespace {
// Where a scrollbar's style draws its handle.
struct BarAccess : QScrollBar {
    static QRect handle(QScrollBar *bar) {
        QStyleOptionSlider option;
        const auto init = &BarAccess::initStyleOption;
        (bar->*init)(&option);
        return bar->style()->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider, bar);
    }
};
} // namespace

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
    // A hover repaints only the cells it leaves and enters, with and without the halo, and the halo outlines
    // such a part exactly as it does the whole page (#2).
    {
        auto grid = floatCal->findChild<QTableView *>();
        struct PaintSpy : QObject {
            QRect painted;
            bool eventFilter(QObject *, QEvent *event) override {
                if (event->type() == QEvent::Paint)
                    painted |= static_cast<QPaintEvent *>(event)->rect();
                return false;
            }
        } spy;
        grid->viewport()->installEventFilter(&spy);
        const QRect from = grid->visualRect(grid->model()->index(3, 2)),
                    to = grid->visualRect(grid->model()->index(3, 3));
        const auto hover = [&](QPoint at) {
            QMouseEvent move(QEvent::MouseMove, at, grid->viewport()->mapToGlobal(at), Qt::NoButton,
                             Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(grid->viewport(), &move);
            settle();
        };
        for (int opacity : {20, 100}) {
            settings.setValue("opacity", opacity);
            applyFloatingAppearance();
            settle();
            hover(from.center());
            spy.painted = {};
            hover(to.center());
            require(!spy.painted.isEmpty() && (from | to).adjusted(-8, -8, 8, 8).contains(spy.painted),
                    QString("a hover repaints only its two cells (opacity %1, painted %2x%3)")
                        .arg(opacity)
                        .arg(spy.painted.width())
                        .arg(spy.painted.height()));
        }
        settings.setValue("opacity", 20);
        applyFloatingAppearance();
        settle();
        QPoint corner = grid->viewport()->mapTo(floating, from.topLeft());
        corner = QPoint(corner.x() / 4 * 4, corner.y() / 4 * 4);
        const QRect part(corner, from.size() * 2);
        const QImage whole = floating->grab().toImage().convertToFormat(QImage::Format_ARGB32);
        const QImage piece = floating->grab(part).toImage().convertToFormat(QImage::Format_ARGB32);
        const qreal ratio = whole.devicePixelRatio();
        const QImage expected = whole.copy(QRect(part.topLeft() * ratio, piece.size()));
        int differ = 0;
        for (int y = 0; y < piece.height(); ++y)
            for (int x = 0; x < piece.width(); ++x) {
                const QRgb a = piece.pixel(x, y), b = expected.pixel(x, y);
                differ += qMax(qMax(qAbs(qRed(a) - qRed(b)), qAbs(qGreen(a) - qGreen(b))),
                               qMax(qAbs(qBlue(a) - qBlue(b)), qAbs(qAlpha(a) - qAlpha(b)))) > 8;
            }
        require(
            differ == 0,
            QString("the halo outlines a repainted part like the whole page (%1 pixels differ)").arg(differ));
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(grid->viewport(), &leave);
        grid->viewport()->removeEventFilter(&spy);
    }
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
    wheelAt(floatCal->yearButton, QPoint(10, 8), -120);
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

    // Stretching the window never pushes content sideways or leaves a strip under it (#9).
    Item plan;
    plan.title = "悬浮窗布局计划";
    plan.type = "plan";
    plan.target = 3;
    store.save(plan);
    refresh();
    setDate(dayWith(8));
    auto list = floating->findChild<QScrollArea *>("floatingScroll");
    const auto inside = [&](QWidget *widget, QWidget *frame) {
        const QPoint at = widget->mapTo(frame, QPoint());
        return at.x() >= 0 && at.x() + widget->width() <= frame->width();
    };
    const auto checkWidth = [&](const QString &context) {
        for (int index : {0, 1}) {
            stack->setCurrentIndex(index);
            settle();
            auto scroll = index ? page : list;
            require(scroll->horizontalScrollBar()->maximum() == 0 && !scroll->horizontalScrollBar()->isVisible(),
                    "floating page fits its width" + context);
            for (auto rows : scroll->findChildren<TaskList *>())
                require(!rows->isVisible() || inside(rows, scroll->viewport()),
                        "floating lists stay inside the page" + context);
            auto bar = stack->currentWidget()->findChild<QWidget *>("quickAddBar");
            require(floating->height() - bar->mapTo(floating, QPoint(0, bar->height())).y() ==
                        floating->layout()->contentsMargins().bottom(),
                    "quick add keeps the bottom margin" + context);
            require(qAbs(bar->y() - scroll->geometry().bottom() - 1) <= scaled(floating, 8),
                    "nothing sits between the page and quick add" + context);
        }
        for (auto arrow : arrows)
            require(inside(arrow, page->viewport()), "month buttons stay visible" + context);
        const QRect sunday = grid->visualRect(grid->model()->index(2, 6));
        require(grid->viewport()->rect().contains(sunday.center()) && inside(grid, page->viewport()) &&
                    sunday.width() >= QFontMetrics(grid->font()).horizontalAdvance(grid->model()->index(0, 6).data().toString()),
                "all seven weekdays stay visible and their names fit" + context);
    };
    for (int font : {15, 22})
        for (bool scale : {true, false}) {
            settings.setValue("fontSize", font);
            for (QSize size : {QSize(390, 340), QSize(420, 480), QSize(390, 700), QSize(420, 900), QSize(390, 1200),
                               QSize(420, 1200), QSize(800, 1000)}) {
                resize(size, scale);
                checkWidth(QString(" (%1x%2 font=%3 scale=%4)")
                               .arg(size.width())
                               .arg(size.height())
                               .arg(font)
                               .arg(scale));
                if (size == QSize(420, 900) || size == QSize(390, 1200))
                    floating->grab().save(directory + QString("/floating-tall-%1-%2-%3.png")
                                                          .arg(size.width())
                                                          .arg(font)
                                                          .arg(scale));
            }
        }
    settings.setValue("fontSize", 15);
    settings.setValue("frameless", false);
    applyFloatingAppearance();
    resize(QSize(420, 900), true);
    checkWidth(" (framed)");
    require(qAbs(floatingScale - qSqrt(900.0 / 480.0)) < .01, "a moderately tall window scales by its area");
    settings.setValue("frameless", true);
    applyFloatingAppearance();
    const auto metrics = [&] {
        resize(QSize(420, 480), true);
        QList<int> values{qRound(floatingScale * 1000), tasks->font().pixelSize(), floatDay->height(),
                          floatCal->height(), floating->layout()->contentsMargins().bottom()};
        stack->setCurrentIndex(0);
        settle();
        values << tasks->height() << plans->height();
        stack->setCurrentIndex(1);
        settle();
        return values;
    };
    const auto original = metrics();
    require(original.first() == 1000, "the default window keeps its exact scale");
    for (int round = 0; round < 2; ++round) {
        for (int height = 480; height <= 1200; height += 72)
            floating->resize(420, height);
        for (int height = 1200; height >= 340; height -= 86)
            floating->resize(390, height);
        require(metrics() == original, "repeated stretching leaves no residue");
    }
}

// The page's scrollbar and the day list's are told apart by place, width and track (#21).
void Controller::scrollbarSmokeTest(const QString &directory) {
    const auto require = [](bool ok, const QString &label) {
        if (!ok)
            throw std::runtime_error(label.toStdString());
    };
    const auto settle = [] {
        for (int pass = 0; pass < 4; ++pass)
            QApplication::processEvents();
    };
    const QDate busy(2026, 12, 1), quiet(2026, 12, 2);
    for (int n = 0; n < 20; ++n) {
        Item item;
        item.title = QString("滚动条事项 %1").arg(n + 1);
        item.date = busy.toString(Qt::ISODate);
        store.save(item);
    }
    Item one;
    one.title = "单条事项";
    one.date = quiet.toString(Qt::ISODate);
    store.save(one);
    refresh();
    settings.setValue("scaleWithWindow", false);
    floating->show();
    stack->setCurrentIndex(1);
    auto page = floating->findChild<QScrollArea *>("floatingCalendarScroll");
    auto outer = page->verticalScrollBar(), inner = floatDay->verticalScrollBar();
    const auto box = [&](QWidget *widget) { return QRect(widget->mapTo(floating, QPoint()), widget->size()); };
    const auto show = [&](QSize size, QDate date, int opacity) {
        settings.setValue("opacity", opacity);
        applyFloatingAppearance();
        floating->resize(size);
        scaleFloating(true);
        setDate(date);
        settle();
        outer->setValue(outer->maximum());
        inner->setValue(0);
        settle();
        return floating->grab().toImage();
    };
    auto image = show(QSize(390, 340), busy, 100);
    require(outer->isVisible() && inner->isVisible(), "both bars show on a short page with a long list");
    const QRect pageBar = box(outer), listBar = box(inner), list = box(floatDay);
    require(pageBar.width() >= 2 * listBar.width() && listBar.right() < pageBar.left() &&
                2 * pageBar.height() > 3 * listBar.height() && list.contains(listBar),
            "the page bar is wide and runs the page; the list bar is thin and inside the list");
    floating->grab().save(directory + "/floating-scrollbars.png");
    const QColor track = image.pixelColor(pageBar.center().x() + 2, pageBar.top() + 3);
    const QColor panel = image.pixelColor(list.left() + 3, list.bottom() - 3);
    const QColor surface = image.pixelColor(page->mapTo(floating, QPoint(2, 2)));
    require(track.rgb() == theme::color("line").rgb() && panel.rgb() == theme::color("canvas").rgb() &&
                surface.rgb() != panel.rgb() && surface.rgb() != track.rgb(),
            "the page bar has a track and the day list sits on its own panel");
    require(image.pixelColor(pageBar.left() + 1, pageBar.top() + 3).rgb() != track.rgb(),
            "a gap separates the page bar from the list");
    // Dragging each handle moves only what it belongs to; the press goes to whatever lies under the handle.
    const auto drag = [&](QScrollBar *bar, int distance) {
        const QPoint start = bar->mapTo(floating, BarAccess::handle(bar).center());
        auto target = floating->childAt(start);
        require(target == bar, "the handle is under the pointer");
        const auto send = [&](QEvent::Type type, QPoint at, Qt::MouseButton button, Qt::MouseButtons held) {
            const QPoint local = target->mapFrom(floating, at);
            QMouseEvent event(type, local, target->mapToGlobal(local), button, held, Qt::NoModifier);
            QApplication::sendEvent(target, &event);
        };
        send(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 4; ++step)
            send(QEvent::MouseMove, start + QPoint(0, distance * step / 4), Qt::NoButton, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, start + QPoint(0, distance), Qt::LeftButton, Qt::NoButton);
        settle();
    };
    const auto listTop = [&] { return floatDay->mapTo(page->viewport(), QPoint()).y(); };
    const auto firstRow = [&] { return floatDay->indexAt(QPoint(4, 4)).row(); };
    const int pageEnd = outer->value(), topAtEnd = listTop();
    drag(inner, 24);
    const int listAt = inner->value(), rowAt = firstRow();
    require(listAt > 0 && rowAt > 0 && outer->value() == pageEnd && listTop() == topAtEnd,
            "dragging the list bar scrolls the list and leaves the page");
    drag(outer, -40);
    require(outer->value() < pageEnd && listTop() > topAtEnd && inner->value() == listAt &&
                firstRow() == rowAt,
            "dragging the page bar scrolls the page and leaves the list");
    // Alone, each bar still reads as its region's.
    show(QSize(420, 480), busy, 100);
    require(!outer->isVisible() && inner->isVisible() && box(floatDay).contains(box(inner)),
            "the list bar alone stays inside the list panel");
    floating->grab().save(directory + "/floating-scrollbar-list.png");
    show(QSize(390, 340), quiet, 100);
    require(outer->isVisible() && !inner->isVisible() && box(outer).height() > box(floatDay).height(),
            "the page bar alone runs the page");
    // Handles stay visible on a fully see-through window.
    image = show(QSize(390, 340), busy, 0);
    const QRect handleSpot = box(inner);
    require(image.pixelColor(box(outer).center().x() + 2, box(outer).bottom() - 10).alpha() >= 170 &&
                image.pixelColor(handleSpot.center().x(), handleSpot.top() + 8).alpha() >= 170,
            "scrollbar handles stay visible without a background");
    floating->grab().save(directory + "/floating-scrollbars-clear.png");
    settings.setValue("opacity", 100);
    applyFloatingAppearance();
}

// Stretching the editor gives the height to the notes instead of an empty band (#9).
void Controller::editorLayoutSmokeTest(const QString &directory) {
    QStringList failures;
    const auto check = [&](bool ok, const QString &label) {
        if (!ok)
            failures << label;
    };
    const auto settle = [] {
        for (int pass = 0; pass < 6; ++pass)
            QApplication::processEvents();
    };
    const auto below = [](QWidget *widget, QWidget *root) {
        return widget->mapTo(root, QPoint(0, widget->height())).y();
    };
    const auto verify = [&](QWidget *root, const QString &context) {
        auto scroll = qobject_cast<QScrollArea *>(editor->parentWidget()->parentWidget());
        auto bar = scroll->verticalScrollBar();
        check(notes->height() >= 64, "notes keep two lines" + context);
        check(!planEditor->isVisible() || planEditor->height() <= planEditor->sizeHint().height() + 1,
              "plan fields are not spread out" + context);
        check(saveButton->mapTo(root, QPoint()).y() >= below(scroll, root) && root->rect().contains(QRect(
                  saveButton->mapTo(root, QPoint()), saveButton->size())), "actions stay reachable" + context);
        // The dialog's minimum width does not follow the text size, so larger text needs more than that.
        check(root->width() < 360 * theme::textScale || scroll->horizontalScrollBar()->maximum() == 0,
              "editor has no sideways overflow" + context);
        if (bar->maximum() == 0) {
            const int blank = saveButton->mapTo(root, QPoint()).y() - below(pinned, root);
            check(blank >= 0 && blank <= 24, "no empty band above the actions" + context + QString::number(blank));
        } else {
            check(notes->height() == 64, "notes give way before the form scrolls" + context);
            bar->setValue(bar->maximum());
            settle();
            check(below(pinned, scroll) <= scroll->height(), "scrolling reaches the last row" + context);
            bar->setValue(0);
        }
    };
    const QList<QJsonObject> rules{{},
                                   {{"kind", "quota"}, {"period", "week"}},
                                   {{"kind", "dates"}, {"mode", "week"}, {"days", QJsonArray{1, 3}}},
                                   {{"kind", "dates"}, {"mode", "month"}, {"days", QJsonArray{5, 31}}},
                                   {{"kind", "interval"}, {"first", "2026-11-02"}, {"interval", 3}, {"basis", "fixed"}},
                                   {{"kind", "total"}}};
    QList<qint64> ids;
    for (const auto &rule : rules) {
        Item item;
        item.title = "编辑窗口布局";
        item.type = rule.isEmpty() ? "task" : "plan";
        item.rule = rule;
        ids << store.save(item).id;
    }
    dirty = false;
    selected = 0;
    refresh();
    const auto appStyle = qApp->styleSheet();
    for (qreal text : {1.0, 1.5}) {
        theme::textScale = text;
        qApp->setStyleSheet(theme::scaleText(theme::baseStyle));
        scaleManager();
        for (int n = 0; n < ids.size(); ++n) {
            const QString kind = QString("%1-text%2").arg(n).arg(qRound(text * 100));
            QTimer::singleShot(0, this, [&] {
                auto dialog = itemWindow.data();
                if (!dialog)
                    return;
                int first = 0;
                for (QSize size : {QSize(560, 460), QSize(360, 460), QSize(560, 660), QSize(560, 1000), QSize(360, 1000),
                                   QSize(560, 460)}) {
                    dialog->resize(size);
                    settle();
                    verify(dialog, QString(" [window %1 %2x%3] ").arg(kind).arg(size.width()).arg(size.height()));
                    if (!first)
                        first = notes->height();
                    if (size == QSize(560, 1000) || size == QSize(360, 460))
                        dialog->grab().save(directory + QString("/editor-%1-%2.png").arg(kind).arg(size.height()));
                }
                check(notes->height() == first, "notes shrink back " + kind);
                dialog->close();
            });
            editFloatingItem(ids[n]);
            choose(ids[n]);
            settle();
            for (int height : {640, 1000, 640}) {
                manager->resize(1180, height);
                settle();
                verify(manager, QString(" [manager %1 %2] ").arg(kind).arg(height));
            }
            manager->resize(1180, 1000);
            settle();
            manager->grab().save(directory + QString("/manager-editor-%1.png").arg(kind));
        }
    }
    theme::textScale = 1.0;
    qApp->setStyleSheet(appStyle);
    scaleManager();
    closeEditor();
    if (!failures.isEmpty())
        throw std::runtime_error(failures.join("\n").toStdString());
}
