#include "app/controller.h"
#include "ui/windows/settings.h"
#include "ui/widgets/smartentry.h"
#include "ui/widgets/widgettools.h"
#include <algorithm>

void smartEntryParserTests();

bool Controller::smokeTest(const QString &dir) {
    auto require = [](bool ok, const char *label) {
        if (!ok)
            throw std::runtime_error(label);
    };
    auto get = [this](qint64 id) {
        for (auto i : store.all())
            if (i.id == id)
                return i;
        return Item{};
    };
    auto contains = [](TaskList *list, qint64 id) {
        for (int n = 0; n < list->count(); ++n)
            if (list->item(n)->data(Qt::UserRole).toLongLong() == id)
                return true;
        return false;
    };
    show();
    QApplication::processEvents();
    // Resizing must change every floating metric using one absolute factor,
    // without affecting the manager or accumulating rounding on repeated drags.
    const int managerFont = all->font().pixelSize();
    const int baseRow = tasks->sizeHintForRow(0);
    const int baseFont = tasks->font().pixelSize();
    floating->resize(630, 720);
    QApplication::processEvents();
    require(qAbs(floatingScale - 1.5) < .01, "uniform floating resize factor");
    require(tasks->sizeHintForRow(0) == 45 && tasks->sizeHint().height() == 225 && tasks->height() >= 225,
            "scaled row and viewport height");
    require(qAbs(tasks->font().pixelSize() - baseFont * 1.5) <= 1, "scaled floating font");
    require(floating->layout()->contentsMargins().left() == 18 && all->font().pixelSize() == managerFont,
            "scaled spacing scoped to float");
    floating->grab().save(dir + "/floating-large.png");
    floating->resize(390, 400);
    QApplication::processEvents();
    require(floatingScale < 1.0 && tasks->sizeHintForRow(0) < baseRow && tasks->font().pixelSize() < baseFont,
            "floating shrink scales down");
    for (int n = 0; n < 3; ++n) {
        floating->resize(560, 640);
        QApplication::processEvents();
        floating->resize(420, 480);
        QApplication::processEvents();
    }
    require(tasks->sizeHintForRow(0) == baseRow && tasks->font().pixelSize() == baseFont &&
                floating->layout()->contentsMargins().left() == 12,
            "no cumulative scale drift");
    // Outside tests a drag's resize steps are coalesced and only the final size is applied.
    testing = false;
    for (int width : {470, 520, 570, 630}) {
        floating->resize(width, width * 720 / 630);
        QApplication::processEvents();
    }
    const bool deferred = qAbs(floatingScale - 1.0) < .01;
    QElapsedTimer waited;
    waited.start();
    while (qAbs(floatingScale - 1.5) >= .01 && waited.elapsed() < 2000)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    testing = true;
    require(deferred && qAbs(floatingScale - 1.5) < .01, "drag resize is debounced to its final size");
    floating->resize(420, 480);
    QApplication::processEvents();
    // Windows text size: tests ignore the machine's setting, so apply an explicit 150%.
    require(theme::scaleText("a{font-size:15px;padding:5px}", 1.5) == "a{font-size:23px;padding:5px}",
            "text scale rewrites font sizes only");
    const auto appStyle = qApp->styleSheet();
    theme::textScale = 1.5;
    qApp->setStyleSheet(theme::scaleText(theme::baseStyle));
    scaleManager();
    floating->resize(floatingSize(420, 480));
    updateLists();
    QApplication::processEvents();
    int itemRow = 0;
    while (itemRow < all->count() && !all->item(itemRow)->data(Qt::UserRole).toLongLong())
        ++itemRow;
    require(all->font().pixelSize() == 23 && all->sizeHintForRow(itemRow) == 45 &&
                floating->size() == QSize(630, 720) && qAbs(floatingScale - 1.5) < .01 &&
                tasks->sizeHintForRow(0) == 45,
            "text size scales manager text and rows, and the default floating window");
    manager->grab().save(dir + "/manager-text-150.png");
    floating->grab().save(dir + "/floating-text-150.png");
    require(manager->width() >= qMin(1770, QGuiApplication::primaryScreen()->availableGeometry().width()) &&
                managerSplit->sizes().first() >= 510,
            "manager window and list pane grow with text size");
    openEditor();
    for (int pass = 0; pass < 3; ++pass)
        QApplication::processEvents();
    const auto pickerAtText150 = date->findChild<QAbstractButton *>("fieldCalendar");
    require(pickerAtText150 && pickerAtText150->width() == 30 && pickerAtText150->height() == 30,
            "field buttons grow with text size");
    manager->grab().save(dir + "/manager-editor-text-150.png");
    closeEditor();
    settings.setValue("scaleWithWindow", false);
    floating->resize(420, 480);
    QApplication::processEvents();
    require(qAbs(floatingScale - 1.5) < .01, "text size applies directly without window scaling");
    settings.setValue("scaleWithWindow", true);
    theme::textScale = 1.0;
    qApp->setStyleSheet(appStyle);
    scaleManager();
    scaleFloating(true);
    updateLists();
    QApplication::processEvents();
    require(all->font().pixelSize() == managerFont && qAbs(floatingScale - 1.0) < .01,
            "text size restored");
    for (bool plan : {false, true})
        for (bool checked : {false, true}) {
            QStandardItemModel model(1, 1);
            auto index = model.index(0, 0);
            model.setData(index, 42, Qt::UserRole);
            model.setData(index, QColor("#6b97c6"), Qt::UserRole + 1);
            model.setData(index, plan, Qt::UserRole + 2);
            model.setData(index, checked, Qt::UserRole + 3);
            QImage image(300, 30, QImage::Format_ARGB32);
            image.fill(Qt::white);
            QStyleOptionViewItem option;
            option.rect = QRect(0, 0, 300, 30);
            option.widget = all;
            option.font = all->font();
            {
                QPainter painter(&image);
                all->itemDelegate()->paint(&painter, option, index);
            }
            int colored = 0, white = 0;
            // The round check sits at the left of the row, centred on (17, 14).
            for (int y = 9; y < 19; ++y)
                for (int x = 12; x < 22; ++x) {
                    auto c = image.pixelColor(x, y);
                    if (c.red() < 245 || c.green() < 245 || c.blue() < 245)
                        ++colored;
                    if (c.red() > 210 && c.green() > 215 && c.blue() > 225)
                        ++white;
                }
            require(checked ? (colored > 0 && white > 0) : colored == 0,
                    "empty unchecked box and checked tick rendering");
            image.save(
                dir +
                QString("/checkbox-%1-%2.png").arg(plan ? "plan" : "task").arg(checked ? "done" : "pending"));
        }
    require(editor->isEnabled() && selected == 0, "blank editor enabled");
    setDate(QDate::currentDate().addDays(4));
    manager->findChild<QPushButton *>("addDateItem")->click();
    require(date->text() == day.toString(Qt::ISODate) && title->text().isEmpty(), "date draft defaults");
    title->setText("日历详细添加验证");
    notes->setPlainText("完整备注");
    time->setText("09:45");
    saveEditor();
    qint64 detailId = selected;
    {
        bool managerVisible = manager->isVisible();
        bool independentEditor = false;
        QTimer::singleShot(0, this, [&] {
            auto dialog = itemWindow.data();
            independentEditor = dialog && dialog->windowTitle() == "编辑事项" && editor->window() == dialog &&
                                manager->isVisible() == managerVisible;
            if (dialog)
                dialog->close();
        });
        tasks->contextEdit(detailId);
        require(independentEditor, "floating context opens editor without showing manager");
        require(editor->window() == manager, "detail editor restored after independent window closes");
    }

    require(get(detailId).notes == "完整备注" && get(detailId).time == "09:45" &&
                get(detailId).date == day.toString(Qt::ISODate),
            "date detail creation");
    auto initial = store.all().size();
    add("列表快速添加验证");
    qint64 id = selected;
    require(store.all().size() == initial + 1 && get(id).date.isEmpty(), "undated quick add");
    schedule(id, QDate::currentDate().addDays(5));
    require(!get(id).date.isEmpty(), "schedule");
    complete(id);
    require(get(id).done && !get(id).completedAt.isEmpty() && contains(tasks, id),
            "completion remains visible");
    require(tasks->item(tasks->count() - 1)->data(Qt::UserRole).toLongLong() == id,
            "completed item sorts last");
    const auto at = QDateTime::fromString(get(id).completedAt, Qt::ISODateWithMs);
    require(visibleInActive(get(id), at.addSecs(48 * 3600 - 1)) &&
                !visibleInActive(get(id), at.addSecs(48 * 3600)),
            "48 hour boundary");
    Item old = get(id);
    old.completedAt = QDateTime::currentDateTimeUtc().addSecs(-49 * 3600).toString(Qt::ISODateWithMs);
    store.save(old);
    refresh();
    require(!contains(tasks, id) && !contains(all, id), "expired hidden");
    showHistory->setChecked(true);
    require(contains(all, id), "history retained");
    showHistory->setChecked(false);
    add("计划对号进度验证");
    type->setCurrentIndex(1);
    target->setValue(2);
    showCalendar->setChecked(true);
    saveEditor();
    id = selected;
    planExpand = true;
    updateLists();
    complete(id);
    require(get(id).progress == 1 && !get(id).done, "plan first tick");
    complete(id);
    require(get(id).progress == 2 && get(id).done && contains(plans, id), "plan reached goal remains");
    complete(id);
    require(!get(id).done && get(id).progress == 0 && get(id).completedAt.isEmpty(), "undo plan completion");
    store.saveCategory({"custom_test", "自定义类型", "#e76f51"});
    reloadCategories();
    choose(id);
    group->setCurrentIndex(group->findData("custom_test"));
    saveEditor();
    require(get(id).group == "custom_test" && groupColor("custom_test") == QColor("#e76f51"),
            "custom category assignment and color");
    store.saveCategory({"custom_test", "阅读", "#345678"});
    reloadCategories();
    refresh();
    require(groupName("custom_test") == "阅读" && groupColor("custom_test") == QColor("#345678"),
            "rename and recolor");
    // Context-menu actions invoke the same persisted path as an actual right click.
    planExpand = true;
    updateLists();
    auto menu = plans->createItemMenu(id);
    require(menu != nullptr, "context menu exists");
    auto action = menu->findChild<QAction *>("togglePin");
    require(action != nullptr, "context pin exists");
    pinExpand = true;
    action->trigger();
    delete menu;
    require(get(id).pinned && contains(pins, id), "pin from menu");
    menu = pins->createItemMenu(id);
    menu->findChild<QAction *>("togglePin")->trigger();
    delete menu;
    require(!get(id).pinned, "unpin from menu");
    store.removeCategory("custom_test");
    reloadCategories();
    refresh();
    require(get(id).group == "other", "delete type reassigns tasks");
    // Enough items to force independent scrolling in both Task and expanded Plan.
    for (int n = 0; n < 14; ++n) {
        Item row;
        row.title = QString("滚动验证任务 %1").arg(n);
        store.save(row);
        row.id = 0;
        row.type = "plan";
        row.title = QString("滚动验证计划 %1").arg(n);
        store.save(row);
    }
    refresh();
    planExpand = false;
    updateLists();
    require(plans->count() == 2 && planMore->isVisible(), "plan collapsed");
    planMore->click();
    QApplication::processEvents();
    require(plans->count() > 6 && plans->verticalScrollBar()->maximum() > 0, "expanded plans scroll");
    require(tasks->verticalScrollBar()->maximum() > 0, "tasks scroll range");
    auto wheelDown = [&](TaskList *list) {
        list->verticalScrollBar()->setValue(0);
        QPointF local(25, 25), global = list->viewport()->mapToGlobal(QPoint(25, 25));
        QWheelEvent wheel(local, global, QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(list->viewport(), &wheel);
        require(list->verticalScrollBar()->value() > 0, "wheel moved list");
    };
    wheelDown(tasks);
    wheelDown(plans);
    int position = tasks->verticalScrollBar()->value();
    updateLists();
    require(tasks->verticalScrollBar()->value() == position, "scroll position retained on refresh");
    settings.setValue("frameless", true);
    settings.setValue("opacity", 75);
    applyFloatingAppearance();
    require(floating->windowFlags().testFlag(Qt::FramelessWindowHint) &&
                qAbs(floating->backgroundOpacity - .75) < .01 && floating->windowOpacity() == 1.0,
            "frameless and opacity");
    settings.setValue("frameless", false);
    applyFloatingAppearance();
    require(!floating->windowFlags().testFlag(Qt::FramelessWindowHint), "framed option");
    settings.setValue("frameless", true);
    settings.setValue("opacity", 100);
    applyFloatingAppearance();
    setTop(true);
    require(floating->windowFlags().testFlag(Qt::WindowStaysOnTopHint), "top flag");
    setTop(false);
    auto manage = floating->findChild<QPushButton *>("openManager");
    require(manage != nullptr, "manager entry exists");
    manager->hide();
    manage->click();
    require(manager->isVisible(), "manager entry works");
    auto table = cal->findChild<QAbstractItemView *>();
    auto idx = table->model()->index(2, 3);
    QPoint pt = table->viewport()->mapTo(cal, table->visualRect(idx).center());
    QMimeData mime;
    mime.setData("application/x-lighttask-id", QByteArray::number(id));
    QDragEnterEvent enter(pt, Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(cal, &enter);
    QDropEvent drop(QPointF(pt), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(cal, &drop);
    require(drop.isAccepted() && QDate::fromString(get(id).date, Qt::ISODate).day() == idx.data().toInt(),
            "drop target date");
    store.saveCategory({"backup_type", "备份类型", "#f08080"});
    reloadCategories();
    QString backup = dir + "/test-backup.json";
    store.backup(backup);
    int count = store.all().size();
    store.removeCategory("backup_type");
    store.importJson(backup);
    reloadCategories();
    refresh();
    require(store.all().size() == count && groupName("backup_type") == "备份类型", "backup includes types");
    QFile bad(dir + "/invalid.json");
    bad.open(QIODevice::WriteOnly);
    bad.write("{\"categories\":[{\"id\":\"other\",\"name\":\"破坏测试\",\"color\":\"#000000\"}],\"items\":[{"
              "\"title\":\"bad\",\"group\":\"other\",\"type\":\"task\",\"date\":\"2026-02-31\"}]}");
    bad.close();
    bool rejected = false;
    try {
        store.importJson(bad.fileName());
    } catch (...) {
        rejected = true;
    }
    require(rejected && store.all().size() == count && store.categories().size() > 1,
            "invalid import rolls back types and tasks");
    {
        Store reopened(dir + "/tasks.sqlite");
        require(reopened.all().size() == count, "persistent reconnect");
    }
    // A v0.1 schema upgrades without touching any real user database.
    const QString legacyFile = dir + "/legacy.sqlite";
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", "legacy-test");
        db.setDatabaseName(legacyFile);
        require(db.open(), "legacy open");
        {
            QSqlQuery q(db);
            require(
                q.exec("CREATE TABLE items(id INTEGER PRIMARY KEY AUTOINCREMENT,title TEXT NOT NULL,grp TEXT "
                       "NOT NULL,type TEXT NOT NULL,date TEXT,time TEXT,time_kind TEXT,notes TEXT,pinned "
                       "INTEGER,done INTEGER,show_calendar INTEGER,progress INTEGER,target INTEGER)"),
                "legacy schema");
            require(
                q.exec(
                    "INSERT INTO items VALUES(42,'legacy','study','task','','','due','old note',1,1,0,0,1)"),
                "legacy row");
        }
        db.close();
    }
    QSqlDatabase::removeDatabase("legacy-test");
    {
        Store migrated(legacyFile);
        auto rows = migrated.all();
        require(rows.size() == 1 && rows[0].id == 42 && rows[0].notes == "old note" &&
                    !rows[0].completedAt.isEmpty(),
                "legacy migration preserves fields");
    }
    require(!QDir(dir).entryList({"legacy.sqlite.before-v02-*.bak"}, QDir::Files).isEmpty(),
            "migration backup exists");
    // Screenshots show compact typography, retained completion, and the new-date editor.
    Item recent;
    recent.title = "已完成 · 保留两天";
    recent.done = true;
    recent.date = QDate::currentDate().toString(Qt::ISODate);
    store.save(recent);
    selected = 0;
    day = QDate::currentDate();
    refresh();
    pinExpand = false;
    planExpand = false;
    updateLists();
    QApplication::processEvents();
    floating->grab().save(dir + "/floating.png");
    inspector->hide();
    QApplication::processEvents();
    manager->grab().save(dir + "/manager.png");
    setDate(QDate::currentDate().addDays(2));
    openEditor();
    title->setText("在选中日期填写详细新事项");
    notes->setPlainText("填写类型、时间、备注后点击添加事项。");
    // Showing the panel reflows its rows; let the queued layout passes finish before the capture.
    for (int pass = 0; pass < 3; ++pass)
        QApplication::processEvents();
    require(inspector->isVisibleTo(manager) && title->isVisibleTo(manager), "editor panel opens for a new item");
    manager->grab().save(dir + "/new-task.png");
    // The field buttons do what the old built-in ones did, and the picker writes the chosen date.
    const auto clearDate = date->findChild<QAbstractButton *>("fieldClear");
    const auto pickDate = date->findChild<QAbstractButton *>("fieldCalendar");
    const auto keptDate = date->text();
    require(clearDate && pickDate && !clearDate->isHidden() && !pickDate->isHidden() && pickDate->width() == 20,
            "date field shows its clear and calendar buttons");
    require((clearDate->focusPolicy() & Qt::TabFocus) && (pickDate->focusPolicy() & Qt::TabFocus),
            "field buttons are reachable with Tab");
    clearDate->click();
    require(date->text().isEmpty() && clearDate->isHidden() && !pickDate->isHidden(),
            "clear button empties the field and steps aside");
    pickDate->click();
    auto popup = QApplication::activePopupWidget();
    require(popup && popup->objectName() == "datePopup", "calendar button opens the date picker");
    for (int pass = 0; pass < 3; ++pass)
        QApplication::processEvents();
    popup->grab().save(dir + "/date-picker.png");
    const QDate pickedDate = QDate::currentDate().addDays(3);
    auto pickerCalendar = popup->findChild<Calendar *>();
    pickerCalendar->setSelectedDate(pickedDate);
    emit pickerCalendar->clicked(pickedDate);
    require(date->text() == pickedDate.toString(Qt::ISODate) && !clearDate->isHidden() &&
                QApplication::activePopupWidget() == nullptr,
            "picking a day fills the field and closes the picker");

    const auto pickedTitle = QStringLiteral("检查选定日期 明天");
    title->setText(pickedTitle);
    require(!parseEditorTitle() && date->text() == pickedDate.toString(Qt::ISODate) &&
                title->text() == pickedTitle && !title->property("entryError").toString().isEmpty(),
            "smart entry preserves a conflicting date chosen with the picker");
    QToolTip::hideText();
    QKeyEvent spaceDown(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier);
    QKeyEvent spaceUp(QEvent::KeyRelease, Qt::Key_Space, Qt::NoModifier);
    QApplication::sendEvent(pickDate, &spaceDown);
    QApplication::sendEvent(pickDate, &spaceUp);
    popup = QApplication::activePopupWidget();
    require(popup && popup->objectName() == "datePopup", "Space opens the date picker");
    pickerCalendar = popup->findChild<Calendar *>();
    const QDate keyboardDate = pickedDate.addDays(1);
    pickerCalendar->setSelectedDate(keyboardDate);
    auto calendarGrid = pickerCalendar->findChild<QAbstractItemView *>();
    require(calendarGrid, "date picker exposes its keyboard grid");
    QKeyEvent confirmDate(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(calendarGrid, &confirmDate);
    require(date->text() == keyboardDate.toString(Qt::ISODate) && !QApplication::activePopupWidget(),
            "Enter chooses the selected date and closes the picker");
    date->setText(keptDate);
    dirty = false;
    fillEditor();
    closeEditor();
    require(!inspector->isVisibleTo(manager) && !selected, "editor panel closes and lets go of its item");
    // Calendar browsing with the panel closed leaves a saved item's form untouched.
    add("面板关闭时日历只浏览");
    const auto browsedId = selected;
    const auto browsedDate = date->text();
    setDate(day.addDays(1));
    require(selected == browsedId && date->text() == browsedDate && !hasEditorChanges(),
            "calendar clicks with the panel closed only browse");
    store.remove(browsedId);
    selected = 0;
    refresh();
    // A closed panel keeps unsaved changes, and reopening the item brings them back.
    Item drafted;
    drafted.title = "关闭面板保留草稿";
    const auto draftedId = store.save(drafted).id;
    refresh();
    choose(draftedId);
    require(inspector->isVisibleTo(manager), "choosing an item opens the panel");
    title->setText("关闭面板保留草稿 未保存");
    closeEditor();
    require(!inspector->isVisibleTo(manager) && selected == draftedId && hasEditorChanges(),
            "closing the panel keeps the draft");
    const auto closedDraft = editorContents();
    setDate(day.addDays(1));
    require(!inspector->isVisibleTo(manager) && selected == draftedId &&
                editorContents() == closedDraft && hasEditorChanges(),
            "calendar browsing with a closed panel preserves the unsaved draft");
    choose(draftedId);
    require(inspector->isVisibleTo(manager) && title->text() == "关闭面板保留草稿 未保存",
            "reopening restores the draft");
    fillEditor();
    closeEditor();
    store.remove(draftedId);
    refresh();
    layoutSmokeTest(dir);
    editorSmokeTest(dir);
    draftSmokeTest(dir);
    calendarEditorSmokeTest();
    smartEntryParserTests();
    smartEntrySmokeTest(dir);
    require(settingsSmokeTest(dir), "classified settings regression");

    return true;
}

void Controller::editorSmokeTest(const QString &dir) {
    auto require = [](bool ok, const char *why) {
        if (!ok)
            throw std::runtime_error(why);
    };
    const QDate monday(2026, 10, 19);
    auto quotaRule = [](const QString &period) {
        return QJsonObject{{"kind", "quota"}, {"period", period}, {"start", ""}, {"end", ""}};
    };
    Item sample;
    sample.type = "plan";
    sample.title = "规则测试";
    sample.target = 2;
    for (auto period : {"day", "week", "month"}) {
        sample.rule = quotaRule(period);
        sample.records = {};
        sample = recordPlan(sample, monday);
        sample = recordPlan(sample, monday);
        require(planState(sample, monday).done, "quota goal completed");
        auto next = QString(period) == "day"    ? monday.addDays(1)
                    : QString(period) == "week" ? monday.addDays(7)
                                                : monday.addMonths(1);
        require(planState(sample, next).progress == 0 && !planState(sample, next).done,
                "new period counter resets");
        require(planState(sample, monday).progress == 2, "previous period count retained");
    }
    sample.target = 1;
    sample.records = {};
    sample.rule = {{"kind", "dates"}, {"mode", "month"}, {"days", QJsonArray{31}}, {"missing", "skip"}};
    require(!planOccurs(sample, QDate(2026, 2, 28)), "31 skipped in February");
    sample.rule.insert("missing", "last");
    require(planOccurs(sample, QDate(2026, 2, 28)), "31 mapped to February end");
    sample.rule.insert("days", QJsonArray{0});
    require(planOccurs(sample, QDate(2028, 2, 29)), "leap month end");
    sample.rule = {{"kind", "dates"}, {"mode", "week"}, {"days", QJsonArray{1, 3, 5}}};
    require(planOccurs(sample, monday) && !planOccurs(sample, monday.addDays(1)), "weekly selected days");
    sample = recordPlan(sample, monday);
    require(planState(sample, monday).done && !planState(sample, monday.addDays(2)).done,
            "occurrences independent");
    sample.records = {};
    sample.rule = {{"kind", "interval"}, {"first", "2026-10-19"}, {"interval", 3}, {"basis", "fixed"}};
    require(planOccurs(sample, monday.addDays(3)) && !planOccurs(sample, monday.addDays(2)),
            "fixed interval dates");
    sample.rule.insert("basis", "after");
    sample = recordPlan(sample, monday.addDays(2));
    require(!planOccurs(sample, monday.addDays(4)) && planOccurs(sample, monday.addDays(5)),
            "completion-relative interval");
    require(planState(sample, monday.addDays(2)).done, "interval completed occurrence retained");
    sample.records = {};
    sample.rule = {{"kind", "total"}};
    sample.target = 3;
    sample = recordPlan(sample, monday);
    require(planState(sample, monday.addMonths(3)).progress == 1, "total never resets by period");
    sample.rule.insert("end", "2026-10-18");
    bool rejected = false;
    try {
        recordPlan(sample, monday);
    } catch (...) {
        rejected = true;
    }
    require(rejected, "completion outside validity rejected");

    // Exercise each actual form, SQLite roundtrip, backup, and independent-window rendering.
    QList<QJsonObject> rules{quotaRule("day"),
                             quotaRule("week"),
                             quotaRule("month"),
                             {{"kind", "dates"},
                              {"mode", "week"},
                              {"days", QJsonArray{1, 3, 5}},
                              {"missing", "skip"},
                              {"start", ""},
                              {"end", ""}},
                             {{"kind", "dates"},
                              {"mode", "month"},
                              {"days", QJsonArray{15, 31, 0}},
                              {"missing", "last"},
                              {"start", ""},
                              {"end", ""}},
                             {{"kind", "interval"},
                              {"first", "2026-10-19"},
                              {"interval", 3},
                              {"basis", "after"},
                              {"start", ""},
                              {"end", ""}},
                             {{"kind", "total"}, {"start", ""}, {"end", ""}}};
    int n = 0;
    for (auto rule : rules) {
        dirty = false;
        selected = 0;
        refresh();
        editorTabs->setCurrentIndex(1);
        title->setText(QString("计划配置验证 %1").arg(n));
        Item draft;
        draft.rule = rule;
        draft.target =
            rule.value("kind").toString() == "quota" || rule.value("kind").toString() == "total" ? 3 : 1;
        planEditor->load(draft);
        require(taskDateBox->isHidden() && !planEditor->isHidden(), "plan shows its own configuration");
        saveEditor();
        const auto id = selected;
        Item saved;
        for (auto i : store.all())
            if (i.id == id)
                saved = i;
        require(saved.rule == rule && saved.target == planEditor->goal(), "all rule fields persisted");
        choose(id);
        require(planEditor->rule() == rule && editorTabs->currentIndex() == 1,
                "plan fields reload correctly");
        // Switching bookmarks keeps the draft until Save or Discard.
        const auto persistedTitle = title->text();
        title->setText(persistedTitle + " 未保存");
        editorTabs->setCurrentIndex(0);
        editorTabs->setCurrentIndex(1);
        require(title->text() == persistedTitle + " 未保存" && planEditor->rule() == rule,
                "bookmark switching retains draft values");
        for (auto stored : store.all())
            if (stored.id == id)
                require(stored.title == persistedTitle, "bookmark switching does not write data");
        fillEditor();
        bool wheelPassed = true, layoutPassed = true;
        QTimer::singleShot(0, this, [&, n] {
            if (auto dialog = itemWindow.data()) {
                QApplication::processEvents();
                dialog->grab().save(dir + QString("/editor-rule-%1.png").arg(n));
                if (n == 4) {
                    auto scroll = qobject_cast<QScrollArea *>(editor->parentWidget()->parentWidget());
                    layoutPassed = editorTabs->parentWidget() == editorHeader && !editorTabs->expanding() &&
                                   group->mapTo(editor, QPoint()).y() ==
                                       planEditor->selector()->mapTo(editor, QPoint()).y();
                    dialog->resize(360, 460);
                    QApplication::processEvents();
                    layoutPassed &= group->mapTo(editor, QPoint()).y() <
                                    planEditor->selector()->mapTo(editor, QPoint()).y();
                    layoutPassed &= group->height() >= group->minimumSizeHint().height() &&
                                    planEditor->selector()->height() >=
                                        planEditor->selector()->minimumSizeHint().height();
                    dialog->grab().save(dir + "/editor-narrow.png");
                    auto wheel = [&](QWidget *receiver, const QPoint &pixels = QPoint()) {
                        const QPoint point = receiver->rect().center();
                        QWheelEvent event(QPointF(point), QPointF(receiver->mapToGlobal(point)), pixels,
                                          pixels.isNull() ? QPoint(0, -120) : QPoint(), Qt::NoButton,
                                          Qt::NoModifier, Qt::ScrollUpdate, false);
                        QCoreApplication::sendEvent(receiver, &event);
                    };
                    for (auto combo : editor->findChildren<QComboBox *>())
                        if (combo->isVisible()) {
                            combo->setFocus();
                            scroll->verticalScrollBar()->setValue(0);
                            const int before = combo->currentIndex();
                            wheel(combo);
                            wheelPassed &=
                                combo->currentIndex() == before && scroll->verticalScrollBar()->value() > 0;
                        }
                    // Popup lists also route wheel input to the outer form.
                    auto combo = planEditor->selector();
                    combo->showPopup();
                    QApplication::processEvents();
                    scroll->verticalScrollBar()->setValue(0);
                    const int before = combo->currentIndex();
                    wheel(combo->view()->viewport());
                    wheelPassed &=
                        combo->currentIndex() == before && scroll->verticalScrollBar()->value() > 0;
                    combo->hidePopup();
                    notes->setPlainText(QString("备注\n").repeated(30));
                    notes->setFocus();
                    scroll->verticalScrollBar()->setValue(0);
                    notes->verticalScrollBar()->setValue(0);
                    wheel(notes->viewport());
                    wheelPassed &=
                        notes->verticalScrollBar()->value() == 0 && scroll->verticalScrollBar()->value() > 0;
                    scroll->verticalScrollBar()->setValue(0);
                    const int headerY = editorHeader->y(), saveY = saveButton->mapTo(dialog, QPoint()).y();
                    wheel(editorTabs, QPoint(0, -35));
                    wheelPassed &=
                        editorTabs->currentIndex() == 1 && scroll->verticalScrollBar()->value() == 35 &&
                        editorHeader->y() == headerY && saveButton->mapTo(dialog, QPoint()).y() == saveY;
                    notes->setPlainText(saved.notes);
                    dirty = false;
                }
                if (n == 0) {
                    auto scroll = qobject_cast<QScrollArea *>(editor->parentWidget()->parentWidget());
                    dialog->resize(360, 460);
                    QApplication::processEvents();
                    auto spin = planEditor->target;
                    spin->setFocus();
                    scroll->verticalScrollBar()->setValue(0);
                    const auto before = spin->value();
                    auto receiver = spin->findChild<QLineEdit *>();
                    QWheelEvent event(QPointF(5, 5), QPointF(receiver->mapToGlobal(QPoint(5, 5))), QPoint(),
                                      QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
                    QCoreApplication::sendEvent(receiver, &event);
                    wheelPassed &= spin->value() == before && scroll->verticalScrollBar()->value() > 0;
                }
                dialog->close();
            }
        });
        editFloatingItem(id);
        require(wheelPassed, "wheel changes only outer form scrolling");
        require(layoutPassed, "compact wide and stacked narrow layouts with fixed bookmarks");
        ++n;
    }
    dirty = false;
    selected = 0;
    refresh();
    require(!taskDateBox->isHidden() && planEditor->isHidden() && done->isHidden() && timeKind->isHidden(),
            "task form has only task fields");
    title->setText("任务编辑布局");
    saveEditor();
    QTimer::singleShot(0, this, [&] {
        if (auto dialog = itemWindow.data()) {
            dialog->grab().save(dir + "/editor-task.png");
            dialog->close();
        }
    });
    editFloatingItem(selected);
    Store isolated(dir + "/rule-roundtrip.sqlite");
    Item roundtrip;
    roundtrip.title = "规则及记录备份";
    roundtrip.type = "plan";
    roundtrip.rule = quotaRule("day");
    roundtrip.target = 3;
    roundtrip = recordPlan(roundtrip, monday);
    isolated.save(roundtrip);
    isolated.backup(dir + "/rule-roundtrip.json");
    isolated.clearItems();
    isolated.importJson(dir + "/rule-roundtrip.json");
    require(isolated.all().first().rule == roundtrip.rule &&
                isolated.all().first().records == roundtrip.records,
            "rules and history backup roundtrip");
    dirty = false;
}

void Controller::draftSmokeTest(const QString &dir) {
    auto require = [](bool ok, const char *why) {
        if (!ok)
            throw std::runtime_error(why);
    };
    dirty = false;
    selected = 0;
    fillEditor();
    openEditor();
    const auto selectedDay = QDate::currentDate().addDays(20);
    setDate(selectedDay);
    editorTabs->setCurrentIndex(1);
    require(planEditor->rule().value("end").toString().isEmpty(), "new plan defaults to unlimited end");
    title->setText("计划草稿保留");
    auto contents = editorContents();
    setDate(selectedDay.addDays(1));
    require(editorContents() == contents && !selected && hasEditorChanges(),
            "calendar browsing retains plan draft without adding bounds");
    refresh();
    require(editorContents() == contents, "refresh preserves unsaved draft");
    settingsDialog();
    require(settingsWindow && QApplication::activeModalWidget() == nullptr && editorContents() == contents,
            "settings opens without save dialog or losing draft");
    settingsWindow->reject();
    require(editorContents() == contents, "closing settings retains draft");
    dirty = false;
    selected = 0;
    fillEditor();
    title->setText("任务日期草稿");
    saveEditor();
    const auto taskId = selected;
    const auto original = editorContents();
    const auto originalDate = date->text();
    QMouseEvent datePress(QEvent::MouseButtonPress, QPointF(5, 5), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
    QApplication::sendEvent(date, &datePress);
    setDate(day.addDays(2));
    require(selected == taskId && title->text() == "任务日期草稿" &&
                date->text() == day.toString(Qt::ISODate),
            "calendar changes current task deadline without changing selection");
    bool databaseUnchanged = false;
    for (auto i : store.all())
        if (i.id == taskId)
            databaseUnchanged = i.date == originalDate;
    require(databaseUnchanged, "calendar edit stays in draft until Save");
    date->setText(originalDate);
    require(!hasEditorChanges() && leaveEditor(), "reverting date needs no prompt");
    title->setText(title->text() + " 临时");
    title->setText(original.value("title").toString());
    require(!hasEditorChanges() && leaveEditor(), "reverting title needs no prompt");
    title->setFocus();
    choose(taskId);
    require(!hasEditorChanges(), "selecting current item leaves form untouched");
    Item other;
    other.title = "草稿切换目标";
    const auto otherId = store.save(other).id;
    title->setText("保留的未保存名称");
    contents = editorContents();
    bool sawPrompt = false;
    QTimer::singleShot(0, this, [&] {
        if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            sawPrompt = true;
            box->button(QMessageBox::Cancel)->click();
        }
    });
    choose(otherId);
    require(sawPrompt && selected == taskId && editorContents() == contents,
            "switching to another item asks and Cancel keeps draft");
    bool nonmodal = false, movableCalendar = false;
    QTimer::singleShot(0, this, [&] {
        auto dialog = itemWindow.data();
        if (!dialog)
            return;
        nonmodal = !dialog->isModal() && QApplication::activeModalWidget() == nullptr;
        manager->show();
        const auto dateBefore = date->text();
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(date, &press);
        setDate(day.addDays(3));
        movableCalendar =
            selected == taskId && date->text() != dateBefore && title->text() == "保留的未保存名称";
        dialog->close();
    });
    editFloatingItem(taskId);
    require(nonmodal && movableCalendar && title->text() == "保留的未保存名称" && hasEditorChanges(),
            "independent editor allows calendar and closes with draft retained");
    fillEditor();
    choose(otherId);
    require(selected == otherId, "unchanged form switches without prompt");
    settings.setValue("frameless", true);
    settings.setValue("opacity", 20);
    settings.setValue("textOpacity", 100);
    applyFloatingAppearance();
    const auto backgroundStyle = floating->styleSheet();
    require(qAbs(floating->backgroundOpacity - .2) < .01 &&
                foregroundColor(tasks, QColor("#293448")).alpha() == 255 &&
                floating->windowOpacity() == 1.0 && foregroundColor(all, QColor("#293448")).alpha() == 255,
            "background transparency keeps floating and manager text opaque");
    const auto transparent = floating->grab().toImage();
    require(qAbs(transparent.pixelColor(5, floating->height() / 2).alpha() - 51) <= 1,
            "floating background pixels actually contain the configured alpha");
    transparent.save(dir + "/floating-background-20-text-100.png");
    settings.setValue("textOpacity", 0);
    applyFloatingAppearance();
    require(qAbs(foregroundColor(tasks, QColor("#293448")).alphaF() - MIN_TEXT_OPACITY / 100.0) < .01,
            "text opacity below the floor is clamped");
    settings.setValue("textOpacity", 45);
    applyFloatingAppearance();
    require(qAbs(floating->backgroundOpacity - .2) < .01 &&
                qAbs(foregroundColor(tasks, QColor("#293448")).alphaF() - .45) < .01 &&
                floating->styleSheet() != backgroundStyle &&
                foregroundColor(all, QColor("#293448")).alpha() == 255,
            "text transparency changes independently of background and manager");
    require(qAbs(floatCal->weekdayTextFormat(Qt::Saturday).foreground().color().alphaF() - .45) < .01,
            "calendar weekend text follows foreground opacity");
    floating->grab().save(dir + "/floating-background-20-text-45.png");
    // Everything in the window fades with the text setting, the quick-add placeholder included.
    floating->show();
    stack->setCurrentIndex(0);
    QApplication::processEvents();
    SmartEntry *quickInput = nullptr;
    for (auto bar : stack->currentWidget()->findChildren<QWidget *>("quickAddBar"))
        quickInput = bar->findChild<SmartEntry *>();
    require(quickInput && quickInput->toPlainText().isEmpty() && quickInput->isVisible(), "placeholder is on screen");
    require(qAbs(quickInput->palette().placeholderText().color().alphaF() - .45) < .01, "placeholder palette fades");
    const auto faded = floating->grab().toImage();
    const QRect inside(quickInput->mapTo(floating, QPoint(10, 6)), quickInput->size() - QSize(20, 12));
    int strongest = 0;
    for (int y = inside.top(); y <= inside.bottom(); ++y)
        for (int x = inside.left(); x <= inside.right(); ++x)
            strongest = qMax(strongest, faded.pixelColor(x, y).alpha());
    // 45% text over a 20% background composites to about 143, or up to about 170 with text smoothing;
    // an unfaded placeholder reads 255.
    require(strongest <= 190, "rendered placeholder follows text opacity");
    calendarView = true;
    stack->setCurrentIndex(1);
    settings.setValue("opacity", 100);
    settings.setValue("textOpacity", 100);
    applyFloatingAppearance();
    const auto monthTitle = [](QDate d) { return QString("%1 年 %2 月").arg(d.year()).arg(d.month()); };
    setDate(QDate::currentDate().addMonths(2));
    require(floatCal->titleButton->text() == monthTitle(QDate::currentDate().addMonths(2)) &&
                cal->titleButton->text() == floatCal->titleButton->text() && floatCal->todayButton->isEnabled(),
            "month title follows the selected date");
    setDate(QDate::currentDate());
    require(floatCal->titleButton->text() == monthTitle(QDate::currentDate()) && !floatCal->todayButton->isEnabled(),
            "today control rests when today is showing");
    for (int pass = 0; pass < 3; ++pass)
        QApplication::processEvents();
    floating->grab().save(dir + "/floating-calendar.png");
    floatCal->findChild<QAbstractItemView *>()->setFocus();
    QApplication::processEvents();
    floating->grab().save(dir + "/floating-calendar-focus.png");
    tasks->setFocus();
    calendarView = false;
    stack->setCurrentIndex(0);
    // An overdue task shows its date in the warning colour; capture it for review.
    Item overdue;
    overdue.title = "已逾期的任务";
    overdue.date = QDate::currentDate().addDays(-3).toString(Qt::ISODate);
    const auto overdueId = store.save(overdue).id;
    dirty = false;
    selected = 0;
    refresh();
    for (int pass = 0; pass < 3; ++pass)
        QApplication::processEvents();
    manager->grab().save(dir + "/manager-overdue.png");
    store.remove(overdueId);
    refresh();
    settings.setValue("opacity", 20);
    settings.setValue("textOpacity", 45);
    applyFloatingAppearance();
    settingsDialog();
    auto background = settingsWindow->findChild<QSlider *>("setting_opacity");
    auto text = settingsWindow->findChild<QSlider *>("setting_textOpacity");
    require(background && text, "two separate opacity sliders exist");
    text->setValue(80);
    require(settings.value("opacity").toInt() == 20 && settings.value("textOpacity").toInt() == 80,
            "text slider persists independently");
    settingsWindow->reject();
    settings.setValue("opacity", 100);
    settings.setValue("textOpacity", 100);
    applyFloatingAppearance();
    dirty = false;
}

void Controller::calendarEditorSmokeTest() {
    auto require = [](bool ok, const char *why) {
        if (!ok)
            throw std::runtime_error(why);
    };
    auto stored = [&](qint64 id) {
        for (auto i : store.all())
            if (i.id == id)
                return i;
        return Item{};
    };
    auto clickDateField = [&] {
        QApplication::processEvents();
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(date, &press);
    };
    auto clickDate = [&](Calendar *calendar, QDate target) {
        QApplication::processEvents();
        {
            QSignalBlocker navigation(calendar);
            calendar->setCurrentPage(target.year(), target.month());
        }
        auto table = calendar->findChild<QAbstractItemView *>();
        const int firstColumn = calendar->verticalHeaderFormat() == QCalendarWidget::NoVerticalHeader ? 0 : 1;
        int firstNumber = table->model()->index(1, firstColumn).data().toInt();
        QDate first(target.year(), target.month(), 1);
        if (firstNumber > 7)
            first = first.addMonths(-1);
        first = QDate(first.year(), first.month(), firstNumber);
        auto offset = first.daysTo(target);
        auto index = table->model()->index(1 + offset / 7, firstColumn + offset % 7);
        require(index.data().toInt() == target.day(), qPrintable(QString("cell mismatch first=%1 target=%2 actual=%3").arg(firstNumber).arg(target.toString(Qt::ISODate)).arg(index.data().toString()) + QString(" shown=%1-%2 selected=%3 firstDay=%4").arg(calendar->yearShown()).arg(calendar->monthShown()).arg(calendar->selectedDate().toString(Qt::ISODate)).arg(calendar->firstDayOfWeek())));
        auto point = table->visualRect(index).center();
        auto global = table->viewport()->mapToGlobal(point);
        QMouseEvent press(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton,
                            Qt::NoModifier);
        QApplication::sendEvent(table->viewport(), &press);
        QApplication::sendEvent(table->viewport(), &release);
        QApplication::processEvents();
    };
    dirty = false;
    selected = 0;
    fillEditor();
    const auto count = store.all().size();
    const QDate a = QDate::currentDate().addDays(4), b = a.addDays(1), c = b.addDays(1);
    clickDate(cal, a);
    require(!selected && title->text().isEmpty() && date->text() == a.toString(Qt::ISODate) &&
                !hasEditorChanges(),
            "date click opens an unlocked creation placeholder");
    notes->setPlainText("尚未命名");
    require(!hasEditorChanges(), "unnamed form does not begin creation");
    clickDate(cal, b);
    require(date->text() == b.toString(Qt::ISODate) && title->text().isEmpty() && !hasEditorChanges() &&
                store.all().size() == count,
            "empty creation follows calendar without saving or prompting");
    title->setText("创建锁定验证");
    clickDate(cal, c);
    require(!selected && date->text() == b.toString(Qt::ISODate) && title->text() == "创建锁定验证",
            "named creation locks fields while calendar is browsed");
    title->clear();
    clickDate(cal, a);
    require(date->text() == a.toString(Qt::ISODate) && !hasEditorChanges(),
            "clearing name unlocks placeholder");
    title->setText("保存后不能误改日期");
    saveEditor();
    const auto id = selected;
    require(id && !hasEditorChanges(), "saving enters clean existing-item view");
    clickDate(cal, b);
    require(!selected && title->text().isEmpty() && date->text() == b.toString(Qt::ISODate) &&
                stored(id).date == a.toString(Qt::ISODate),
            "date after Save starts new form without modifying saved task");
    all->edit(id);
    require(selected == id && title->text() == stored(id).title, "left list selects existing task");
    clickDateField();
    clickDate(cal, c);
    require(selected == id && date->text() == c.toString(Qt::ISODate) &&
                stored(id).date == a.toString(Qt::ISODate),
            qPrintable(QString("date draft selected=%1 expected=%2 date=%3 expectedDate=%4 stored=%5 original=%6")
                .arg(selected).arg(id).arg(date->text()).arg(c.toString(Qt::ISODate))
                .arg(stored(id).date).arg(a.toString(Qt::ISODate)) + QString(" cal=%1 active=%2 pending=%3 mouse=%4").arg(cal->selectedDate().toString(Qt::ISODate)).arg(dateFieldActive).arg(calendarDateEditPending).arg(cal->mouseDateSelection)));
    int prompts = 0;
    auto answer = [&](QMessageBox::StandardButton button) {
        auto timer = new QTimer(this);
        timer->setInterval(0);
        connect(timer, &QTimer::timeout, this, [&, button] {
            if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                ++prompts;
                box->button(button)->click();
            }
        });
        timer->start();
        return timer;
    };
    auto timer = answer(QMessageBox::Cancel);
    clickDate(cal, b);
    timer->stop();
    delete timer;
    require(prompts == 1 && selected == id && date->text() == c.toString(Qt::ISODate) &&
                cal->selectedDate() == c && floatCal->selectedDate() == c,
            "next calendar click asks once; Cancel restores both calendars and preserves draft");
    prompts = 0;
    timer = answer(QMessageBox::Discard);
    clickDate(cal, b);
    timer->stop();
    delete timer;
    require(prompts == 1 && !selected && title->text().isEmpty() && date->text() == b.toString(Qt::ISODate) &&
                stored(id).date == a.toString(Qt::ISODate),
            "Discard opens creation without writing edited task");
    choose(id);
    clickDateField();
    QFocusEvent out(QEvent::FocusOut, Qt::MouseFocusReason);
    QApplication::sendEvent(date, &out);
    QApplication::processEvents();
    clickDate(cal, b);
    require(!selected && title->text().isEmpty() && stored(id).date == a.toString(Qt::ISODate),
            "date-field focus loss cancels calendar edit intent");
    choose(id);
    title->setText("先改名再选日期");
    prompts = 0;
    timer = answer(QMessageBox::Save);
    clickDate(cal, b);
    timer->stop();
    delete timer;
    require(prompts == 1 && !selected && stored(id).title == "先改名再选日期" &&
                stored(id).date == a.toString(Qt::ISODate) && date->text() == b.toString(Qt::ISODate),
            "Save confirms only existing modifications before opening new date form");
    choose(id);
    clickDate(cal, a);
    require(!selected && title->text().isEmpty(),
            "same-day click also opens creation rather than sticking to saved task");
    dayItems->edit(id);
    require(selected == id, "date item list selects existing task");
    clickDateField();
    clickDate(floatCal, b);
    require(selected == id && date->text() == b.toString(Qt::ISODate),
            "floating calendar honors date-field intent too");
    fillEditor();
    clickDate(cal, b);
    require(!selected && !hasEditorChanges(), "clean existing view switches without any confirmation");
}

void Controller::smartEntrySmokeTest(const QString &directory) {
    auto check = [](bool ok, const char *name) {
        if (!ok)
            throw std::runtime_error(name);
    };
    auto get = [&] {
        for (auto i : store.all())
            if (i.id == selected)
                return i;
        return Item{};
    };
    auto enter = [](QWidget *input) {
        QKeyEvent event(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
        QApplication::sendEvent(input, &event);
    };
    dirty = false;
    selected = 0;
    fillEditor();
    auto before = store.all().size();
    title->setText("交报告 明天 18:00");
    enter(title);
    check(title->text() == "交报告" &&
              date->text() == QDate::currentDate().addDays(1).toString(Qt::ISODate) &&
              time->text() == "18:00" && !selected && store.all().size() == before && hasEditorChanges(),
          "name Enter fills draft only");
    saveEditor();
    check(get().title == "交报告" && get().time == "18:00", "parsed form saves explicitly");
    const auto previousDate = date->text(), previousTime = time->text();
    title->setText("改期 后天 19:00");
    enter(title);
    check(!title->property("entryError").toString().isEmpty() && title->text() == "改期 后天 19:00" &&
              date->text() == previousDate && time->text() == previousTime,
          "conflict retains all form contents");
    fillEditor();
    dirty = false;
    selected = 0;
    fillEditor();
    date->setText("2030-01-01");
    dateManuallyEdited = true;
    title->setText("开会 明天");
    enter(title);
    check(date->text() == "2030-01-01" && title->text() == "开会 明天",
          "manually entered date not overwritten");
    date->clear();
    enter(title);
    check(title->text() == "开会" && date->text() == QDate::currentDate().addDays(1).toString(Qt::ISODate),
          "clear field resolves conflict");
    dirty = false;
    selected = 0;
    fillEditor();
    editorTabs->setCurrentIndex(1);
    title->setText("计划 明天 18:00");
    enter(title);
    check(title->text() == "计划 明天 18:00" && planEditor->rule().value("end").toString().isEmpty(),
          "plan name not parsed");
    dirty = false;
    selected = 0;
    fillEditor();
    auto quick = manager->findChild<SmartEntry *>();
    check(quick, "manager quick input exists");
    quick->setPlainText("买菜");
    enter(quick);
    check(quick->toPlainText().isEmpty() && get().title == "买菜" && get().date.isEmpty(),
          "plain quick title keeps undated behavior");
    quick->setPlainText("提醒 18:00");
    enter(quick);
    check(get().date == QDate::currentDate().toString(Qt::ISODate) && get().time == "18:00",
          "time-only quick entry defaults today");
    before = store.all().size();
    quick->setPlainText("明天 18:00");
    enter(quick);
    check(!quick->toPlainText().isEmpty() && store.all().size() == before &&
              !quick->property("entryError").toString().isEmpty(),
          "invalid quick entry does not clear or create");
    quick->setPlainText("会议 今天 明天");
    enter(quick);
    check(store.all().size() == before && !quick->toPlainText().isEmpty(),
          "ambiguous quick entry not created");
    quick->setPlainText("准备 明天 下午三点");
    check(quick->extraSelections().size() == 2 && quick->toolTip().contains("15:00"),
          "recognized spans highlighted with resolved preview");
    quick->grab().save(directory + "/smart-entry-preview.png");
    quick->setPlainText("输入法");
    QInputMethodEvent preedit("ming", {});
    QApplication::sendEvent(quick, &preedit);
    enter(quick);
    check(store.all().size() == before, "IME candidate Enter does not create task");
    QInputMethodEvent committed;
    committed.setCommitString("明");
    QApplication::sendEvent(quick, &committed);
    quick->clear();
    quick->insertPlainText("交报告 明天");
    quick->undo();
    check(quick->toPlainText().isEmpty(), "highlight preserves undo");
    quick->setPlainText("取消时保留 明天");
    title->setText("已有表单未保存修改");
    bool cancelled = false;
    QTimer::singleShot(0, this, [&] {
        if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            cancelled = true;
            box->button(QMessageBox::Cancel)->click();
        }
    });
    enter(quick);
    check(cancelled && quick->toPlainText() == "取消时保留 明天" && store.all().size() == before,
          "cancelled quick creation retains text");
    fillEditor();
    quick->clear();
    const auto chosen = QDate::currentDate().addDays(5);
    selected = 0;
    dirty = false;
    fillEditor();
    setDate(chosen);
    auto floatInputs = floating->findChildren<SmartEntry *>();
    check(floatInputs.size() == 2, "floating list and calendar smart inputs exist");
    SmartEntry *calendarInput = nullptr;
    for (auto input : floatInputs)
        if (input->placeholderText() == "添加到选中日期")
            calendarInput = input;
    check(calendarInput, "calendar quick entry route found");
    calendarInput->setPlainText("选中日期事项 08:30");
    enter(calendarInput);
    check(get().date == chosen.toString(Qt::ISODate) && get().time == "08:30",
          "calendar quick entry uses selected date");
    // Independent dialogs must not turn the name-field Enter into a save click.
    before = store.all().size();
    QTimer::singleShot(0, this, [&] {
        title->setText("已有任务 18:00");
        time->clear();
        enter(title);
        check(store.all().size() == before && hasEditorChanges(), "dialog Enter does not save");
        if (itemWindow)
            itemWindow->close();
    });
    editFloatingItem(selected);
    fillEditor();
}

void Controller::layoutSmokeTest(const QString &directory) {
    QStringList failures;
    auto check = [&](bool ok, const QString &label) {
        if (!ok)
            failures.append(label);
    };
    floating->show();
    for (bool scale : {false, true}) {
        settings.setValue("scaleWithWindow", scale);
        for (int count : {0, 1, 8}) {
            QList<Item> rows;
            for (int n = 0; n < count; ++n) {
                Item item;
                item.id = n + 1;
                item.title = QString("布局验证事项 %1").arg(n + 1);
                item.group = "other";
                rows.append(item);
            }
            floatDay->fill(rows);
            for (QSize size : {QSize(390, 340), QSize(490, 446), QSize(420, 900), QSize(630, 720)}) {
                floating->resize(size);
                scaleFloating(true);
                for (int mode : {0, 1}) {
                    stack->setCurrentIndex(mode);
                    QApplication::processEvents();
                    auto page = stack->currentWidget();
                    auto inputs = page->findChildren<SmartEntry *>();
                    check(inputs.size() == 1, "one quick entry per view");
                    if (inputs.isEmpty())
                        continue;
                    auto footer = inputs.first()->parentWidget();
                    const int bottom = footer->mapTo(page, QPoint(0, footer->height())).y();
                    const QString context = QString("mode=%1 size=%2x%3 rows=%4 scale=%5")
                                                .arg(mode).arg(size.width()).arg(size.height())
                                                .arg(count).arg(scale);
                    check(qAbs(page->height() - bottom) <= 2, "footer anchored: " + context);
                    if (mode == 0) {
                        const int row = scaled(floating, ROW_HEIGHT);
                        check(tasks->viewport()->height() % row == 0,
                              "task viewport contains whole rows: " + context);
                        if (!scale && size == QSize(420, 900))
                            check(tasks->viewport()->height() > tasks->sizeHint().height(),
                                  "task list fills spare height beyond its preferred rows: " + context);
                    }
                    if (mode == 1) {
                        check(floatDay->geometry().top() > floatCal->geometry().bottom(),
                              "calendar/list disjoint: " + context);
                        auto table = floatCal->findChild<QAbstractItemView *>();
                        if (table)
                            check(floatCal->rect().contains(QRect(table->mapTo(floatCal, QPoint()),
                                                                 table->size())),
                                  "calendar last row remains in bounds: " + context);
                        check(floatDay->height() == 4 * scaled(floating, ROW_HEIGHT),
                              "redesigned day list keeps four visible rows: " + context);
                    }
                    if (!scale && count == 1 && size == QSize(420, 900))
                        floating->grab().save(directory + QString("/layout-tall-%1.png").arg(mode));
                    if (scale && count == 8 && size == QSize(390, 340))
                        floating->grab().save(directory + QString("/layout-small-%1.png").arg(mode));
                }
            }
        }
    }
    QFile report(directory + "/layout-checks.txt");
    report.open(QIODevice::WriteOnly);
    report.write(failures.isEmpty() ? QByteArray("PASS: 48 layout cases\n")
                                   : failures.join("\n").toUtf8());
    if (!failures.isEmpty())
        throw std::runtime_error(failures.first().toStdString());
    // Verify the compact calendar overflow can be reached using the scrollbar.
    stack->setCurrentIndex(1);
    floating->resize(390, 340);
    scaleFloating(true);
    QApplication::processEvents();
    auto scroll = floating->findChild<QScrollArea *>("floatingCalendarScroll");
    if (!scroll || scroll->verticalScrollBar()->maximum() <= 0)
        throw std::runtime_error("small calendar overflow must be scrollable");
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    QApplication::processEvents();
    floating->grab().save(directory + "/layout-small-calendar-bottom.png");
    stack->setCurrentIndex(0);
    floating->resize(420, 480);
    refresh();
}
