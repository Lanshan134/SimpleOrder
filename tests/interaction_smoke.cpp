#include "app/controller.h"
#include "ui/windows/settings.h"

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
void Controller::interactionSmokeTest() {
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
    tasks->edit = [&, edit](qint64 id) {
        calls << "edit";
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
    tasks->edit = edit;
    tasks->complete = complete;
    reset();
}
