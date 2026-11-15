#include "app/controller.h"
#include "ui/windows/settings.h"
#include "ui/widgets/smartentry.h"
#include "ui/widgets/widgettools.h"
#include <algorithm>

void Controller::updateEditorMode() {
    bool plan = type->currentData().toString() == "plan";
    taskDateBox->setVisible(!plan);
    taskTimeBox->setVisible(!plan);
    planEditor->setVisible(plan);
    taskDateBox->parentWidget()->setVisible(!plan);
    typeRuleRow->setSecondVisible(plan);
    showCalendar->setVisible(plan);
    bool total = planEditor->rule().value("kind").toString() == "total";
    showCalendar->setText(total ? "在日历标记结束日期" : "在日历显示");
    showCalendar->setEnabled(!total || !planEditor->rule().value("end").toString().isEmpty());
    saveButton->setText((selected ? "保存" : "添加") + QString(plan ? "计划" : "任务"));
}
QJsonObject Controller::editorContents() const {
    const bool plan = type->currentData().toString() == "plan";
    QJsonObject result{{"title", title->text().trimmed()},
                       {"type", type->currentData().toString()},
                       {"group", group->currentData().toString()},
                       {"notes", notes->toPlainText()},
                       {"pinned", pinned->isChecked()}};
    if (plan) {
        result.insert("rule", planEditor->rule());
        result.insert("goal", planEditor->goal());
        result.insert("time", planEditor->rule().value("kind").toString() == "total" ? QString()
                                                                                     : planEditor->time());
        result.insert("calendar", showCalendar->isChecked() && showCalendar->isEnabled());
    } else {
        result.insert("date", date->text().trimmed());
        result.insert("time", time->text().trimmed());
    }
    return result;
}
bool Controller::hasEditorChanges() const {
    // An unnamed form is only a date-selection placeholder, not a started creation.
    return (selected || !title->text().trimmed().isEmpty()) && editorContents() != editorBaseline;
}
bool Controller::eventFilter(QObject *watched, QEvent *event) {
    if (watched == date) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress) {
            ++dateFocusGeneration;
            dateFieldActive = true;
        } else if (event->type() == QEvent::FocusOut) {
            const int generation = ++dateFocusGeneration;
            // Let the immediately following calendar press consume the date-field intent.
            QTimer::singleShot(0, this, [this, generation] {
                if (generation == dateFocusGeneration && !date->hasFocus())
                    dateFieldActive = false;
            });
        }
    }
    return QObject::eventFilter(watched, event);
}
void Controller::fillEditor() {
    refreshing = true;
    dateManuallyEdited = false;
    title->setProperty("entryError", QString());
    dateFieldActive = false;
    calendarDateEditPending = false;
    date->clearFocus();
    auto found = std::find_if(items.begin(), items.end(), [this](const Item &i) { return i.id == selected; });
    editor->setEnabled(!settingsPreview);
    saveButton->setEnabled(!settingsPreview);
    deleteButton->setEnabled(!settingsPreview);
    editorHeader->setAccessibleName(selected ? "编辑事项" : "新建事项 · " + day.toString(Qt::ISODate));
    saveButton->setText(selected ? "保存修改" : "添加事项");
    deleteButton->setVisible(selected != 0);
    Item i;
    if (found != items.end())
        i = *found;
    if (!selected) {
        i.date = day.toString(Qt::ISODate);
        i.group = settings.value("defaultType", "other").toString();
        i.showCalendar = settings.value("planDefaultCalendar", false).toBool();
    }
    title->setText(i.title);
    date->setText(i.date);
    time->setText(i.time);
    group->setCurrentIndex(qMax(0, group->findData(i.group)));
    type->setCurrentIndex(qMax(0, type->findData(i.type)));
    timeKind->setCurrentIndex(qMax(0, timeKind->findData(i.timeKind)));
    pinned->setChecked(i.pinned);
    done->setChecked(i.done);
    showCalendar->setChecked(i.showCalendar);
    progress->setValue(i.progress);
    planEditor->load(i);
    notes->setPlainText(i.notes);
    updateEditorMode();
    editorBaseline = editorContents();
    dirty = false;
    refreshing = false;
}
bool Controller::saveEditor() {
    if (settingsPreview)
        return false;
    Item x;
    x.id = selected;
    for (auto i : items)
        if (i.id == selected)
            x = i;
    x.title = title->text();
    x.date = date->text().trimmed();
    x.time = time->text().trimmed();
    x.group = group->currentData().toString();
    x.type = type->currentData().toString();
    x.timeKind = timeKind->currentData().toString();
    x.pinned = pinned->isChecked();
    x.done = done->isChecked();
    x.showCalendar = showCalendar->isChecked();
    x.progress = progress->value();
    x.notes = notes->toPlainText();
    if (x.type == "plan") {
        const auto oldRule = x.rule;
        const int oldGoal = x.target;
        x.rule = planEditor->rule();
        x.target = planEditor->goal();
        x.date = x.rule.value("kind").toString() == "interval" ? x.rule.value("first").toString()
                 : x.rule.value("kind").toString() == "total"  ? x.rule.value("end").toString()
                                                               : QString();
        x.time = x.rule.value("kind").toString() == "total" ? QString() : planEditor->time();
        x.showCalendar = showCalendar->isChecked() && showCalendar->isEnabled();
        if (oldRule.isEmpty() && x.rule.value("kind").toString() == "total" && x.progress <= x.target) {
            // Preserve an existing plan's cumulative progress on its first edit.
            x.records.insert(planBucket(x, QDate::currentDate()),
                             QJsonObject{{"count", x.progress}, {"at", x.completedAt}});
        } else if (oldRule != x.rule || oldGoal != x.target) {
            // Historical counters remain in their own rule namespace.
            x.progress = 0;
            x.done = false;
            x.completedAt.clear();
        }
        x = planState(x, QDate::currentDate());
    } else {
        x.rule = {};
        x.progress = 0;
        x.target = 1;
        x.showCalendar = false;
    }
    selected = store.save(x).id;
    dirty = false;
    if (!x.date.isEmpty())
        day = QDate::fromString(x.date, Qt::ISODate);
    refresh();
    return true;
}
bool Controller::leaveEditor() {
    dirty = hasEditorChanges();
    if (!dirty)
        return true;
    auto result = QMessageBox::question(editor->window(), "未保存修改", "是否保存当前事项的修改？",
                                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                        QMessageBox::Save);
    if (result == QMessageBox::Cancel)
        return false;
    if (result == QMessageBox::Discard) {
        fillEditor();
        return true;
    }
    bool ok = false;
    guard([&] { ok = saveEditor(); });
    return ok;
}
void Controller::choose(qint64 id) {
    if (id != selected) {
        if (!leaveEditor())
            return;
        selected = id;
        for (auto i : items)
            if (i.id == id && !i.date.isEmpty())
                day = QDate::fromString(i.date, Qt::ISODate);
        refresh();
    }
    if (itemWindow) {
        itemWindow->show();
        itemWindow->raise();
        itemWindow->activateWindow();
    } else {
        openEditor();
        manager->show();
        manager->raise();
        manager->activateWindow();
    }
}
void Controller::editFloatingItem(qint64 id) {
    if (itemWindow) {
        choose(id);
        return;
    }
    if (id != selected) {
        if (!leaveEditor())
            return;
        selected = id;
        refresh();
    }
    // Temporarily host the existing editor in its own window; restore it on close.
    auto scroll = qobject_cast<QScrollArea *>(editor->parentWidget()->parentWidget());
    auto host = qobject_cast<QVBoxLayout *>(scroll->parentWidget()->layout());
    // The action row is a child layout of the same right-hand panel.
    QLayout *actionRow = nullptr;
    for (int n = 0; n < host->count(); ++n)
        if (auto layout = host->itemAt(n)->layout())
            if (layout->indexOf(saveButton) >= 0)
                actionRow = layout;
    Q_ASSERT(actionRow);
    int titleIndex = host->indexOf(editorHeader);
    host->removeWidget(editorHeader);
    host->removeWidget(scroll);
    host->removeItem(actionRow);
    // The panel in the manager has nothing to show while the form lives in this window.
    const bool panelWasOpen = inspector->isVisibleTo(manager);
    inspector->hide();
    closeEditorButton->hide();
    QDialog dialog(nullptr, Qt::Window | Qt::WindowCloseButtonHint);
    dialog.setObjectName("itemDialog");
    itemWindow = &dialog;
    dialog.setWindowModality(Qt::NonModal);
    dialog.setWindowTitle(settingsPreview ? "查看事项" : "编辑事项");
    dialog.resize(560, type->currentIndex() == 1 ? 660 : 460);
    dialog.setMinimumSize(360, 460);
    auto body = new QVBoxLayout(&dialog);
    body->setContentsMargins(16, 12, 16, 12);
    body->setSpacing(8);
    body->addWidget(editorHeader);
    body->addWidget(scroll, 1);
    body->addLayout(actionRow);
    // Non-modal: the editor stays alive while other app windows can be opened.
    // Closing restores the same draft to the manager without saving or prompting.
    QEventLoop loop;
    connect(&dialog, &QDialog::finished, &loop, &QEventLoop::quit);
    dialog.show();
    if (testing)
        dialog.setAttribute(Qt::WA_DontShowOnScreen);
    loop.exec();
    itemWindow.clear();
    body->removeWidget(editorHeader);
    body->removeWidget(scroll);
    body->removeItem(actionRow);
    auto panel = host->parentWidget();
    editorHeader->setParent(panel);
    scroll->setParent(panel);
    for (int n = 0; n < actionRow->count(); ++n)
        if (auto widget = actionRow->itemAt(n)->widget())
            widget->setParent(panel);
    host->insertWidget(titleIndex, editorHeader);
    host->insertWidget(titleIndex + 1, scroll, 1);
    host->insertLayout(titleIndex + 2, actionRow);
    editorHeader->show();
    scroll->show();
    for (int n = 0; n < actionRow->count(); ++n)
        if (auto widget = actionRow->itemAt(n)->widget())
            widget->show();
    deleteButton->setVisible(selected != 0);
    closeEditorButton->show();
    inspector->setVisible(panelWasOpen);
}
bool Controller::parseEditorTitle() {
    if (settingsPreview || type->currentData().toString() != "task")
        return false;
    const auto parsed = parseEntry(title->text());
    QString error = parsed.error;
    if (error.isEmpty() && parsed.date.isValid() && !date->text().trimmed().isEmpty() &&
        date->text().trimmed() != parsed.date.toString(Qt::ISODate) && (selected || dateManuallyEdited))
        error = "日期框已有值，请先清空日期框或移除名称中的日期";
    if (error.isEmpty() && parsed.time.isValid() && !time->text().trimmed().isEmpty() &&
        time->text().trimmed() != parsed.time.toString("HH:mm"))
        error = "时间框已有值，请先清空时间框或移除名称中的时间";
    title->setProperty("entryError", error);
    if (!error.isEmpty()) {
        QToolTip::showText(title->mapToGlobal(QPoint(0, title->height())), error, title);
        return false;
    }
    QToolTip::hideText();
    if (parsed.spans.isEmpty())
        return false;
    title->setText(parsed.title);
    if (parsed.date.isValid())
        date->setText(parsed.date.toString(Qt::ISODate));
    if (parsed.time.isValid()) {
        time->setText(parsed.time.toString("HH:mm"));
        if (date->text().trimmed().isEmpty())
            date->setText(day.toString(Qt::ISODate));
    }
    return true;
}
