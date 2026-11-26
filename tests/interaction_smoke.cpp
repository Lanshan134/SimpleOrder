#include "app/controller.h"
#include "ui/widgets/smartentry.h"
#include "ui/widgets/widgettools.h"
#include "ui/windows/settings.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
// Counts drag starts without running a real drag loop.
class DragProbe : public TaskList {
  public:
    int drags = 0;

  protected:
    void startDrag(Qt::DropActions) override {
        ++drags;
    }
};
} // namespace

// Floating list, calendar and title gestures each trigger exactly the action they are meant to (#18).
void Controller::interactionSmokeTest(const QString &dir) {
    auto require = [](bool ok, const char *why) {
        if (!ok)
            throw std::runtime_error(why);
    };
    const QDate today = QDate::currentDate();
    store.clearItems();
    settings.setValue("sortMode", "date");
    settings.setValue("showCompleted", true);
    settings.setValue("retentionHours", 48);
    dirty = false;
    selected = 0;
    day = today;
    fillEditor();
    closeEditor();
    calendarView = false;
    stack->setCurrentIndex(0);
    floating->show();
    floating->resize(420, 480);
    scaleFloating(true);
    auto make = [&](const QString &title, QDate date = {}) {
        Item i;
        i.title = title;
        i.date = date.isValid() ? date.toString(Qt::ISODate) : QString();
        return store.save(i).id;
    };
    // Dated items sort first, so completing the first row moves the next one under the pointer.
    const qint64 a = make("交互甲", today), c = make("交互丙", today), b = make("交互乙");
    const qint64 later = make("交互丁", today.addDays(6));
    auto stored = [&](qint64 id) {
        for (auto i : store.all())
            if (i.id == id)
                return i;
        return Item{};
    };
    auto reset = [&] {
        dirty = false;
        selected = 0;
        day = today;
        fillEditor();
        closeEditor();
        manager->hide();
        for (auto id : {a, b, c}) {
            auto i = stored(id);
            i.done = false;
            store.save(i);
        }
        refresh();
        tasks->setCurrentRow(-1);
        QApplication::processEvents();
    };
    QStringList calls;
    const auto edit = tasks->edit, complete = tasks->complete;
    bool independentEditor = false;
    tasks->edit = [&, edit](qint64 id) {
        calls << "edit";
        // The floating list opens the independent editor; note what opened, then close it.
        QTimer::singleShot(0, this, [&] {
            independentEditor = itemWindow && !manager->isVisible();
            if (itemWindow)
                itemWindow->close();
        });
        edit(id);
    };
    tasks->complete = [&, complete](qint64 id) {
        calls << "complete";
        complete(id);
    };
    auto send = [](QWidget *w, QEvent::Type type, QPoint p, Qt::MouseButton button, Qt::MouseButtons held) {
        QMouseEvent event(type, p, w->mapToGlobal(p), button, held, Qt::NoModifier);
        QApplication::sendEvent(w, &event);
    };
    auto at = [&](TaskList *list, qint64 id, int x) {
        for (int n = 0; n < list->count(); ++n)
            if (list->item(n)->data(Qt::UserRole).toLongLong() == id)
                return QPoint(x, list->visualItemRect(list->item(n)).center().y());
        throw std::runtime_error("row missing");
    };
    auto view = tasks->viewport();
    auto click = [&](QPoint p) {
        send(view, QEvent::MouseButtonPress, p, Qt::LeftButton, Qt::LeftButton);
        send(view, QEvent::MouseButtonRelease, p, Qt::LeftButton, Qt::NoButton);
    };
    const int check = 17, body = 150;

    // D1: a double click on the circle completes one item, not the row that slides under it.
    reset();
    calls.clear();
    click(at(tasks, a, check));
    send(view, QEvent::MouseButtonDblClick, at(tasks, a, check), Qt::LeftButton, Qt::LeftButton);
    send(view, QEvent::MouseButtonRelease, at(tasks, a, check), Qt::LeftButton, Qt::NoButton);
    require(stored(a).done && !stored(c).done && calls == QStringList{"complete"},
            "double click on a circle completes only that item");
    // D2: press and release must land in the same zone.
    reset();
    calls.clear();
    for (auto [from, to] : {QPair<int, int>{33, 27}, {27, 33}}) {
        send(view, QEvent::MouseButtonPress, at(tasks, a, from), Qt::LeftButton, Qt::LeftButton);
        send(view, QEvent::MouseMove, at(tasks, a, to), Qt::NoButton, Qt::LeftButton);
        send(view, QEvent::MouseButtonRelease, at(tasks, a, to), Qt::LeftButton, Qt::NoButton);
    }
    require(calls.isEmpty() && !stored(a).done, "a click that crosses the circle edge does nothing");
    // D3: the event's own position decides, wherever the pointer is now.
    reset();
    calls.clear();
    const QPoint home = floating->pos();
    floating->move(30000, 30000);
    click(at(tasks, a, body));
    require(calls == QStringList{"edit"} && !stored(a).done, "a body click edits even if the pointer has moved");
    require(independentEditor, "a floating body click opens the independent editor, not the manager");
    floating->move(home);
    // D12: in settings preview the circle does nothing.
    reset();
    calls.clear();
    settingsDialog();
    click(at(tasks, a, check));
    settingsWindow->reject();
    QApplication::processEvents();
    require(calls.isEmpty() && !stored(a).done, "preview circle neither completes nor opens the item");
    // D4: only the left button drags an item.
    {
        DragProbe probe;
        probe.setAttribute(Qt::WA_DontShowOnScreen);
        probe.resize(300, 120);
        probe.fill({stored(a), stored(b)});
        probe.show();
        QApplication::processEvents();
        for (auto button : {Qt::RightButton, Qt::MiddleButton, Qt::LeftButton}) {
            const QPoint p = at(&probe, a, body);
            send(probe.viewport(), QEvent::MouseButtonPress, p, button, button);
            send(probe.viewport(), QEvent::MouseMove, p + QPoint(8, 0), Qt::NoButton, button);
            send(probe.viewport(), QEvent::MouseMove, p + QPoint(25, 0), Qt::NoButton, button);
            send(probe.viewport(), QEvent::MouseButtonRelease, p + QPoint(25, 0), button, Qt::NoButton);
        }
        require(probe.drags == 1, "right and middle drags do not reschedule");
    }
    // D6: keyboard position survives completion and refresh; held Space toggles once; headings are skipped.
    reset();
    tasks->setFocus();
    tasks->setCurrentRow(tasks->row(tasks->itemAt(at(tasks, a, body))));
    for (bool repeat : {false, true}) {
        QKeyEvent space(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier, " ", repeat);
        QApplication::sendEvent(tasks, &space);
    }
    updateLists();
    require(stored(a).done && tasks->currentItem() && tasks->currentItem()->data(Qt::UserRole).toLongLong() == a,
            "Space completes once and the keyboard stays on the item");
    reset();
    all->setCurrentRow(all->row(all->itemAt(at(all, later, body))));
    store.remove(later);
    refresh();
    require(all->currentItem() && all->currentItem()->data(Qt::UserRole).toLongLong(),
            "a removed row hands the keyboard to a neighbouring item, never a heading");
    const qint64 dated = make("交互戊", today.addDays(6));
    // D5: opening an item keeps the selected day.
    reset();
    choose(dated);
    require(day == today, "opening an item keeps the selected day");
    // D8: a panel in a hidden manager does not follow the floating calendar.
    reset();
    choose(a);
    title->setText("交互甲 草稿");
    manager->hide();
    int prompts = 0;
    QTimer watch;
    watch.setInterval(0);
    QMessageBox::StandardButton answer = QMessageBox::Cancel;
    bool saveOffered = true;
    connect(&watch, &QTimer::timeout, this, [&] {
        if (auto box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            ++prompts;
            saveOffered = box->button(QMessageBox::Save) != nullptr;
            box->button(answer)->click();
        }
    });
    watch.start();
    setDate(today.addDays(3));
    require(prompts == 0 && selected == a && title->text() == "交互甲 草稿",
            "hidden manager panel keeps its item while the floating calendar browses");
    // D10: after Cancel the list highlight returns to the item being edited.
    click(at(tasks, b, body));
    require(prompts == 1 && selected == a && tasks->currentItem() &&
                tasks->currentItem()->data(Qt::UserRole).toLongLong() == a,
            "Cancel returns the highlight to the edited item");
    // D13: settings preview offers no Save that would do nothing.
    settingsDialog();
    click(at(tasks, b, body));
    settingsWindow->reject();
    QApplication::processEvents();
    require(prompts == 2 && !saveOffered && selected == a, "preview prompt offers only discard or cancel");
    watch.stop();
    // D7: the independent editor keeps its item while the calendar, drops and quick add are used.
    reset();
    bool kept = false;
    QTimer::singleShot(0, this, [&] {
        if (!itemWindow)
            return;
        setDate(today.addDays(2));
        schedule(b, today.addDays(4));
        add("交互快速添加");
        kept = selected == a && title->text() == "交互甲" && stored(b).date == today.addDays(4).toString(Qt::ISODate);
        itemWindow->close();
    });
    editFloatingItem(a);
    require(kept, "independent editor keeps its item");
    // D9: a press right after a click still drags the window.
    reset();
    auto handle = floating->findChild<QLabel *>("windowTitle");
    const QPoint start = handle->rect().center(), before = floating->pos();
    send(handle, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    send(handle, QEvent::MouseButtonRelease, start, Qt::LeftButton, Qt::NoButton);
    send(handle, QEvent::MouseButtonDblClick, start, Qt::LeftButton, Qt::LeftButton);
    send(handle, QEvent::MouseMove, start + QPoint(12, 9), Qt::NoButton, Qt::LeftButton);
    send(handle, QEvent::MouseButtonRelease, start + QPoint(12, 9), Qt::LeftButton, Qt::NoButton);
    require(floating->pos() == before + QPoint(12, 9), "drag right after a click moves the window");
    floating->move(before);
    // The add button's corner is not covered by the size grip.
    auto grip = floating->findChild<QSizeGrip *>();
    auto quickAdd = stack->currentWidget()->findChild<QPushButton *>("quickAdd");
    const QRect overlap = QRect(grip->mapTo(floating, QPoint()), grip->size()) &
                          QRect(quickAdd->mapTo(floating, QPoint()), quickAdd->size());
    require(overlap.isEmpty() || floating->childAt(overlap.center()) == quickAdd, "add button corner is clickable");
    // Rule: actions on another item leave the draft alone; the draft's own circle completes it and keeps the draft.
    reset();
    choose(a);
    title->setText("交互甲 草稿");
    manager->hide();
    prompts = 0;
    watch.start();
    click(at(tasks, b, check));
    schedule(c, today.addDays(5));
    tasks->togglePin(b);
    click(at(tasks, a, check));
    require(prompts == 0 && stored(b).done && stored(b).pinned &&
                stored(c).date == today.addDays(5).toString(Qt::ISODate) && stored(a).done && selected == a &&
                title->text() == "交互甲 草稿",
            "completing, pinning and rescheduling do not ask about the draft");
    saveEditor();
    require(stored(a).done && stored(a).title == "交互甲 草稿", "saving the draft keeps its own completion");
    watch.stop();
    for (auto id : {a, b, c}) {
        auto i = stored(id);
        i.pinned = false;
        i.title = id == a ? "交互甲" : i.title;
        store.save(i);
    }
    // Rule: count and cumulative plans have no date to drag to; interval and chosen-day plans do.
    {
        QList<Item> plans;
        for (auto kind : {"quota", "total", "interval", "dates"}) {
            Item plan;
            plan.type = "plan";
            plan.title = QString("交互计划 ") + kind;
            plan.rule = {{"kind", kind}, {"period", "day"}, {"first", today.toString(Qt::ISODate)}, {"interval", 2},
                         {"basis", "fixed"}, {"mode", "week"}, {"days", QJsonArray{1}}, {"missing", "skip"},
                         {"start", ""}, {"end", ""}};
            plan.id = store.save(plan).id;
            plans << plan;
        }
        DragProbe probe;
        probe.setAttribute(Qt::WA_DontShowOnScreen);
        probe.resize(300, 200);
        probe.fill(plans);
        probe.show();
        QApplication::processEvents();
        for (const auto &plan : plans) {
            const QPoint p = at(&probe, plan.id, body);
            send(probe.viewport(), QEvent::MouseButtonPress, p, Qt::LeftButton, Qt::LeftButton);
            send(probe.viewport(), QEvent::MouseMove, p + QPoint(8, 0), Qt::NoButton, Qt::LeftButton);
            send(probe.viewport(), QEvent::MouseMove, p + QPoint(25, 0), Qt::NoButton, Qt::LeftButton);
            send(probe.viewport(), QEvent::MouseButtonRelease, p + QPoint(25, 0), Qt::LeftButton, Qt::NoButton);
        }
        const auto before = stored(plans[0].id);
        refresh();
        schedule(plans[0].id, today.addDays(3));
        require(probe.drags == 2, qPrintable(QString("only interval and chosen-day plans drag (%1)").arg(probe.drags)));
        require(stored(plans[0].id).rule == before.rule && stored(plans[0].id).date == before.date,
                "a count plan does not reschedule");
        for (const auto &plan : plans)
            store.remove(plan.id);
    }
    tasks->edit = edit;
    tasks->complete = complete;
    reset();
    // #2: menus opened from the floating window stay opaque and legible whatever the window's opacity.
    settings.setValue("opacity", 0);
    settings.setValue("textOpacity", 45);
    applyFloatingAppearance();
    auto spare = new QMenu(floatCal);
    spare->addAction("月份");
    for (QMenu *menu : {tasks->createItemMenu(a), spare,
                        stack->currentWidget()->findChild<SmartEntry *>()->createStandardContextMenu()}) {
        menu->setAttribute(Qt::WA_DontShowOnScreen);
        menu->popup(QPoint(100, 100));
        QApplication::processEvents();
        const QImage image = menu->grab().toImage();
        const QColor inside = image.pixelColor(6, image.height() / 2);
        require(inside.alpha() == 255 && inside.lightness() > 240 &&
                    menu->palette().color(QPalette::WindowText).alpha() == 255,
                "menus from the floating window stay opaque");
        delete menu;
    }
    settings.setValue("opacity", 100);
    settings.setValue("textOpacity", 100);
    applyFloatingAppearance();

    // #2: the title bar is three icon buttons that name themselves.
    auto config = floating->findChild<QPushButton *>("floatingSettings");
    auto openManager = floating->findChild<QPushButton *>("openManager");
    require(config && openManager && switchButton->text().isEmpty() && switchButton->size() == QSize(28, 28) &&
                switchButton->accessibleName() == "切换到日历" && switchButton->toolTip() == "切换到日历" &&
                openManager->accessibleName() == "打开管理窗口" && config->accessibleName() == "设置",
            "icon title bar with names and tooltips");
    switchButton->click();
    require(calendarView && switchButton->accessibleName() == "返回任务列表", "view switch renames its icon");
    switchButton->click();
    // Text colour and halo round-trip through settings; the halo applies only below the threshold.
    settingsDialog();
    auto tone = settingsWindow->findChild<QComboBox *>("setting_floatingText");
    auto halo = settingsWindow->findChild<QCheckBox *>("setting_floatingHalo");
    auto opacity = settingsWindow->findChild<QSlider *>("setting_opacity");
    require(tone && halo && halo->isChecked() && tone->currentData() == "dark", "legibility settings default");
    auto haloOn = [&] {
        return floating->findChild<QLabel *>("windowTitle")->parentWidget()->graphicsEffect() && stack->graphicsEffect();
    };
    opacity->setValue(qRound(HALO_BELOW * 100));
    require(!haloOn() && foregroundColor(tasks, theme::color("ink-muted")) == theme::color("ink-muted"),
            "no halo at the threshold");
    opacity->setValue(qRound(HALO_BELOW * 100) - 1);
    require(haloOn() && foregroundColor(tasks, theme::color("ink-muted")).rgb() == theme::color("ink").rgb(),
            "halo and full ink below the threshold");
    tone->setCurrentIndex(tone->findData("light"));
    opacity->setValue(20);
    require(settings.value("floatingText") == "light" &&
                foregroundColor(tasks, theme::color("ink")).rgb() == QColor("#f5f7fa").rgb(),
            "light text persists and applies");
    QImage dark(floating->size(), QImage::Format_RGB32);
    {
        QPainter painter(&dark);
        QLinearGradient gradient(0, 0, dark.width(), dark.height());
        gradient.setColorAt(0, QColor("#10141c"));
        gradient.setColorAt(1, QColor("#2a3242"));
        painter.fillRect(dark.rect(), gradient);
        painter.drawImage(0, 0, floating->grab().toImage());
    }
    dark.save(dir + "/floating-light-text-20-over-dark.png");
    // Opaque, the calendar page's day list is a dark panel under light text too.
    opacity->setValue(100);
    stack->setCurrentIndex(1);
    setDate(today.addDays(40));
    QApplication::processEvents();
    const QImage lightCalendar = floating->grab().toImage();
    lightCalendar.save(dir + "/floating-light-text-calendar.png");
    require(floatDay->count() == 0 &&
                lightCalendar.pixelColor(floatDay->mapTo(floating, QPoint(4, floatDay->height() / 2)))
                        .lightness() < 80,
            "the day list is a dark panel under light text");
    stack->setCurrentIndex(0);
    setDate(today);
    opacity->setValue(20);
    tone->setCurrentIndex(tone->findData("dark"));
    {
        QPainter painter(&dark);
        QLinearGradient gradient(0, 0, dark.width(), dark.height());
        gradient.setColorAt(0, QColor("#10141c"));
        gradient.setColorAt(1, QColor("#2a3242"));
        painter.fillRect(dark.rect(), gradient);
        painter.drawImage(0, 0, floating->grab().toImage());
    }
    dark.save(dir + "/floating-halo-20-over-dark.png");
    halo->setChecked(false);
    require(settings.value("floatingHalo") == false && !haloOn(), "halo can be turned off");
    settingsWindow->findChild<QListWidget *>("settingsNavigation")->setCurrentRow(0);
    for (auto button : settingsWindow->findChildren<QPushButton *>())
        if (button->text() == "恢复本页默认值")
            button->click();
    require(halo->isChecked() && tone->currentData() == "dark" && !haloOn(), "page reset restores the defaults");
    settingsWindow->reject();
    QApplication::processEvents();

    // #17: the application icon reaches every window, the tray's source and the executable itself.
    const auto sizes = qApp->windowIcon().availableSizes();
    require(sizes.contains(QSize(16, 16)) && sizes.contains(QSize(24, 24)) && sizes.contains(QSize(256, 256)) &&
                floating->windowIcon().pixmap(16, 16).toImage().pixelColor(8, 1).rgb() == QColor("#2563eb").rgb(),
            "application icon set on the windows");
#ifdef Q_OS_WIN
    const auto exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath()).toStdWString();
    require(ExtractIconExW(exe.c_str(), -1, nullptr, nullptr, 0) >= 1, "the executable carries the icon");
#endif
}