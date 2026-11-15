#include "app/controller.h"
#include "ui/windows/settings.h"

bool Controller::settingsSmokeTest(const QString &directory) {
    auto require = [](bool ok, const char *label) {
        if (!ok)
            throw std::runtime_error(label);
    };
    // Exercise the production non-modal settings lifecycle and its preview-only gate.
    settingsDialog();
    require(settingsWindow && !settingsWindow->isModal() && settingsPreview,
            "settings are nonmodal with preview gate");
    auto live = settingsWindow.data();
    settingsDialog();
    require(settingsWindow == live, "settings window reused");
    require(floating->isEnabled() && manager->isEnabled() && !editor->isEnabled() &&
                QApplication::activeModalWidget() == nullptr,
            "main windows remain interactive while editor is locked");
    auto originalRows = store.all();
    qint64 previewId = originalRows.first().id;
    auto original = Store::json(originalRows.first());
    int countBeforePreview = originalRows.size();
    complete(previewId);
    add("must not add during preview");
    schedule(previewId, QDate::currentDate().addDays(9));
    require(!saveEditor(), "preview save denied");
    all->togglePin(previewId);
    require(store.all().size() == countBeforePreview && Store::json(store.all().first()) == original,
            "preview mutations blocked");
    auto menu = all->createItemMenu(previewId);
    require(menu && !menu->findChild<QAction *>("togglePin")->isEnabled(),
            "preview context mutation disabled");
    delete menu;
    bool expanded = pinExpand;
    pinMore->click();
    require(pinExpand != expanded, "preview allows expansion");
    pinMore->click();
    auto handle = floating->findChild<QLabel *>("windowTitle");
    auto start = handle->rect().center();
    auto global = handle->mapToGlobal(start);
    auto position = floating->pos();
    QMouseEvent press(QEvent::MouseButtonPress, start, global, Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(handle, &press);
    QMouseEvent move(QEvent::MouseMove, start + QPoint(20, 16), global + QPoint(20, 16), Qt::NoButton,
                     Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(handle, &move);
    QMouseEvent release(QEvent::MouseButtonRelease, start + QPoint(20, 16), global + QPoint(20, 16),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(handle, &release);
    require(floating->pos() == position + QPoint(20, 16), "floating header drag works with settings open");
    position = manager->pos();
    manager->move(position + QPoint(15, 15));
    require(manager->pos() == position + QPoint(15, 15), "manager movement remains available");
    choose(previewId);
    require(!editor->isEnabled(), "viewing item remains read only");
    settingsWindow->reject();
    QApplication::processEvents();
    require(!settingsPreview && !settingsWindow && editor->isEnabled(), "closing settings restores editing");
    selected = 0;
    refresh();
    SettingsDialog dialog(*this, manager);
    dialog.show();
    QApplication::processEvents();
    require(dialog.navigation->count() == 6 && dialog.pages->count() == 7, "six settings pages and help");
    dialog.navigation->setCurrentRow(5);
    for (auto button : dialog.findChildren<QPushButton *>())
        if (button->text() == "关于与帮助")
            button->click();
    require(dialog.pages->currentIndex() == 6, "about page opens");
    dialog.navigation->setCurrentRow(5);
    require(dialog.pages->currentIndex() == 5, "return from about to same category");
    dialog.navigation->setCurrentRow(0);
    // Type table: first mouse click selects, the second edits inline.
    dialog.navigation->setCurrentRow(1);
    QApplication::processEvents();
    require(dialog.types->columnCount() == 3 && dialog.types->horizontalHeaderItem(2)->text() == "操作",
            "type item count column removed");
    auto pump = []() {
        QApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QApplication::processEvents();
    };
    auto editorFor = [&]() -> QLineEdit * {
        for (auto editor : dialog.types->findChildren<QLineEdit *>())
            if (editor->isVisible())
                return editor;
        return nullptr;
    };
    auto clickCell = [&](int row, int column) {
        auto point = dialog.types->visualItemRect(dialog.types->item(row, column)).center();
        auto global = dialog.types->viewport()->mapToGlobal(point);
        QMouseEvent down(QEvent::MouseButtonPress, point, global, Qt::LeftButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(dialog.types->viewport(), &down);
        QMouseEvent up(QEvent::MouseButtonRelease, point, global, Qt::LeftButton, Qt::NoButton,
                       Qt::NoModifier);
        QApplication::sendEvent(dialog.types->viewport(), &up);
        pump();
    };
    auto key = [&](QWidget *target, int code) {
        QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier);
        QApplication::sendEvent(target, &event);
        pump();
    };
    dialog.types->clearSelection();
    dialog.types->setCurrentCell(-1, -1);
    auto renameId = dialog.types->item(0, 1)->data(Qt::UserRole).toString();
    clickCell(0, 1);
    require(dialog.types->selectionModel()->isRowSelected(0, QModelIndex()) && !editorFor(),
            "first click selects without editing");
    clickCell(0, 1);
    auto inlineEditor = editorFor();
    require(inlineEditor && QApplication::activeModalWidget() == nullptr,
            "second click opens inline name editor");
    inlineEditor->setText("行内名称验证");
    key(inlineEditor, Qt::Key_Return);
    bool renamed = false;
    for (auto category : store.categories())
        if (category.id == renameId)
            renamed = category.name == "行内名称验证";
    require(renamed && dialog.isVisible(), "inline rename persists without closing settings");
    clickCell(0, 1);
    inlineEditor = editorFor();
    require(inlineEditor, "reenter inline editor");
    inlineEditor->setText("cancel this name");
    key(inlineEditor, Qt::Key_Escape);
    for (auto category : store.categories())
        if (category.id == renameId)
            require(category.name == "行内名称验证", "escape cancels rename");
    require(!dialog.saveTypeName(renameId, " ") && QApplication::activeModalWidget() == nullptr,
            "invalid inline name is rejected without popup");
    int typeCount = store.categories().size();
    dialog.addType();
    inlineEditor = editorFor();
    require(inlineEditor, "add type uses inline editor");
    inlineEditor->setText("行内新增类型");
    key(inlineEditor, Qt::Key_Return);
    require(store.categories().size() == typeCount + 1, "inline new type saved");
    dialog.addType();
    inlineEditor = editorFor();
    require(inlineEditor, "blank draft editor");
    key(inlineEditor, Qt::Key_Escape);
    require(dialog.types->rowCount() == store.categories().size(), "escape removes unsaved empty row");
    dialog.types->clearSelection();
    clickCell(0, 0);
    require(!dialog.typeColorPicker, "first color click only selects");
    clickCell(0, 0);
    require(dialog.typeColorPicker && !dialog.typeColorPicker->isModal(),
            "second color click opens nonmodal picker");
    dialog.typeColorPicker->setCurrentColor(QColor("#228855"));
    dialog.typeColorPicker->accept();
    pump();
    require(groupColor(renameId) == QColor("#228855"), "color changes preserve inline name");
    dialog.types->clearSelection();
    bool actionsOpened = false;
    QTimer::singleShot(0, &dialog, [&] {
        if (auto popup = QApplication::activePopupWidget()) {
            actionsOpened = true;
            popup->close();
        }
    });
    clickCell(0, 2);
    require(actionsOpened && QApplication::activePopupWidget() == nullptr,
            "actions marker opens its menu on the first click");
    dialog.navigation->setCurrentRow(0);
    auto font = dialog.findChild<QSpinBox *>("setting_fontSize");
    auto rows = dialog.findChild<QSpinBox *>("setting_taskRows");
    auto scaling = dialog.findChild<QCheckBox *>("setting_scaleWithWindow");
    require(font && rows && scaling, "floating settings exposed");
    font->setValue(20);
    rows->setValue(8);
    scaling->setChecked(false);
    floating->resize(630, 720);
    QApplication::processEvents();
    require(tasks->font().pixelSize() == 20 && tasks->sizeHint().height() == 320 &&
                tasks->viewport()->height() >= 40 && tasks->viewport()->height() % 40 == 0,
            "settings change the preferred row count while available space keeps whole rows");
    // Resetting only the floating page leaves unrelated preferences untouched.
    settings.setValue("retentionHours", 72);
    dialog.navigation->setCurrentRow(0);
    dialog.resetPage();
    require(settings.value("retentionHours").toInt() == 72 && rows->value() == 5 && font->value() == 15 &&
                scaling->isChecked(),
            "page-scoped reset");
    floating->resize(420, 480);
    QApplication::processEvents();
    auto retention = dialog.findChild<QComboBox *>("setting_retentionHours");
    auto showDone = dialog.findChild<QCheckBox *>("setting_showCompleted");
    Item recent;
    recent.title = "settings completion";
    recent.done = true;
    recent = store.save(recent);
    retention->setCurrentIndex(retention->findData(0));
    require(!visibleItem(recent), "immediate completed hiding");
    retention->setCurrentIndex(retention->findData(48));
    require(visibleItem(recent), "retention preference");
    showDone->setChecked(false);
    require(!visibleItem(recent), "completed visibility preference");
    showDone->setChecked(true);
    auto week = dialog.findChild<QComboBox *>("setting_weekStart");
    week->setCurrentIndex(week->findData(int(Qt::Sunday)));
    auto dots = dialog.findChild<QSpinBox *>("setting_calendarDots");
    dots->setValue(1);
    require(cal->firstDayOfWeek() == Qt::Sunday && floatCal->maxDots == 1,
            "calendar preferences applied to both views");
    auto sort = dialog.findChild<QComboBox *>("setting_sortMode");
    sort->setCurrentIndex(sort->findData("name"));
    Item z, a;
    z.title = "Zebra";
    a.title = "Apple";
    require(sortedItems({z, a}).first().title == "Apple", "name sorting preference");
    auto displayTime = dialog.findChild<QCheckBox *>("setting_showTimes");
    displayTime->setChecked(false);
    require(!tasks->property("showTimes").toBool() && !all->property("showTimes").toBool(),
            "list display flags");
    displayTime->setChecked(true);
    store.saveCategory({"settings_type", "阅读", "#e76f51"});
    reloadCategories();
    dialog.populateTypes();
    dialog.defaultType->setCurrentIndex(dialog.defaultType->findData("settings_type"));
    add("默认类型验证");
    require(store.all().last().group == "settings_type", "default type for quick add");
    auto cats = store.categories();
    QStringList ids;
    for (auto c : cats)
        ids << c.id;
    auto first = ids.first();
    ids.move(0, ids.size() - 1);
    store.reorderCategories(ids);
    require(store.categories().last().id == first, "category order persistence");
    store.removeCategory("settings_type", "study");
    for (auto i : store.all())
        if (i.title == "默认类型验证")
            require(i.group == "study", "delete category transfers items");
    reloadCategories();
    applyPreferences();
    require(settings.value("defaultType").toString() == "other", "missing default type fallback");
    dialog.populateTypes();
    settings.setValue("backupDirectory", directory + "/settings-backups");
    settings.setValue("backupKeep", 2);
    QDir().mkpath(directory + "/settings-backups");
    QFile manual(directory + "/settings-backups/manual.json");
    manual.open(QIODevice::WriteOnly);
    manual.write("manual");
    manual.close();
    for (int n = 0; n < 3; ++n) {
        maybeAutoBackup(true);
        QThread::msleep(3);
    }
    QDir backups(directory + "/settings-backups");
    require(backups.entryList({"lighttask-auto-*.json"}, QDir::Files).size() == 2 &&
                QFile::exists(manual.fileName()),
            "auto backup retention preserves manual files");
    settings.setValue("backupFrequency", "daily");
    auto before = backups.entryList({"lighttask-auto-*.json"}, QDir::Files);
    maybeAutoBackup();
    require(backups.entryList({"lighttask-auto-*.json"}, QDir::Files) == before, "backup frequency honored");
    settings.sync();
    {
        QSettings loaded(settings.fileName(), QSettings::IniFormat);
        require(loaded.value("taskRows", 5).toInt() == 5 &&
                    loaded.value("weekStart").toInt() == int(Qt::Sunday),
                "settings persisted on disk");
    }
    auto startup = dialog.findChild<QComboBox *>("setting_startupView");
    startup->setCurrentIndex(startup->findData("manager"));
    settings.setValue("defaultView", "calendar");
    {
        Controller reopened(store, settings.fileName(), true);
        reopened.show();
        QApplication::processEvents();
        require(reopened.manager->isVisible() && !reopened.floating->isVisible() && reopened.calendarView,
                "startup preferences after reopen");
    }
    auto close = dialog.findChild<QComboBox *>("setting_closeBehavior");
    close->setCurrentIndex(close->findData("tray"));
    dirty = false;
    closeWindow(false);
    require(floating->isVisible() && !manager->isVisible(), "tray fallback keeps a window reachable");
    manager->show();
    {
        Store isolated(directory + "/clear-test.sqlite");
        isolated.seed();
        isolated.backup(directory + "/before-clear-test.json");
        isolated.clearItems();
        require(isolated.all().isEmpty() && isolated.categories().size() == 4, "clear keeps types");
        isolated.importJson(directory + "/before-clear-test.json");
        require(isolated.all().size() == 12, "clear backup restores tasks");
    }
    // Return to defaults before rendering each of the actual settings pages.
    for (int page : {0, 2, 3, 4, 5}) {
        dialog.navigation->setCurrentRow(page);
        dialog.resetPage();
    }
    settings.remove("defaultView");
    applyPreferences();
    dialog.populateTypes();
    for (int page = 0; page < 6; ++page) {
        dialog.navigation->setCurrentRow(page);
        QApplication::processEvents();
        dialog.grab().save(directory + QString("/settings-%1.png").arg(page));
    }
    dialog.close();
    return true;
}
