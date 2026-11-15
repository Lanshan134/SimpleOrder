#include "app/controller.h"
#include "ui/windows/settings.h"
#include "ui/widgets/smartentry.h"
#include "ui/widgets/widgettools.h"
#include <algorithm>

Controller::Controller(Store &s, const QString &path, bool test, bool separateData)
    : store(s), settings(path, QSettings::IniFormat), systemStartupEnabled(!test && !separateData),
      testing(test) {
    reloadCategories();
    buildFloating();
    buildManager();
    setupFloatingScaling();
    if (testing) {
        floating->setAttribute(Qt::WA_DontShowOnScreen);
        manager->setAttribute(Qt::WA_DontShowOnScreen);
    }
    refresh();
    applyFloatingAppearance();
    setTop(settings.value("alwaysOnTop", false).toBool());
    applyPreferences();
    calendarView = settings.value("defaultView", "tasks").toString() == "calendar";
    stack->setCurrentIndex(calendarView ? 1 : 0);
    switchButton->setText(calendarView ? "列表" : "日历");
    floating->findChild<QLabel *>("windowTitle")->setText(calendarView ? "日历" : "任务");
    if (!testing) {
        if (settings.value("rememberGeometry", true).toBool()) {
            floating->restoreGeometry(settings.value("floatingGeometry").toByteArray());
            manager->restoreGeometry(settings.value("managerGeometry").toByteArray());
        }
        for (auto w : {floating, manager}) {
            QRect visible = QGuiApplication::primaryScreen()->availableGeometry();
            bool onScreen = false;
            for (auto screen : QGuiApplication::screens())
                if (screen->availableGeometry().intersects(w->frameGeometry()))
                    onScreen = true;
            if (!onScreen)
                w->move(visible.topLeft() + QPoint(60, 60));
        }
    }
    auto retentionTimer = new QTimer(this);
    retentionTimer->setInterval(60000);
    connect(retentionTimer, &QTimer::timeout, this, [this] {
        guard([&] {
            items = store.all();
            for (auto &i : items)
                i = planState(i, QDate::currentDate());
            updateLists();
            cal->items = items;
            floatCal->items = items;
            cal->repaintCells();
            floatCal->repaintCells();
            maybeAutoBackup();
        });
    });
    if (!testing)
        retentionTimer->start();
    floating->onClose = [this] { closeWindow(true); };
    manager->onClose = [this] { closeWindow(false); };
    if (!testing && QSystemTrayIcon::isSystemTrayAvailable()) {
        QPixmap pix(32, 32);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(theme::color("accent"));
        p.setPen(Qt::NoPen);
        p.drawEllipse(1, 1, 30, 30);
        p.setPen(QPen(Qt::white, 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF tick[] = {{9, 16.5}, {14, 21.5}, {23, 11}};
        p.drawPolyline(tick, 3);
        p.end();
        tray = new QSystemTrayIcon(QIcon(pix), this);
        tray->setToolTip("简序");
        auto menu = new QMenu;
        menu->addAction("打开悬浮窗", [this] {
            floating->show();
            floating->raise();
        });
        menu->addAction("打开管理窗口", [this] {
            manager->show();
            manager->raise();
        });
        menu->addSeparator();
        menu->addAction("退出", [this] {
            if (leaveEditor()) {
                persistGeometry();
                qApp->quit();
            }
        });
        tray->setContextMenu(menu);
        connect(tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
            if (r == QSystemTrayIcon::DoubleClick) {
                floating->show();
                floating->raise();
            }
        });
        tray->show();
    }
}
Controller::~Controller() {
    if (settingsWindow) {
        disconnect(settingsWindow.data(), nullptr, this, nullptr);
        delete settingsWindow.data();
    }
    persistGeometry();
    if (tray) {
        delete tray->contextMenu();
        tray->hide();
    }
    delete floating;
    delete manager;
}
void Controller::persistGeometry() {
    if (settings.value("rememberGeometry", true).toBool()) {
        settings.setValue("floatingGeometry", floating->saveGeometry());
        settings.setValue("managerGeometry", manager->saveGeometry());
    }
    settings.sync();
}
void Controller::guard(const std::function<void()> &fn) {
    try {
        fn();
    } catch (const std::exception &e) {
        if (testing)
            throw;
        QMessageBox::critical(editor ? editor->window() : manager, "操作未完成", QString::fromUtf8(e.what()));
    }
}
void Controller::bindList(TaskList *list) {
    list->edit = [this](qint64 id) { choose(id); };
    list->contextEdit = [this, list](qint64 id) {
        if (list == pins || list == tasks || list == plans || list == floatDay)
            editFloatingItem(id);
        else
            choose(id);
    };
    list->complete = [this, list](qint64 id) {
        guard([&] { complete(id, list == dayItems || list == floatDay ? day : QDate::currentDate()); });
    };
    list->togglePin = [this](qint64 id) {
        guard([&] {
            if (settingsPreview)
                return;
            if (!leaveEditor())
                return;
            for (auto i : store.all())
                if (i.id == id) {
                    i.pinned = !i.pinned;
                    store.save(i);
                    refresh();
                    break;
                }
        });
    };
}
void Controller::updateLists() {
    QHash<TaskList *, int> positions;
    for (auto l : {pins, tasks, plans, all, dayItems, floatDay})
        positions[l] = l->verticalScrollBar()->value();
    QList<Item> p, t, pl, visible, dayRows, floatRows;
    for (const auto &i : items) {
        bool active = visibleItem(i);
        if (active) {
            if (i.pinned && !i.done)
                p << i;
            else if (i.type == "task")
                t << i;
            else
                pl << i;
        }
        auto dated = planState(i, day);
        if (visibleItem(dated) && planOccurs(i, day) && (i.type == "task" || i.showCalendar))
            floatRows << dated;
        if ((visibleItem(dated) || showHistory->isChecked()) && planOccurs(i, day) &&
            (i.type == "task" || i.showCalendar))
            dayRows << dated;
    }
    pins->fill(pinExpand ? sortedItems(p) : sortedItems(p).mid(0, settings.value("pinRows", 2).toInt()));
    plans->fill(planExpand ? sortedItems(pl) : sortedItems(pl).mid(0, settings.value("planRows", 2).toInt()));
    tasks->fill(sortedItems(t));
    pins->setFixedHeight(qMax(1, qMin(pinExpand ? 6 : settings.value("pinRows", 2).toInt(), p.size())) *
                         scaled(pins, ROW_HEIGHT));
    plans->setFixedHeight(qMax(1, qMin(planExpand ? 6 : settings.value("planRows", 2).toInt(), pl.size())) *
                          scaled(plans, ROW_HEIGHT));
    pinMore->setVisible(p.size() > settings.value("pinRows", 2).toInt());
    planMore->setVisible(pl.size() > settings.value("planRows", 2).toInt());
    const auto openCount = [](const QList<Item> &list) {
        return std::count_if(list.begin(), list.end(), [](const Item &i) { return !i.done; });
    };
    pinHeading->setText(QString("置顶  %1").arg(p.size()));
    taskHeading->setText(QString("任务  %1").arg(openCount(t)));
    planHeading->setText(QString("计划  %1").arg(openCount(pl)));
    // Empty groups step aside so the list starts with what there is to do.
    pinHeading->setVisible(!p.isEmpty());
    pins->setVisible(!p.isEmpty());
    planHeading->setVisible(!pl.isEmpty());
    plans->setVisible(!pl.isEmpty());
    floatDate->setText(QLocale().toString(QDate::currentDate(), "M月d日 ddd"));
    all->setProperty("emptyText", search->text().trimmed().isEmpty() ? "还没有事项" : "没有匹配的事项");
    pinMore->setText(pinExpand ? "收起"
                               : "展开 " + QString::number(p.size() - settings.value("pinRows", 2).toInt()));
    planMore->setText(planExpand ? "收起"
                                 : "展开 " + QString::number(pl.size() - settings.value("planRows", 2).toInt()));
    for (int section = 0; section < 4; ++section) {
        QList<Item> part;
        for (auto i : items)
            if ((i.done             ? 3
                 : i.pinned         ? 0
                 : i.type == "task" ? 1
                                    : 2) == section &&
                (visibleItem(i) || showHistory->isChecked()) &&
                i.title.contains(search->text().trimmed(), Qt::CaseInsensitive))
                part << i;
        visible << sortedItems(part);
    }
    all->fill(visible, true);
    dayItems->fill(sortedItems(dayRows));
    floatDay->fill(sortedItems(floatRows));
    dateLabel->setText(QLocale().toString(day, day == QDate::currentDate() ? "今天 · M月d日 ddd" : "M月d日 ddd") +
                       QString(" · %1 项").arg(dayRows.size()));
    for (auto list : {all, pins, tasks, plans, dayItems, floatDay}) {
        for (int n = 0; n < list->count(); ++n)
            if (list->item(n)->data(Qt::UserRole).toLongLong() == selected)
                list->setCurrentRow(n);
        list->verticalScrollBar()->setValue(positions.value(list));
    }
}
void Controller::refresh() {
    items = store.all();
    for (auto &i : items)
        i = planState(i, QDate::currentDate());
    {
        QSignalBlocker b(cal), b2(floatCal);
        cal->setSelectedDate(day);
        floatCal->setSelectedDate(day);
    }
    cal->items = items;
    floatCal->items = items;
    cal->repaintCells();
    floatCal->repaintCells();
    updateLists();
    if (!dirty || !hasEditorChanges())
        fillEditor();
    status->setText(settingsPreview      ? "设置预览中 · 可移动窗口、展开和浏览，事项暂不可编辑"
                    : hasEditorChanges() ? "有未保存的草稿"
                                         : "已保存在本机");
}
void Controller::setDate(QDate d) {
    if (refreshing || !d.isValid())
        return;
    const bool editTaskDate =
        (dateFieldActive || calendarDateEditPending) && type->currentData().toString() == "task";
    dateFieldActive = false;
    calendarDateEditPending = false;
    auto selectDay = [&](QDate value) {
        QSignalBlocker a(cal), b(floatCal);
        cal->setSelectedDate(value);
        floatCal->setSelectedDate(value);
        // Signal blockers also silence the redesigned month-title updates.
        cal->repaintCells();
        floatCal->repaintCells();
    };
    if (settingsPreview || (!itemWindow && !inspector->isVisibleTo(manager))) {
        // A closed editor keeps its saved item or draft while the calendar browses.
        day = d;
        selectDay(d);
        updateLists();
        return;
    }
    if (editTaskDate) {
        day = d;
        selectDay(d);
        date->setText(d.toString(Qt::ISODate));
        dateManuallyEdited = true;
        date->clearFocus();
        updateLists();
        return;
    }
    if (!selected && !title->text().trimmed().isEmpty()) {
        // A named, unsaved creation is locked; browsing the calendar keeps its fields intact.
        day = d;
        selectDay(d);
        updateLists();
        return;
    }
    if (selected && !leaveEditor()) {
        selectDay(day);
        return;
    }
    // Saving ends creation lock. A subsequent calendar click starts a fresh placeholder.
    day = d;
    selected = 0;
    dirty = false;
    selectDay(d);
    fillEditor();
    updateLists();
}
void Controller::complete(qint64 id, QDate occurrence) {
    if (settingsPreview)
        return;
    if (!leaveEditor())
        return;
    for (auto i : items)
        if (i.id == id) {
            if (i.type == "plan" && !i.rule.isEmpty()) {
                i = recordPlan(i, occurrence.isValid() ? occurrence : QDate::currentDate());
            } else if (i.type == "plan") {
                if (i.done) {
                    i.done = false;
                    i.progress = 0;
                } else {
                    i.progress = qMin(i.target, i.progress + 1);
                    i.done = i.progress >= i.target;
                }
            } else
                i.done = !i.done;
            store.save(i);
            refresh();
            return;
        }
}
void Controller::schedule(qint64 id, QDate d) {
    if (settingsPreview)
        return;
    if (!leaveEditor())
        return;
    for (auto i : items)
        if (i.id == id) {
            i.date = d.toString(Qt::ISODate);
            if (i.type == "plan" && !i.rule.isEmpty()) {
                auto k = i.rule.value("kind").toString();
                if (k == "total")
                    i.rule.insert("end", i.date);
                else if (k == "interval")
                    i.rule.insert("first", i.date);
                else if (k == "quota")
                    i.rule.insert("start", i.date);
                else {
                    auto days = i.rule.value("days").toArray();
                    int value = i.rule.value("mode").toString() == "week" ? d.dayOfWeek() : d.day();
                    if (!days.contains(value))
                        days.append(value);
                    i.rule.insert("days", days);
                }
                i = planState(i, QDate::currentDate());
            }
            store.save(i);
            day = d;
            selected = id;
            refresh();
            return;
        }
}
bool Controller::add(const QString &t, QDate d) {
    if (settingsPreview)
        return false;
    const auto parsed = parseEntry(t);
    if (!parsed.error.isEmpty())
        throw std::runtime_error(parsed.error.toUtf8().constData());
    if (!leaveEditor())
        return false;
    if (parsed.date.isValid())
        d = parsed.date;
    else if (parsed.time.isValid() && !d.isValid())
        d = QDate::currentDate();
    Item i;
    i.title = parsed.title;
    i.group = settings.value("defaultType", "other").toString();
    i.date = d.isValid() ? d.toString(Qt::ISODate) : QString();
    i.time = parsed.time.isValid() ? parsed.time.toString("HH:mm") : QString();
    selected = store.save(i).id;
    dirty = false;
    if (d.isValid())
        day = d;
    refresh();
    return true;
}
void Controller::setTop(bool top) {
    bool shown = floating->isVisible();
    floating->setWindowFlag(Qt::WindowStaysOnTopHint, top);
    if (shown)
        floating->show();

    settings.setValue("alwaysOnTop", top);
    settings.sync();
}
void Controller::show() {
    const auto view = settings.value("startupView", "both").toString();
    if (view != "manager")
        floating->show();
    if (view != "floating")
        manager->show();
    if (!testing)
        guard([this] { maybeAutoBackup(); });
}
void Controller::settingsDialog() {
    if (settingsWindow) {
        settingsWindow->show();
        settingsWindow->raise();
        settingsWindow->activateWindow();
        return;
    }
    auto dialog = new SettingsDialog(*this, manager);
    settingsWindow = dialog;
    dialog->setWindowModality(Qt::NonModal);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    setSettingsPreview(true);
    connect(dialog, &QDialog::finished, this, [this, dialog] {
        if (settingsWindow == dialog) {
            settingsWindow.clear();
            setSettingsPreview(false);
        }
    });
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
void Controller::setSettingsPreview(bool enabled) {
    settingsPreview = enabled;
    for (auto list : {pins, tasks, plans, all, dayItems, floatDay})
        list->setPreviewOnly(enabled);
    for (auto calendar : {cal, floatCal}) {
        calendar->previewOnly = enabled;
        calendar->setAcceptDrops(!enabled);
    }
    for (auto root : {floating, manager})
        for (auto bar : root->findChildren<QWidget *>("quickAddBar"))
            bar->setEnabled(!enabled);
    editor->setEnabled(!enabled);
    editorHeader->setEnabled(!enabled);
    saveButton->setEnabled(!enabled);
    deleteButton->setEnabled(!enabled);
    manager->findChild<QPushButton *>("addDateItem")->setEnabled(!enabled);
    status->setText(enabled ? "设置预览（只读）"
                           : hasEditorChanges() ? "有未保存的草稿" : "已保存在本机");
}
void Controller::reloadCategories() {
    const auto categories = store.categories();
    updateCategoryColors(categories);
    if (group) {
        QSignalBlocker b(group);
        auto id = group->currentData().toString();
        group->clear();
        for (const auto &c : categories)
            group->addItem(typeDot(c.color), c.name, c.id);
        group->setCurrentIndex(qMax(0, group->findData(id)));
    }
}
