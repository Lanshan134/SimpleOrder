#include "app/controller.h"
#include "ui/windows/settings.h"
#include "ui/widgets/smartentry.h"
#include "ui/widgets/widgettools.h"
#include <algorithm>

void Controller::buildManager() {
    manager = new Window;
    manager->setObjectName("managerWindow");
    manager->setWindowTitle("简序 · 管理");
    auto root = new QVBoxLayout(manager);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    managerSplit = new QSplitter;
    managerSplit->setChildrenCollapsible(false);
    managerSplit->setHandleWidth(1);
    root->addWidget(managerSplit, 1);

    auto left = new QFrame;
    left->setObjectName("listPane");
    auto lv = new QVBoxLayout(left);
    lv->setContentsMargins(12, 12, 12, 12);
    lv->setSpacing(8);
    auto header = new QHBoxLayout;
    auto label = new QLabel("简序");
    label->setObjectName("windowTitle");
    header->addWidget(label);
    header->addStretch();
    auto showFloat = button("悬浮窗"), settingsButton = button("设置"), exit = button("退出");
    header->addWidget(showFloat);
    header->addWidget(settingsButton);
    header->addWidget(exit);
    lv->addLayout(header);
    search = new QLineEdit;
    search->setPlaceholderText("搜索事项");
    search->setAccessibleName("搜索事项");
    addClearButton(search);
    lv->addWidget(search);
    all = new TaskList;
    all->setAccessibleName("全部事项");
    bindList(all);
    lv->addWidget(all, 1);
    showHistory = new QCheckBox("显示历史已完成事项");
    connect(showHistory, &QCheckBox::toggled, this, [this] {
        items = store.all();
        for (auto &i : items)
            i = planState(i, QDate::currentDate());
        updateLists();
    });
    lv->addWidget(showHistory);
    lv->addWidget(quickBar("添加事项", [this](QString t) {
        bool created = false;
        guard([&] { created = add(t); });
        return created;
    }));
    managerSplit->addWidget(left);

    auto center = new QFrame;
    center->setObjectName("calendarPane");
    auto rv = new QVBoxLayout(center);
    rv->setContentsMargins(16, 12, 16, 12);
    rv->setSpacing(8);
    cal = new Calendar;
    cal->setMinimumHeight(250);
    rv->addWidget(cal, 3);
    auto dh = new QHBoxLayout;
    dateLabel = new QLabel;
    dateLabel->setObjectName("dayTitle");
    dh->addWidget(dateLabel);
    dh->addStretch();
    auto dateAdd = button("添加到此日期");
    dateAdd->setObjectName("addDateItem");
    dh->addWidget(dateAdd);
    rv->addLayout(dh);
    dayItems = new TaskList;
    dayItems->setAccessibleName("选中日期的事项");
    dayItems->setProperty("dayList", true);
    dayItems->setProperty("emptyText", "这一天没有事项");
    dayItems->setMinimumHeight(60);
    bindList(dayItems);
    rv->addWidget(dayItems, 2);
    managerSplit->addWidget(center);

    inspector = new QFrame;
    inspector->setObjectName("inspector");
    inspector->setMinimumWidth(340);
    auto iv = new QVBoxLayout(inspector);
    iv->setContentsMargins(16, 12, 16, 12);
    iv->setSpacing(8);
    editorHeader = new QWidget;
    editorHeader->setObjectName("editorHeader");
    auto tabRow = new QHBoxLayout(editorHeader);
    tabRow->setContentsMargins(0, 0, 0, 0);
    tabRow->setSpacing(0);
    editorTabs = new QTabBar;
    editorTabs->setObjectName("editorTabs");
    editorTabs->addTab("任务");
    editorTabs->addTab("计划");
    editorTabs->setExpanding(false);
    editorTabs->setDrawBase(false);
    tabRow->addWidget(editorTabs);
    tabRow->addStretch();
    closeEditorButton = button("关闭");
    closeEditorButton->setToolTip("关闭编辑面板；未保存的内容会保留为草稿");
    tabRow->addWidget(closeEditorButton);
    iv->addWidget(editorHeader);
    auto scroll = new EditorScrollArea;
    scroll->setWheelHeader(editorHeader);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    editor = new QWidget;
    editor->setObjectName("detail");
    scroll->setWidget(editor);
    iv->addWidget(scroll, 1);
    auto form = new QVBoxLayout(editor);
    form->setContentsMargins(0, 4, 8, 4);
    form->setSpacing(10);

    title = new QLineEdit;
    title->setMaxLength(1000);
    title->setToolTip("可输入名称与日期时间，按回车识别后再保存。");
    connect(title, &QLineEdit::returnPressed, this, [this] { parseEditorTitle(); });
    date = new QLineEdit;
    date->installEventFilter(this);
    date->setPlaceholderText("如 2026-11-05，可留空");
    time = new QLineEdit;
    time->setPlaceholderText("如 14:30，可留空");
    date->setToolTip("可清空；先激活此框，再点击日历改期。");
    time->setToolTip("可清空；时间仅作记录，不发送提醒。");
    group = new QComboBox;
    useListPopup(group);
    for (auto c : store.categories())
        group->addItem(typeDot(c.color), c.name, c.id);
    type = new QComboBox(editor);
    type->hide();
    type->addItem("任务", "task");
    type->addItem("计划", "plan");
    timeKind = new QComboBox(editor);
    timeKind->hide();
    timeKind->addItem("截止时间", "due");
    timeKind->addItem("开始时间", "start");
    form->addWidget(editorField("名称", title));
    planEditor = new PlanEditor;
    typeRuleRow = new EditorRow(editorField("类型", group), editorField("计划规则", planEditor->selector()));
    form->addWidget(typeRuleRow);
    taskDateBox = editorField("日期", date);
    taskTimeBox = editorField("时间", time);
    auto taskDates = new EditorRow(taskDateBox, taskTimeBox);
    form->addWidget(taskDates);
    addClearButton(date);
    addClearButton(time);
    attachDatePicker(date, [this] { dateManuallyEdited = true; });
    form->addWidget(planEditor);
    target = planEditor->totalTarget;
    // Completion belongs to the list. Keep the backing controls for compatibility.
    progress = new QSpinBox(editor);
    progress->setRange(0, 9999);
    progress->hide();
    notes = new QPlainTextEdit;
    notes->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    notes->setFixedHeight(64);
    form->addWidget(editorField("备注", notes));
    auto checkrow = new QHBoxLayout;
    pinned = new QCheckBox("置顶");
    done = new QCheckBox(editor);
    done->hide();
    showCalendar = new QCheckBox("在日历显示");
    checkrow->addWidget(pinned);
    checkrow->addStretch();
    checkrow->addWidget(showCalendar);
    form->addLayout(checkrow);
    form->addStretch();
    auto actions = new QHBoxLayout;
    auto save = button("保存修改", "primary"), cancel = button("放弃修改"), del = button("删除", "deleteItem");
    saveButton = save;
    deleteButton = del;
    actions->addWidget(save);
    actions->addWidget(cancel);
    actions->addStretch();
    actions->addWidget(del);
    iv->addLayout(actions);
    managerSplit->addWidget(inspector);
    managerSplit->setStretchFactor(0, 0);
    managerSplit->setStretchFactor(1, 1);
    managerSplit->setStretchFactor(2, 0);
    inspector->hide();
    status = new QLabel("已保存在本机");
    status->setObjectName("managerStatus");
    root->addWidget(status);
    // Start focus in the list rather than ringing the first button in the window.
    all->setFocus();
    scaleManager();
    connect(closeEditorButton, &QPushButton::clicked, this, [this] { closeEditor(); });
    connect(search, &QLineEdit::textChanged, this, [this] { updateLists(); });
    for (auto calendar : {cal, floatCal}) {
        calendar->dateGesture = [this](bool started) {
            calendarDateEditPending = started && dateFieldActive;
            if (started)
                dateFieldActive = false;
        };
        connect(calendar, &QCalendarWidget::selectionChanged, this, [this, calendar] {
            // A mouse click also emits clicked; process it only once, including same-date clicks.
            if (!calendar->mouseDateSelection)
                setDate(calendar->selectedDate());
        });
        connect(calendar, &QCalendarWidget::clicked, this, [this](QDate d) { setDate(d); });
    }
    cal->schedule = [this](qint64 id, QDate d) { guard([&] { schedule(id, d); }); };
    connect(dateAdd, &QPushButton::clicked, this, [this] {
        if (!leaveEditor())
            return;
        selected = 0;
        fillEditor();
        openEditor();
        title->setFocus();
    });

    connect(showFloat, &QPushButton::clicked, this, [this] {
        floating->show();
        floating->raise();
    });
    connect(settingsButton, &QPushButton::clicked, this, [this] { settingsDialog(); });
    connect(exit, &QPushButton::clicked, this, [this] {
        if (leaveEditor()) {
            persistGeometry();
            qApp->quit();
        }
    });
    connect(save, &QPushButton::clicked, this, [this] { guard([&] { saveEditor(); }); });
    connect(cancel, &QPushButton::clicked, this, [this] { fillEditor(); });
    connect(del, &QPushButton::clicked, this, [this] {
        if (settingsPreview)
            return;
        if (!selected)
            return;
        if (QMessageBox::question(editor->window(), "删除事项", "确认删除当前事项？此操作无法撤销。") !=
            QMessageBox::Yes)
            return;
        guard([&] {
            store.remove(selected);
            selected = 0;
            dirty = false;
            refresh();
            closeEditor();
        });
    });
    connect(date, &QLineEdit::textEdited, this, [this] { dateManuallyEdited = true; });
    auto changed = [this] {
        if (!refreshing) {
            dirty = hasEditorChanges();
            status->setText(dirty ? "有未保存的草稿" : "内容未改变");
        }
    };
    for (auto w : {title, date, time})
        connect(w, &QLineEdit::textChanged, this, changed);
    for (auto w : {group, type, timeKind})
        connect(w, QOverload<int>::of(&QComboBox::currentIndexChanged), this, changed);
    for (auto w : {pinned, done, showCalendar})
        connect(w, &QCheckBox::toggled, this, changed);
    connect(notes, &QPlainTextEdit::textChanged, this, changed);
    for (auto w : {progress, target})
        connect(w, QOverload<int>::of(&QSpinBox::valueChanged), this, changed);
    planEditor->onChanged([this, changed] {
        updateEditorMode();
        changed();
    });
    connect(editorTabs, &QTabBar::currentChanged, this, [this](int n) { type->setCurrentIndex(n); });
    connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int n) {
        QSignalBlocker blocker(editorTabs);
        editorTabs->setCurrentIndex(n);
        updateEditorMode();
    });
}
// The window and its panes grow with the Windows text size, so larger text still fits its columns.
void Controller::scaleManager() {
    const qreal factor = theme::textScale;
    // Painted rows and calendar marks follow the text size like the stylesheet text does.
    manager->setProperty("uiScale", factor);
    const QSize screen = QGuiApplication::primaryScreen()->availableGeometry().size();
    manager->setMinimumSize(QSize(qRound(1000 * factor), qRound(640 * factor)).boundedTo(screen));
    manager->resize(QSize(qRound(1180 * factor), qRound(780 * factor)).boundedTo(screen));
    inspector->setMinimumWidth(qRound(340 * factor));
    managerSplit->setSizes({qRound(340 * factor), qRound(840 * factor), qRound(400 * factor)});
    group->setIconSize(QSize(qRound(16 * factor), qRound(16 * factor)));
    reloadCategories();
    arrangeFieldButtons(manager);
}
void Controller::openEditor() {
    if (itemWindow || inspector->isVisibleTo(manager))
        return;
    if (!selected && !hasEditorChanges())
        fillEditor();
    inspector->show();
    auto sizes = managerSplit->sizes();
    const int width = qMax(inspector->minimumWidth(), qRound(400 * theme::textScale));
    sizes[1] = qMax(cal->minimumWidth() + 32, sizes[1] + sizes[2] - width);
    sizes[2] = width;
    managerSplit->setSizes(sizes);
}
void Controller::closeEditor() {
    if (itemWindow) {
        itemWindow->close();
        return;
    }
    inspector->hide();
    // An untouched form lets go of its item, so later calendar clicks only browse.
    // Unsaved changes stay as a draft until the item is opened again.
    if (!hasEditorChanges() && selected) {
        selected = 0;
        dirty = false;
        refresh();
    }
}
