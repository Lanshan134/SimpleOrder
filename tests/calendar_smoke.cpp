#include "app/controller.h"

// Calendar navigation (#20) and expansion (#3). Runs last, after floatingSmokeTest; it replaces the items.
void Controller::calendarSmokeTest(const QString &directory) {
    const auto require = [](bool ok, const QString &label) {
        if (!ok)
            throw std::runtime_error(label.toStdString());
    };
    const auto settle = [] {
        for (int pass = 0; pass < 4; ++pass)
            QApplication::processEvents();
    };
    const auto page = [](Calendar *c) { return QDate(c->yearShown(), c->monthShown(), 1); };
    // Opens a header menu as a click would, then chooses an entry; returns what the menu offered.
    const auto choose = [](QPushButton *owner, const QString &entry) {
        emit owner->menu()->aboutToShow();
        const auto actions = owner->menu()->actions();
        QString checked;
        QAction *chosen = nullptr;
        for (auto action : actions) {
            if (action->isChecked())
                checked = action->text();
            if (action->text() == entry)
                chosen = action;
        }
        if (chosen)
            chosen->trigger();
        return QString("%1..%2 (%3) %4")
            .arg(actions.first()->text(), actions.last()->text())
            .arg(actions.size())
            .arg(checked);
    };
    const auto header = [](Calendar *c) { return c->todayButton->parentWidget(); };
    openEditor();
    settle();
    date->findChild<QAbstractButton *>("fieldCalendar")->click();
    auto popup = QApplication::activePopupWidget();
    require(popup && popup->findChild<Calendar *>(), "the date picker opens");
    for (auto c : {cal, floatCal, popup->findChild<Calendar *>()}) {
        const QString name = c == cal ? " (manager)" : c == floatCal ? " (floating)" : " (picker)";
        c->setCurrentPage(2026, 11);
        require(choose(c->yearButton, "2020 年") == "2020 年..2032 年 (13) 2026 年" &&
                    page(c) == QDate(2020, 11, 1) && c->yearButton->text() == "2020 年" &&
                    c->monthButton->text() == "11 月",
                "the year menu keeps the month" + name);
        require(choose(c->monthButton, "3 月") == "1 月..12 月 (12) 11 月" && page(c) == QDate(2020, 3, 1) &&
                    c->yearButton->text() == "2020 年" && c->monthButton->text() == "3 月",
                "the month menu keeps the year" + name);
        c->setCurrentPage(3, 5);
        require(choose(c->yearButton, {}).startsWith("1 年..9 年 (9)"),
                "the year menu starts at year 1" + name);
        c->setCurrentPage(9996, 5);
        require(choose(c->yearButton, {}).startsWith("9990 年..9999 年 (10)"),
                "the year menu ends at 9999" + name);
        const auto arrow = [c](const QString &label) {
            for (auto button : c->findChildren<QPushButton *>("calendarArrow"))
                if (button->accessibleName() == label)
                    return button;
            return static_cast<QPushButton *>(nullptr);
        };
        c->setCurrentPage(2026, 12);
        arrow("下个月")->click();
        require(page(c) == QDate(2027, 1, 1) && c->yearButton->text() == "2027 年" &&
                    c->monthButton->text() == "1 月",
                "next month crosses into the next year" + name);
        arrow("上个月")->click();
        arrow("上个月")->click();
        require(page(c) == QDate(2026, 11, 1) && c->yearButton->text() == "2026 年",
                "previous month crosses back" + name);
        require(header(c)->minimumSizeHint().width() <= c->width(), "the calendar header fits" + name);
    }
    popup->close();
    closeEditor();
    settle();
    // The menus are not drawn with the floating window's see-through stylesheet.
    settings.setValue("opacity", 0);
    applyFloatingAppearance();
    auto menu = floatCal->monthButton->menu();
    emit menu->aboutToShow();
    menu->ensurePolished();
    menu->adjustSize();
    const QImage shot = menu->grab().toImage();
    shot.save(directory + "/calendar-month-menu-clear.png");
    const QColor paper = shot.pixelColor(shot.width() - 8, shot.height() / 2);
    require(paper.alpha() == 255 && paper.lightness() > 230,
            "the month menu stays opaque on a see-through window");
    settings.setValue("opacity", 100);
    applyFloatingAppearance();
    // Larger text widens the header; the picker and the manager's calendar keep room for it.
    const auto appStyle = qApp->styleSheet();
    theme::textScale = 2.0;
    qApp->setStyleSheet(theme::scaleText(theme::baseStyle));
    scaleManager();
    openEditor();
    managerSplit->setSizes(
        {managerSplit->sizes()[0] + managerSplit->sizes()[1], 0, managerSplit->sizes()[2]});
    settle();
    require(header(cal)->minimumSizeHint().width() <= cal->width(),
            QString("the manager calendar header fits at 200% text (%1 in %2)")
                .arg(header(cal)->minimumSizeHint().width())
                .arg(cal->width()));
    date->findChild<QAbstractButton *>("fieldCalendar")->click();
    popup = QApplication::activePopupWidget();
    settle();
    auto picker = popup->findChild<Calendar *>();
    auto grid = picker->findChild<QTableView *>();
    int rows = 0;
    for (int row = 0; row < grid->model()->rowCount(); ++row)
        rows += grid->rowHeight(row);
    popup->grab().save(directory + "/date-picker-text-200.png");
    require(header(picker)->minimumSizeHint().width() <= picker->width() &&
                rows <= grid->viewport()->height(),
            "the date picker shows its whole header and grid at 200% text");
    popup->close();
    closeEditor();
    theme::textScale = 1.0;
    qApp->setStyleSheet(appStyle);
    scaleManager();
    settle();

    // Expansion (#3): cells name their items, the windows give the calendar height but keep their width.
    const QDate busy(2026, 11, 18);
    store.clearItems();
    for (int n = 0; n < 6; ++n) {
        Item item;
        item.title = QString("展开验证事项 %1").arg(n + 1);
        item.date = busy.toString(Qt::ISODate);
        item.time = n == 0 ? "09:30" : QString();
        store.save(item);
    }
    dirty = false;
    selected = 0;
    settings.setValue("scaleWithWindow", true);
    settings.setValue("fontSize", 15);
    floating->show();
    calendarView = true;
    stack->setCurrentIndex(1);
    setDate(busy);
    refresh();
    // The busy day's cell (18 appears once, in a middle row), whatever the first day of the week.
    const auto busyCell = [&](QTableView *grid) {
        for (int row = 2; row < 6; ++row)
            for (int column = 0; column < 7; ++column)
                if (grid->model()->index(row, column).data().toInt() == busy.day())
                    return grid->model()->index(row, column);
        return QModelIndex();
    };
    // Dark pixels in the lower half of the busy day's cell: dots alone are lighter than any text.
    const auto inkBelowNumber = [&](Calendar *c, const QString &shot) {
        settle();
        auto grid = c->findChild<QTableView *>();
        const QRect cell = grid->visualRect(busyCell(grid));
        const QImage image = c->grab().toImage();
        image.save(directory + "/" + shot);
        int ink = 0;
        const QPoint origin = grid->viewport()->mapTo(c, cell.topLeft());
        for (int y = cell.height() / 2; y < cell.height(); ++y)
            for (int x = 0; x < cell.width(); ++x)
                ink += image.pixelColor(origin + QPoint(x, y)).lightness() < 140;
        return ink;
    };
    require(floatCal->itemsOn(busy).size() == 6, "the calendar finds the day's items");
    floating->resize(420, 480);
    settle();
    const qreal compactScale = floatingScale;
    const int compactInk = inkBelowNumber(floatCal, "calendar-floating-compact.png");
    const QPoint compactAt = floating->pos();
    floatCal->expandButton->click();
    settle();
    auto scroller = floating->findChild<QScrollArea *>("floatingCalendarScroll");
    require(floating->size() == QSize(420, 720) && qAbs(floatingScale - compactScale) < .01 &&
                scroller->horizontalScrollBar()->maximum() == 0 &&
                settings.value("floatingCalendarExpanded").toBool(),
            "the floating window grows downward at the same width and scale");
    const int expandedInk = inkBelowNumber(floatCal, "calendar-floating-expanded-cal.png");
    require(expandedInk > compactInk + 40,
            QString("expanded cells name their items (%1 against %2)").arg(expandedInk).arg(compactInk));
    floating->grab().save(directory + "/calendar-floating-expanded.png");
    for (QSize size : {QSize(390, 340), QSize(390, 1200), QSize(800, 1000)}) {
        floating->resize(size);
        settle();
        require(scroller->horizontalScrollBar()->maximum() == 0 &&
                    floatCal->todayButton->parentWidget()->minimumSizeHint().width() <= floatCal->width(),
                QString("the expanded floating calendar fits %1x%2").arg(size.width()).arg(size.height()));
        if (size == QSize(390, 340))
            floating->grab().save(directory + "/calendar-floating-expanded-min.png");
    }
    floating->resize(420, 720);
    settle();
    floatCal->expandButton->click();
    settle();
    require(floating->size() == QSize(420, 480) && qAbs(floatingScale - compactScale) < .01 &&
                floating->pos() == compactAt && !settings.value("floatingCalendarExpanded").toBool(),
            "collapsing restores the floating window");
    // Near the bottom of the screen the window moves up to stay on screen, and back down when collapsed.
    const QRect screen = floating->screen()->availableGeometry();
    floating->move(screen.left() + 40, screen.bottom() - floating->frameGeometry().height() - 10);
    const QPoint low = floating->pos();
    floatCal->expandButton->click();
    settle();
    require(floating->frameGeometry().bottom() <= screen.bottom() && floating->y() < low.y(),
            "an expanded window stays on screen");
    floatCal->expandButton->click();
    settle();
    require(floating->pos() == low && floating->height() == 480, "collapsing moves the window back down");
    floating->move(compactAt);

    // The manager takes the height from the day list, never from the editor or its draft.
    manager->resize(1180, 780);
    openEditor();
    title->setText("展开时保留的草稿");
    notes->setPlainText("草稿备注");
    settle();
    const auto sizes = managerSplit->sizes();
    const int compactCalendar = cal->height(), compactDay = dayItems->height();
    const int managerInk = inkBelowNumber(cal, "calendar-manager-compact.png");
    cal->expandButton->click();
    settle();
    require(dayItems->height() == 2 * ROW_HEIGHT && cal->height() > compactCalendar + 150 &&
                managerSplit->sizes() == sizes && title->text() == "展开时保留的草稿" &&
                notes->toPlainText() == "草稿备注" && hasEditorChanges(),
            "the manager calendar grows into the day list and keeps the draft");
    require(inkBelowNumber(cal, "calendar-manager-expanded-cal.png") > managerInk + 40,
            "expanded manager cells name their items");
    auto weeks = cal->findChild<QTableView *>();
    require(2 * weeks->rowHeight(0) < weeks->rowHeight(1), "the weekday row keeps its text height");
    manager->grab().save(directory + "/calendar-manager-expanded-editor.png");
    // The tooltip spells out the whole day.
    grid = cal->findChild<QTableView *>();
    const QPoint busyAt = grid->visualRect(busyCell(grid)).center();
    QHelpEvent help(QEvent::ToolTip, busyAt, grid->viewport()->mapToGlobal(busyAt));
    QApplication::sendEvent(grid->viewport(), &help);
    const QString tip = QToolTip::text();
    QToolTip::hideText();
    // In the day lists' order: untimed items before timed ones.
    require(tip.count('\n') == 5 && tip.endsWith("\n09:30 展开验证事项 1"),
            "the tooltip lists the day's items: " + tip);
    cal->expandButton->click();
    settle();
    require(dayItems->height() == compactDay && cal->height() == compactCalendar &&
                title->text() == "展开时保留的草稿" && hasEditorChanges() && managerSplit->sizes() == sizes,
            "collapsing restores the manager and keeps the draft");
    dirty = false;
    fillEditor();
    closeEditor();
    // Drops still land on the cell under the pointer in the tall rows.
    expandCalendar(cal, true);
    settle();
    const qint64 moved = store.all().first().id;
    for (auto index : {grid->model()->index(1, 0), grid->model()->index(6, 6)}) {
        // Dropping selects the day, which may turn the page; read the cell first.
        cal->setCurrentPage(busy.year(), busy.month());
        const int target = index.data().toInt();
        const QPoint at = grid->viewport()->mapTo(cal, grid->visualRect(index).center());
        QMimeData mime;
        mime.setData("application/x-lighttask-id", QByteArray::number(moved));
        QDragEnterEvent enter(at, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(cal, &enter);
        QDropEvent drop(QPointF(at), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(cal, &drop);
        Item stored;
        for (const auto &i : store.all())
            if (i.id == moved)
                stored = i;
        require(drop.isAccepted() && QDate::fromString(stored.date, Qt::ISODate).day() == target,
                "a drop on an expanded cell schedules that day");
    }
    // Both windows reopen the way they were left.
    expandCalendar(floatCal, true);
    settings.sync();
    {
        Controller reopened(store, settings.fileName(), true);
        require(reopened.cal->expanded && reopened.floatCal->expanded &&
                    reopened.dayItems->maximumHeight() == 2 * ROW_HEIGHT &&
                    reopened.floating->height() == 720 && reopened.cal->expandButton->isChecked(),
                "expansion is remembered per window");
    }
    // Larger text: the manager cells still fit item rows.
    setDate(busy);
    theme::textScale = 1.5;
    qApp->setStyleSheet(theme::scaleText(theme::baseStyle));
    scaleManager();
    expandCalendar(cal, true);
    openEditor();
    require(inkBelowNumber(cal, "calendar-manager-expanded-text-150-cal.png") > managerInk + 40,
            "expanded cells name their items at 150% text");
    manager->grab().save(directory + "/calendar-manager-expanded-text-150.png");
    closeEditor();
    theme::textScale = 1.0;
    qApp->setStyleSheet(appStyle);
    scaleManager();
    expandCalendar(cal, false);
    expandCalendar(floatCal, false);
    settle();
}