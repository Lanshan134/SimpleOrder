#include "app/controller.h"

// Calendar navigation (#20). Runs after floatingSmokeTest, whose items and settings it does not rely on.
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
        return QString("%1..%2 (%3) %4").arg(actions.first()->text(), actions.last()->text()).arg(actions.size()).arg(checked);
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
        require(choose(c->yearButton, "2020 年") == "2020 年..2032 年 (13) 2026 年" && page(c) == QDate(2020, 11, 1) &&
                    c->yearButton->text() == "2020 年" && c->monthButton->text() == "11 月",
                "the year menu keeps the month" + name);
        require(choose(c->monthButton, "3 月") == "1 月..12 月 (12) 11 月" && page(c) == QDate(2020, 3, 1) &&
                    c->yearButton->text() == "2020 年" && c->monthButton->text() == "3 月",
                "the month menu keeps the year" + name);
        c->setCurrentPage(3, 5);
        require(choose(c->yearButton, {}).startsWith("1 年..9 年 (9)"), "the year menu starts at year 1" + name);
        c->setCurrentPage(9996, 5);
        require(choose(c->yearButton, {}).startsWith("9990 年..9999 年 (10)"), "the year menu ends at 9999" + name);
        const auto arrow = [c](const QString &label) {
            for (auto button : c->findChildren<QPushButton *>("calendarArrow"))
                if (button->accessibleName() == label)
                    return button;
            return static_cast<QPushButton *>(nullptr);
        };
        c->setCurrentPage(2026, 12);
        arrow("下个月")->click();
        require(page(c) == QDate(2027, 1, 1) && c->yearButton->text() == "2027 年" && c->monthButton->text() == "1 月",
                "next month crosses into the next year" + name);
        arrow("上个月")->click();
        arrow("上个月")->click();
        require(page(c) == QDate(2026, 11, 1) && c->yearButton->text() == "2026 年", "previous month crosses back" + name);
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
    require(paper.alpha() == 255 && paper.lightness() > 230, "the month menu stays opaque on a see-through window");
    settings.setValue("opacity", 100);
    applyFloatingAppearance();
    // Larger text widens the header; the picker and the manager's calendar keep room for it.
    const auto appStyle = qApp->styleSheet();
    theme::textScale = 2.0;
    qApp->setStyleSheet(theme::scaleText(theme::baseStyle));
    scaleManager();
    openEditor();
    managerSplit->setSizes({managerSplit->sizes()[0] + managerSplit->sizes()[1], 0, managerSplit->sizes()[2]});
    settle();
    require(header(cal)->minimumSizeHint().width() <= cal->width(),
            QString("the manager calendar header fits at 200% text (%1 in %2)").arg(header(cal)->minimumSizeHint().width()).arg(cal->width()));
    date->findChild<QAbstractButton *>("fieldCalendar")->click();
    popup = QApplication::activePopupWidget();
    settle();
    auto picker = popup->findChild<Calendar *>();
    auto grid = picker->findChild<QTableView *>();
    int rows = 0;
    for (int row = 0; row < grid->model()->rowCount(); ++row)
        rows += grid->rowHeight(row);
    popup->grab().save(directory + "/date-picker-text-200.png");
    require(header(picker)->minimumSizeHint().width() <= picker->width() && rows <= grid->viewport()->height(),
            "the date picker shows its whole header and grid at 200% text");
    popup->close();
    closeEditor();
    theme::textScale = 1.0;
    qApp->setStyleSheet(appStyle);
    scaleManager();
    settle();
}