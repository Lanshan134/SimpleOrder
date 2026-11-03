#include "settings.h"
#include <algorithm>

namespace {
class TypeTable : public QTableWidget {
    bool selectedAtPress = false;
    int pressedRow = -1;

  public:
    std::function<void(int, int, bool)> activate;
    TypeTable() : QTableWidget(0, 3) {}

  protected:
    void remember(QMouseEvent *event) {
        auto i = indexAt(event->pos());
        pressedRow = i.row();
        selectedAtPress = i.isValid() && selectionModel()->isRowSelected(i.row(), QModelIndex());
    }
    void mousePressEvent(QMouseEvent *event) override {
        remember(event);
        QTableWidget::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override {
        remember(event);
        QTableWidget::mouseDoubleClickEvent(event);
    }
    void mouseReleaseEvent(QMouseEvent *event) override {
        auto i = indexAt(event->pos());
        QTableWidget::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton && i.isValid() && i.row() == pressedRow && activate)
            activate(i.row(), i.column(), selectedAtPress);
    }
    void keyPressEvent(QKeyEvent *event) override {
        if ((event->key() == Qt::Key_F2 || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
            currentRow() >= 0) {
            setCurrentCell(currentRow(), 1);
            editItem(item(currentRow(), 1));
            event->accept();
            return;
        }
        QTableWidget::keyPressEvent(event);
    }
};
class TypeDelegate : public QStyledItemDelegate {
  public:
    std::function<bool(QString, QString)> save;
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &,
                          const QModelIndex &index) const override {
        if (index.column() != 1)
            return nullptr;
        auto editor = new QLineEdit(parent);
        editor->setMaxLength(40);
        editor->setProperty("typeId", index.data(Qt::UserRole));
        return editor;
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override {
        auto input = qobject_cast<QLineEdit *>(editor);
        if (input && save && save(index.data(Qt::UserRole).toString(), input->text()))
            model->setData(index, input->text().trimmed());
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        if (index.column() != 0) {
            QStyledItemDelegate::paint(p, option, index);
            return;
        }
        p->save();
        p->fillRect(option.rect, option.state & QStyle::State_Selected ? QColor("#dce8f6") : Qt::white);
        p->setRenderHint(QPainter::Antialiasing);
        p->setPen(Qt::NoPen);
        p->setBrush(QColor(index.data(Qt::UserRole + 1).toString()));
        p->drawEllipse(QPointF(option.rect.center()), 6, 6);
        p->restore();
    }
};
class ToggleSwitch : public QCheckBox {
  public:
    ToggleSwitch() {
        setFixedSize(38, 24);
        setCursor(Qt::PointingHandCursor);
    }

  protected:
    bool hitButton(const QPoint &p) const override {
        return rect().contains(p);
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(isEnabled() ? (isChecked() ? QColor("#557da9") : QColor("#cbd5e2")) : QColor("#e4e8ee"));
        p.drawRoundedRect(QRectF(1, 2, 36, 20), 10, 10);
        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(isChecked() ? 19 : 3, 4, 16, 16));
        if (hasFocus()) {
            p.setPen(QPen(QColor("#8ca9cd"), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(.5, .5, 37, 23), 11, 11);
        }
    }
};
QLabel *hint(const QString &text) {
    auto label = new QLabel(text);
    label->setWordWrap(true);
    label->setObjectName("settingsHint");
    return label;
}
QPushButton *action(const QString &text) {
    auto b = new QPushButton(text);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
QStringList keysForPage(int page) {
    if (page == 0)
        return {"alwaysOnTop", "frameless", "opacity",  "textOpacity", "fontSize",        "scaleWithWindow",
                "taskRows",    "pinRows",   "planRows", "defaultView", "rememberGeometry"};
    if (page == 2)
        return {"showCompleted", "retentionHours", "sortMode",    "undatedLast",
                "showDates",     "showTimes",      "showProgress"};
    if (page == 3)
        return {"weekStart", "outsideDates", "calendarDots", "calendarCompleted", "planDefaultCalendar"};
    if (page == 4)
        return {"startupView", "closeBehavior"};
    if (page == 5)
        return {"backupFrequency", "backupKeep", "backupDirectory"};
    return {};
}
} // namespace

SettingsDialog::SettingsDialog(Controller &controller, QWidget *parent) : QDialog(parent), c(controller) {
    setWindowTitle("简序 · 设置");
    setWindowModality(Qt::NonModal);
    setObjectName("settingsDialog");
    resize(860, 660);
    setMinimumSize(760, 560);
    if (c.testing)
        setAttribute(Qt::WA_DontShowOnScreen);
    setStyleSheet(
        "QDialog#settingsDialog {background:#f3f6fa;} QListWidget#settingsNavigation "
        "{background:#edf2f8;border:0;font-size:15px;} QListWidget#settingsNavigation::item {padding:12px "
        "15px;border-radius:7px;} QListWidget#settingsNavigation::item:selected "
        "{background:#dbe7f5;color:#315f9b;} QLabel#settingsPageTitle {font-size:22px;font-weight:600;} "
        "QLabel#settingsHint {font-size:12px;color:#7b8797;} QGroupBox {background:white;border:1px solid "
        "#e1e7f0;border-radius:10px;margin-top:14px;padding:14px 12px 10px;} QGroupBox::title "
        "{subcontrol-origin:margin;left:13px;padding:0 4px;color:#60748e;font-size:13px;} QPushButton#danger "
        "{color:#b65353;} QTableWidget {background:white;border:1px solid "
        "#e1e7f0;border-radius:8px;gridline-color:#edf1f7;} QHeaderView::section "
        "{background:#edf2f8;border:0;padding:8px;color:#72839a;}");
    auto root = new QHBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(16);
    auto left = new QVBoxLayout;
    navigation = new QListWidget;
    navigation->setObjectName("settingsNavigation");
    navigation->setFixedWidth(150);
    navigation->addItems({"悬浮窗", "类型", "事项与显示", "日历", "启动与操作", "数据与备份"});
    left->addWidget(navigation, 1);
    auto about = action("关于与帮助");
    left->addWidget(about);
    root->addLayout(left);
    auto right = new QVBoxLayout;
    pages = new QStackedWidget;
    right->addWidget(pages, 1);
    auto footer = new QHBoxLayout;
    saved = hint("");
    saved->setToolTip("设置即时生效并自动保存。");
    footer->addWidget(saved, 1);
    reset = action("恢复本页默认值");
    auto close = action("关闭");
    footer->addWidget(reset);
    footer->addWidget(close);
    right->addLayout(footer);
    root->addLayout(right, 1);
    floatingPage();
    typePage();
    displayPage();
    calendarPage();
    startupPage();
    dataPage();
    aboutPage();
    connect(navigation, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0)
            return;
        pages->setCurrentIndex(row);
        reset->setEnabled(!keysForPage(row).isEmpty());
    });
    connect(about, &QPushButton::clicked, this, [this] {
        QSignalBlocker block(navigation);
        navigation->setCurrentRow(-1);
        pages->setCurrentIndex(6);
        reset->setEnabled(false);
    });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    connect(reset, &QPushButton::clicked, this, [this] { resetPage(); });
    navigation->setCurrentRow(0);
}
QWidget *SettingsDialog::page(const QString &title, const QString &description) {
    auto content = new QWidget;
    auto layout = new QVBoxLayout(content);
    layout->setContentsMargins(1, 0, 8, 8);
    layout->setSpacing(12);
    auto label = new QLabel(title);
    label->setObjectName("settingsPageTitle");
    layout->addWidget(label);
    label->setToolTip(description);
    layout->addStretch();
    auto scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setWidget(content);
    pages->addWidget(scroll);
    return content;
}
QFormLayout *SettingsDialog::section(QWidget *parent, const QString &name) {
    auto box = new QGroupBox(name);
    auto form = new QFormLayout(box);
    form->setContentsMargins(14, 20, 14, 12);
    form->setVerticalSpacing(14);
    form->setHorizontalSpacing(22);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto layout = static_cast<QVBoxLayout *>(parent->layout());
    layout->insertWidget(layout->count() - 1, box);
    return form;
}
void SettingsDialog::commit(const QString &key, const QVariant &value) {
    c.guard([&] {
        c.settings.setValue(key, value);
        c.settings.sync();
        if (c.settings.status() != QSettings::NoError)
            throw std::runtime_error("设置文件保存失败");
        c.applyPreferences();
        if (readers.contains("lastAutoBackup"))
            readers["lastAutoBackup"]();
        saved->setText(key == "startupView" || key == "defaultView" ? "已保存，下次启动生效" : "已保存");
    });
}
QCheckBox *SettingsDialog::toggle(QFormLayout *form, const QString &label, const QString &key,
                                  bool fallback) {
    auto box = new ToggleSwitch;
    box->setObjectName("setting_" + key);
    box->setAccessibleName(label);
    box->setChecked(c.settings.value(key, fallback).toBool());
    form->addRow(label, box);
    readers[key] = [this, key, fallback, box] {
        QSignalBlocker b(box);
        box->setChecked(c.settings.value(key, fallback).toBool());
    };
    connect(box, &QCheckBox::toggled, this, [this, key](bool on) { commit(key, on); });
    return box;
}
QSpinBox *SettingsDialog::number(QFormLayout *form, const QString &label, const QString &key, int min,
                                 int max, int fallback, const QString &suffix) {
    auto box = new QSpinBox;
    box->setObjectName("setting_" + key);
    box->setRange(min, max);
    box->setSuffix(suffix);
    box->setValue(c.settings.value(key, fallback).toInt());
    box->setMaximumWidth(160);
    form->addRow(label, box);
    readers[key] = [this, key, fallback, box] {
        QSignalBlocker b(box);
        box->setValue(c.settings.value(key, fallback).toInt());
    };
    connect(box, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [this, key](int value) { commit(key, value); });
    return box;
}
QComboBox *SettingsDialog::choices(QFormLayout *form, const QString &label, const QString &key,
                                   const QList<QPair<QString, QVariant>> &options, QVariant fallback) {
    auto box = new QComboBox;
    box->setObjectName("setting_" + key);
    for (auto v : options)
        box->addItem(v.first, v.second);
    box->setCurrentIndex(qMax(0, box->findData(c.settings.value(key, fallback))));
    form->addRow(label, box);
    readers[key] = [this, key, fallback, box] {
        QSignalBlocker b(box);
        box->setCurrentIndex(qMax(0, box->findData(c.settings.value(key, fallback))));
    };
    connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, key, box](int) { commit(key, box->currentData()); });
    return box;
}
void SettingsDialog::floatingPage() {
    auto p = page("悬浮窗设置", "调整外观与布局；修改后可直接观察悬浮窗。字号与跟随窗口缩放独立设置。");
    auto appearance = section(p, "外观");
    toggle(appearance, "始终置顶", "alwaysOnTop", false);
    toggle(appearance, "无边框悬浮窗", "frameless", true);
    auto opacityControl = [&](const QString &key, const QString &label, int minimum) {
        auto opacity = new QWidget;
        auto row = new QHBoxLayout(opacity);
        row->setContentsMargins(0, 0, 0, 0);
        auto slider = new QSlider(Qt::Horizontal);
        slider->setObjectName("setting_" + key);
        slider->setRange(minimum, 100);
        slider->setValue(c.settings.value(key, 100).toInt());
        auto value = new QLabel(QString::number(slider->value()) + "%");
        row->addWidget(slider, 1);
        row->addWidget(value);
        appearance->addRow(label, opacity);
        readers[key] = [this, key, slider, value] {
            QSignalBlocker b(slider);
            slider->setValue(c.settings.value(key, 100).toInt());
            value->setText(QString::number(slider->value()) + "%");
        };
        connect(slider, &QSlider::valueChanged, this, [this, key, value](int n) {
            value->setText(QString::number(n) + "%");
            commit(key, n);
        });
    };
    opacityControl("opacity", "窗口背景不透明度", 0);
    opacityControl("textOpacity", "文字不透明度", 0);
    auto layout = section(p, "文字与布局");
    number(layout, "基础字号", "fontSize", 12, 22, 15, " px");
    toggle(layout, "随窗口大小缩放", "scaleWithWindow", true);
    number(layout, "Task 可见行数", "taskRows", 3, 12, 5, " 行");
    number(layout, "置顶默认显示", "pinRows", 1, 6, 2, " 行");
    number(layout, "计划默认显示", "planRows", 1, 6, 2, " 行");
    auto display = section(p, "启动显示与位置");
    choices(display, "悬浮窗启动内容", "defaultView", {{"任务列表", "tasks"}, {"日历", "calendar"}}, "tasks");
    toggle(display, "记住窗口位置与大小", "rememberGeometry", true);
    auto restore = action("恢复悬浮窗大小与位置");
    display->addRow(restore);
    connect(restore, &QPushButton::clicked, this, [this] {
        c.floating->resize(420, 480);
        c.floating->move(QGuiApplication::primaryScreen()->availableGeometry().topLeft() + QPoint(40, 60));
        c.persistGeometry();
        saved->setText("已恢复悬浮窗位置与大小");
    });
}
void SettingsDialog::typePage() {
    auto p = page("类型设置", "首次点击选中整行，再次点击名称直接编辑。删除类型时可选择事项转移位置。");
    auto form = section(p, "自定义类型");
    auto toolbar = new QHBoxLayout;
    auto add = action("＋ 新增类型"), up = action("↑ 上移"), down = action("↓ 下移");
    toolbar->addWidget(add);
    toolbar->addStretch();
    toolbar->addWidget(up);
    toolbar->addWidget(down);
    form->addRow(toolbar);
    auto table = new TypeTable;
    types = table;
    types->setObjectName("typesTable");
    types->setHorizontalHeaderLabels({"颜色", "名称", "操作"});
    types->verticalHeader()->hide();
    types->setSelectionBehavior(QAbstractItemView::SelectRows);
    types->setSelectionMode(QAbstractItemView::SingleSelection);
    types->setEditTriggers(QAbstractItemView::NoEditTriggers);
    types->setMinimumHeight(230);
    types->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    types->setColumnWidth(0, 68);
    types->setColumnWidth(2, 65);
    auto delegate = new TypeDelegate(types);
    delegate->save = [this](QString id, QString name) { return saveTypeName(id, name); };
    types->setItemDelegate(delegate);
    table->activate = [this](int row, int column, bool selected) { activateTypeCell(row, column, selected); };
    connect(delegate, &QAbstractItemDelegate::closeEditor, this, [this] {
        QTimer::singleShot(0, this, [this] {
            QSet<QString> persisted, editing;
            for (auto cat : c.store.categories())
                persisted.insert(cat.id);
            for (auto input : types->findChildren<QLineEdit *>())
                if (input->isVisible())
                    editing.insert(input->property("typeId").toString());
            for (int row = types->rowCount() - 1; row >= 0; --row) {
                auto id = types->item(row, 1)->data(Qt::UserRole).toString();
                if (!persisted.contains(id) && !editing.contains(id) &&
                    types->item(row, 1)->text().trimmed().isEmpty())
                    types->removeRow(row);
            }
        });
    });
    form->addRow(types);
    auto defaults = section(p, "添加默认值");
    defaultType = new QComboBox;
    defaultType->setObjectName("setting_defaultType");
    defaults->addRow("新事项默认类型", defaultType);
    populateTypes();
    connect(defaultType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        if (defaultType->currentIndex() >= 0)
            commit("defaultType", defaultType->currentData());
    });
    connect(add, &QPushButton::clicked, this, [this] { addType(); });
    connect(up, &QPushButton::clicked, this, [this] { moveType(-1); });
    connect(down, &QPushButton::clicked, this, [this] { moveType(1); });
}
void SettingsDialog::refreshDefaultTypes() {
    QSignalBlocker block(defaultType);
    defaultType->clear();
    for (auto cat : c.store.categories())
        defaultType->addItem(cat.name, cat.id);
    defaultType->setCurrentIndex(qMax(0, defaultType->findData(c.settings.value("defaultType", "other"))));
}
void SettingsDialog::populateTypes() {
    QString selected;
    if (types->currentRow() >= 0 && types->item(types->currentRow(), 1))
        selected = types->item(types->currentRow(), 1)->data(Qt::UserRole).toString();
    QSignalBlocker blocker(types);
    types->clearContents();
    types->setRowCount(0);
    for (auto cat : c.store.categories()) {
        int row = types->rowCount();
        types->insertRow(row);
        types->setRowHeight(row, 44);
        auto color = new QTableWidgetItem;
        color->setData(Qt::UserRole + 1, cat.color);
        color->setToolTip("选中该行后再次点击修改颜色");
        types->setItem(row, 0, color);
        auto name = new QTableWidgetItem(cat.name);
        name->setData(Qt::UserRole, cat.id);
        name->setFlags(name->flags() | Qt::ItemIsEditable);
        name->setToolTip("选中该行后再次点击名称，或按 F2");
        types->setItem(row, 1, name);
        auto more = new QTableWidgetItem("⋯");
        more->setTextAlignment(Qt::AlignCenter);
        more->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        types->setItem(row, 2, more);
        color->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        if (cat.id == selected)
            types->selectRow(row);
    }
    if (defaultType)
        refreshDefaultTypes();
}
void SettingsDialog::addType() {
    const QString id = "type_" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    int row = types->rowCount();
    types->insertRow(row);
    types->setRowHeight(row, 44);
    auto color = new QTableWidgetItem;
    color->setData(Qt::UserRole + 1, "#6b97c6");
    color->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    types->setItem(row, 0, color);
    auto name = new QTableWidgetItem;
    name->setData(Qt::UserRole, id);
    name->setFlags(name->flags() | Qt::ItemIsEditable);
    types->setItem(row, 1, name);
    auto more = new QTableWidgetItem("⋯");
    more->setTextAlignment(Qt::AlignCenter);
    more->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    types->setItem(row, 2, more);
    types->setCurrentCell(row, 1);
    types->selectRow(row);
    types->scrollToItem(name);
    types->editItem(name);
    saved->setText("输入类型名称，Enter 或移开焦点保存，Esc 取消");
}
bool SettingsDialog::saveTypeName(const QString &id, const QString &name) {
    try {
        Category cat{id, name, "#6b97c6"};
        for (auto old : c.store.categories())
            if (old.id == id)
                cat = {id, name, old.color};
        for (int row = 0; row < types->rowCount(); ++row)
            if (types->item(row, 1)->data(Qt::UserRole).toString() == id)
                cat.color = types->item(row, 0)->data(Qt::UserRole + 1).toString();
        c.store.saveCategory(cat);
        c.reloadCategories();
        c.refresh();
        refreshDefaultTypes();
        saved->setText("名称已保存");
        return true;
    } catch (const std::exception &error) {
        saved->setText("未保存：" + QString::fromUtf8(error.what()));
        return false;
    }
}
void SettingsDialog::activateTypeCell(int row, int column, bool alreadySelected) {
    if (!alreadySelected)
        return;
    auto item = types->item(row, 1);
    if (!item)
        return;
    auto id = item->data(Qt::UserRole).toString();
    if (column == 1) {
        types->setCurrentCell(row, 1);
        types->editItem(item);
    } else if (column == 0)
        chooseTypeColor(id);
    else if (column == 2) {
        QMenu menu;
        menu.addAction("重命名", [this, row] {
            types->setCurrentCell(row, 1);
            types->editItem(types->item(row, 1));
        });
        auto remove = menu.addAction("删除并转移事项", [this] { deleteType(); });
        remove->setEnabled(id != "other");
        menu.exec(types->viewport()->mapToGlobal(types->visualItemRect(types->item(row, 2)).bottomLeft()));
    }
}
void SettingsDialog::chooseTypeColor(const QString &id) {
    if (typeColorPicker)
        typeColorPicker->reject();
    QString color;
    for (int row = 0; row < types->rowCount(); ++row)
        if (types->item(row, 1)->data(Qt::UserRole).toString() == id)
            color = types->item(row, 0)->data(Qt::UserRole + 1).toString();
    if (color.isEmpty())
        return;
    auto picker = new QColorDialog(QColor(color), this);
    typeColorPicker = picker;
    picker->setOption(QColorDialog::DontUseNativeDialog);
    picker->setWindowTitle("圆点颜色");
    picker->setWindowModality(Qt::NonModal);
    picker->setAttribute(Qt::WA_DeleteOnClose);
    if (c.testing)
        picker->setAttribute(Qt::WA_DontShowOnScreen);
    connect(picker, &QDialog::accepted, this, [this, picker, id] {
        c.guard([&] {
            int row = -1;
            for (int n = 0; n < types->rowCount(); ++n)
                if (types->item(n, 1)->data(Qt::UserRole).toString() == id)
                    row = n;
            if (row < 0)
                return;
            auto color = picker->selectedColor().name();
            for (auto cat : c.store.categories())
                if (cat.id == id) {
                    cat.color = color;
                    c.store.saveCategory(cat);
                    c.reloadCategories();
                    c.refresh();
                    break;
                }
            types->item(row, 0)->setData(Qt::UserRole + 1, color);
            saved->setText("颜色已保存");
        });
    });
    picker->show();
}
void SettingsDialog::deleteType() {
    int row = types->currentRow();
    if (row < 0)
        return;
    auto id = types->item(row, 1)->data(Qt::UserRole).toString();
    if (id == "other")
        return;
    bool exists = false;
    for (auto category : c.store.categories())
        if (category.id == id)
            exists = true;
    if (!exists) {
        types->removeRow(row);
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle("删除类型并转移事项");
    QVBoxLayout root(&dialog);
    root.addWidget(hint("该类型下的事项会转移到你选择的类型，不会删除事项。"));
    auto destination = new QComboBox;
    for (auto cat : c.store.categories())
        if (cat.id != id)
            destination->addItem(cat.name, cat.id);
    root.addWidget(destination);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    root.addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        c.guard([&] {
            auto to = destination->currentData().toString();
            c.store.removeCategory(id, to);
            if (c.settings.value("defaultType", "other") == id)
                c.settings.setValue("defaultType", to);
            c.reloadCategories();
            c.refresh();
            populateTypes();
            saved->setText("类型已删除，事项已转移");
            dialog.accept();
        });
    });
    dialog.exec();
}
void SettingsDialog::moveType(int direction) {
    int row = types->currentRow();
    if (row < 0)
        return;
    c.guard([&] {
        QStringList ids;
        for (auto cat : c.store.categories())
            ids << cat.id;
        row = ids.indexOf(types->item(row, 1)->data(Qt::UserRole).toString());
        int to = row + direction;
        if (row < 0 || to < 0 || to >= ids.size())
            return;
        ids.swapItemsAt(row, to);
        c.store.reorderCategories(ids);
        c.reloadCategories();
        c.refresh();
        populateTypes();
        types->selectRow(to);
        saved->setText("类型顺序已保存");
    });
}
void SettingsDialog::displayPage() {
    auto p = page("事项与显示", "设置日常列表的可见范围。已完成记录不会自动从数据库删除。");
    auto done = section(p, "完成事项");
    toggle(done, "显示近期已完成事项", "showCompleted", true);
    choices(done, "完成后保留", "retentionHours",
            {{"立即隐藏", 0}, {"1 天", 24}, {"2 天", 48}, {"3 天", 72}, {"7 天", 168}, {"一直显示", -1}}, 48);
    done->parentWidget()->setToolTip("已完成事项淡化、加删除线并排到末尾；历史记录保留。");
    auto order = section(p, "排列方式");
    auto sorting =
        choices(order, "未完成事项排序", "sortMode",
                {{"按日期和时间", "date"}, {"按名称", "name"}, {"新建事项优先", "newest"}}, "date");
    auto undated = toggle(order, "无日期事项排在后面", "undatedLast", true);
    undated->setEnabled(sorting->currentData().toString() == "date");
    connect(sorting, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [sorting, undated] { undated->setEnabled(sorting->currentData().toString() == "date"); });
    auto readSort = readers["sortMode"];
    readers["sortMode"] = [readSort, sorting, undated] {
        readSort();
        undated->setEnabled(sorting->currentData().toString() == "date");
    };
    auto info = section(p, "列表信息");
    toggle(info, "显示日期", "showDates", true);
    toggle(info, "显示时间", "showTimes", true);
    toggle(info, "显示计划进度", "showProgress", true);
}
void SettingsDialog::calendarPage() {
    auto p = page("日历设置", "影响管理窗口与悬浮日历。单个计划是否显示，仍由其详情设置控制。");
    auto layout = section(p, "月历布局");
    choices(layout, "每周起始日", "weekStart", {{"周一", int(Qt::Monday)}, {"周日", int(Qt::Sunday)}},
            int(Qt::Monday));
    toggle(layout, "显示非本月日期", "outsideDates", true);
    number(layout, "每日最多标记数", "calendarDots", 1, 5, 3, " 个");
    auto content = section(p, "事项标记");
    toggle(content, "显示近期已完成标记", "calendarCompleted", true);
    toggle(content, "新计划默认在日历显示", "planDefaultCalendar", false);
    content->parentWidget()->setToolTip("显示已启用日历的计划执行日，超出每日标记数量以 +N 显示。");
}
void SettingsDialog::startupPage() {
    auto p = page("启动与操作", "控制启动界面和窗口关闭方式。");
    auto start = section(p, "启动");
    auto autoStart = new ToggleSwitch;
    autoStart->setObjectName("setting_autostart");
    start->addRow("登录 Windows 后启动", autoStart);
#ifdef Q_OS_WIN
    if (!c.testing) {
        QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      QSettings::NativeFormat);
        // Retain the legacy registry key so renaming does not create duplicate startup entries.
        autoStart->setChecked(run.contains("LightTaskLocal"));
    } else
        autoStart->setEnabled(false);
    connect(autoStart, &QCheckBox::toggled, this, [this, autoStart](bool on) {
        if (c.testing)
            return;
        QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      QSettings::NativeFormat);
        if (on)
            run.setValue("LightTaskLocal",
                         "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) + "\"");
        else
            run.remove("LightTaskLocal");
        run.sync();
        if (run.status() != QSettings::NoError) {
            QSignalBlocker b(autoStart);
            autoStart->setChecked(!on);
            QMessageBox::warning(this, "无法保存", "无法修改当前用户的启动项。");
            return;
        }
        saved->setText("启动项已保存");
    });
#else
    autoStart->setEnabled(false);
    autoStart->setToolTip("当前仅支持 Windows 启动项");
#endif
    choices(start, "启动时打开", "startupView",
            {{"悬浮窗和管理窗口", "both"}, {"仅悬浮窗", "floating"}, {"仅管理窗口", "manager"}}, "both");
    auto close = section(p, "关闭行为");
    choices(close, "点击窗口关闭按钮时", "closeBehavior",
            {{"切换到另一个窗口", "switch"}, {"隐藏到系统托盘", "tray"}, {"退出程序", "exit"}}, "switch");
    close->parentWidget()->setToolTip("系统托盘不可用时，切换到另一个窗口。");
}
void SettingsDialog::dataPage() {
    auto p = page("数据与备份", "备份包含事项、完成时间和类型配置。导入前会保留原数据备份。");
    auto location = section(p, "本地数据");
    auto path = new QLineEdit(QFileInfo(c.settings.fileName()).absolutePath());
    path->setReadOnly(true);
    path->setCursorPosition(0);
    path->setToolTip(path->text());
    location->addRow("数据目录", path);
    auto open = action("打开数据文件夹");
    location->addRow(open);
    connect(open, &QPushButton::clicked, this,
            [this, path] { QDesktopServices::openUrl(QUrl::fromLocalFile(path->text())); });
    auto manual = section(p, "导入与导出");
    auto row = new QHBoxLayout;
    auto exportButton = action("导出备份"), importButton = action("导入数据"),
         restoreButton = action("恢复备份");
    row->addWidget(exportButton);
    row->addWidget(importButton);
    row->addWidget(restoreButton);
    manual->addRow(row);
    connect(exportButton, &QPushButton::clicked, this, [this] {
        auto path = QFileDialog::getSaveFileName(this, "导出备份",
                                                 QDir::homePath() + "/简序-" +
                                                     QDate::currentDate().toString(Qt::ISODate) + ".json",
                                                 "JSON (*.json)");
        if (!path.isEmpty())
            c.guard([&] {
                c.store.backup(path);
                saved->setText("备份已导出");
            });
    });
    connect(importButton, &QPushButton::clicked, this, [this] { importData(); });
    connect(restoreButton, &QPushButton::clicked, this, [this] { importData(true); });
    auto automatic = section(p, "自动备份");
    choices(automatic, "备份频率", "backupFrequency",
            {{"关闭", "off"}, {"每天", "daily"}, {"每周", "weekly"}}, "off");
    number(automatic, "保留最新备份", "backupKeep", 1, 30, 7, " 份");
    auto directory = new QLineEdit(
        c.settings.value("backupDirectory", QFileInfo(c.settings.fileName()).absolutePath() + "/backups")
            .toString());
    directory->setReadOnly(true);
    directory->setCursorPosition(0);
    directory->setToolTip(directory->text());
    auto folder = action("更改");
    auto folderRow = new QHBoxLayout;
    folderRow->addWidget(directory, 1);
    folderRow->addWidget(folder);
    automatic->addRow("备份目录", folderRow);
    readers["backupDirectory"] = [this, directory] {
        directory->setText(
            c.settings.value("backupDirectory", QFileInfo(c.settings.fileName()).absolutePath() + "/backups")
                .toString());
        directory->setCursorPosition(0);
        directory->setToolTip(directory->text());
    };
    connect(folder, &QPushButton::clicked, this, [this, directory] {
        auto path = QFileDialog::getExistingDirectory(this, "备份目录", directory->text());
        if (!path.isEmpty()) {
            directory->setText(path);
            directory->setCursorPosition(0);
            directory->setToolTip(path);
            commit("backupDirectory", path);
        }
    });
    auto last = hint("");
    readers["lastAutoBackup"] = [this, last] {
        auto timestamp =
            QDateTime::fromString(c.settings.value("lastAutoBackup").toString(), Qt::ISODateWithMs);
        last->setText("最近自动备份：" +
                      (timestamp.isValid() ? timestamp.toLocalTime().toString("yyyy-MM-dd HH:mm") : "尚无"));
    };
    readers["lastAutoBackup"]();
    automatic->addRow(last);
    auto now = action("立即备份一次");
    automatic->addRow(now);
    connect(now, &QPushButton::clicked, this, [this, last] {
        c.guard([&] {
            c.maybeAutoBackup(true);
            readers["lastAutoBackup"]();
            saved->setText("备份已保存");
        });
    });
    automatic->parentWidget()->setToolTip("保留数量只作用于自动备份，不清理手动导出的备份。");
    auto danger = section(p, "清空事项");
    auto clear = action("清空全部事项…");
    clear->setObjectName("danger");
    danger->addRow(hint("类型配置会保留。清空前自动保存完整备份，可通过恢复备份找回。"));
    danger->addRow(clear);
    connect(clear, &QPushButton::clicked, this, [this] {
        bool ok = false;
        auto text = QInputDialog::getText(this, "清空事项", "输入「清空」确认删除全部事项；删除前会先备份。",
                                          QLineEdit::Normal, {}, &ok);
        if (!ok || text != "清空")
            return;
        c.guard([&] {
            QString path = QFileInfo(c.settings.fileName()).absolutePath() + "/before-clear-" +
                           QDateTime::currentDateTime().toString("yyyyMMdd-HHmmsszzz") + ".json";
            c.store.backup(path);
            c.store.clearItems();
            c.selected = 0;
            c.dirty = false;
            c.refresh();
            populateTypes();
            QMessageBox::information(this, "已清空", "清空前备份：\n" + path);
        });
    });
}
void SettingsDialog::importData(bool fromBackup) {
    auto directory = fromBackup ? c.settings
                                      .value("backupDirectory",
                                             QFileInfo(c.settings.fileName()).absolutePath() + "/backups")
                                      .toString()
                                : QString();
    auto file =
        QFileDialog::getOpenFileName(this, fromBackup ? "恢复备份" : "导入数据", directory, "JSON (*.json)");
    if (file.isEmpty())
        return;
    if (QMessageBox::question(this, "替换事项", "将替换全部事项及备份中的类型配置，导入前自动备份。继续？") !=
        QMessageBox::Yes)
        return;
    c.guard([&] {
        auto before = QFileInfo(c.settings.fileName()).absolutePath() + "/before-import-" +
                      QDateTime::currentDateTime().toString("yyyyMMdd-HHmmsszzz") + ".json";
        c.store.backup(before);
        c.store.importJson(file);
        c.selected = 0;
        c.dirty = false;
        c.reloadCategories();
        c.applyPreferences();
        c.refresh();
        populateTypes();
        saved->setText("数据已恢复，导入前备份已保留");
    });
}
void SettingsDialog::aboutPage() {
    auto p = page("关于与帮助", "简序 · SimpleOrder · 个人任务管理器");
    auto about = section(p, "当前版本");
    about->addRow("版本", new QLabel(QCoreApplication::applicationVersion() + " · 源码开发版"));
    about->parentWidget()->setToolTip("设置即时保存；恢复本页默认值不影响其他页面。");
    auto help = section(p, "常用操作");
    help->addRow(hint("列表内：Enter 编辑事项，空格完成或记录进度。"));
    help->addRow(hint("• 拖动悬浮窗标题移动，拖动右下角调整大小。\n• "
                      "点击事项名称编辑，右键可置顶或取消置顶。\n• 选中日期后，在右侧表单填写详细新事项。\n• "
                      "对号记录完成；计划按目标次数累计。\n• 修改详情后请保存，再切换事项。"));
}
void SettingsDialog::resetPage() {
    auto keys = keysForPage(pages->currentIndex());
    if (keys.isEmpty())
        return;
    if (!c.testing &&
        QMessageBox::question(this, "恢复本页默认值",
                              "仅恢复本页的选项，事项、类型和备份文件不会删除。继续？") != QMessageBox::Yes)
        return;
    c.guard([&] {
        if (pages->currentIndex() == 4)
            if (auto autostart = findChild<QCheckBox *>("setting_autostart"))
                autostart->setChecked(false);
        for (auto key : keys)
            c.settings.remove(key);
        c.settings.sync();
        c.applyPreferences();
        for (auto key : keys)
            if (readers.contains(key))
                readers[key]();
        saved->setText("本页已恢复默认值");
    });
}

bool Controller::visibleItem(const Item &i) const {
    return !i.done || (settings.value("showCompleted", true).toBool() &&
                       visibleInActive(i, QDateTime::currentDateTimeUtc(),
                                       settings.value("retentionHours", 48).toInt()));
}
QList<Item> Controller::sortedItems(QList<Item> input) const {
    const auto mode = settings.value("sortMode", "date").toString();
    bool undatedLast = settings.value("undatedLast", true).toBool();
    std::stable_sort(input.begin(), input.end(), [&](const Item &a, const Item &b) {
        if (a.done != b.done)
            return !a.done;
        if (mode == "name")
            return QString::localeAwareCompare(a.title, b.title) < 0;
        if (mode == "newest")
            return a.id > b.id;
        if (a.date.isEmpty() != b.date.isEmpty())
            return undatedLast ? !a.date.isEmpty() : a.date.isEmpty();
        if (a.date != b.date)
            return a.date < b.date;
        if (a.time != b.time)
            return a.time < b.time;
        return a.id < b.id;
    });
    return input;
}
void Controller::applyPreferences() {
    bool validDefault = false;
    for (auto category : store.categories())
        if (category.id == settings.value("defaultType", "other").toString())
            validDefault = true;
    if (!validDefault)
        settings.setValue("defaultType", "other");
    setTop(settings.value("alwaysOnTop", false).toBool());
    applyFloatingAppearance();
    scaleFloating(true);
    for (auto list : {pins, tasks, plans, all, dayItems, floatDay})
        for (auto key : QStringList{"showDates", "showTimes", "showProgress"})
            list->setProperty(key.toUtf8(), settings.value(key, true));
    for (auto calendar : {cal, floatCal}) {
        QSignalBlocker b(calendar);
        calendar->setFirstDayOfWeek(Qt::DayOfWeek(settings.value("weekStart", int(Qt::Monday)).toInt()));
        calendar->displayOutside = settings.value("outsideDates", true).toBool();
        calendar->maxDots = settings.value("calendarDots", 3).toInt();
        calendar->showCompleted = settings.value("showCompleted", true).toBool() &&
                                  settings.value("calendarCompleted", true).toBool();
        calendar->retentionHours = settings.value("retentionHours", 48).toInt();
        calendar->repaintCells();
    }
    items = store.all();
    updateLists();
    if (!dirty && !selected)
        fillEditor();
    settings.sync();
    if (!testing)
        maybeAutoBackup();
}
void Controller::closeWindow(bool floatingWindow) {
    persistGeometry();
    auto mode = settings.value("closeBehavior", "switch").toString();
    if (mode == "exit") {
        if (!leaveEditor())
            return;
        qApp->quit();
        return;
    }
    if (mode == "tray" && tray) {
        (floatingWindow ? floating : manager)->hide();
        return;
    }
    (floatingWindow ? floating : manager)->hide();
    auto next = floatingWindow ? manager : floating;
    next->show();
    next->raise();
}
void Controller::maybeAutoBackup(bool force) {
    auto frequency = settings.value("backupFrequency", "off").toString();
    if (!force && frequency == "off")
        return;
    auto last = QDateTime::fromString(settings.value("lastAutoBackup").toString(), Qt::ISODateWithMs);
    int hours = frequency == "weekly" ? 168 : 24;
    auto now = QDateTime::currentDateTimeUtc();
    if (!force && last.isValid() && last.secsTo(now) < hours * 3600)
        return;
    auto path = settings.value("backupDirectory", QFileInfo(settings.fileName()).absolutePath() + "/backups")
                    .toString();
    if (!QDir().mkpath(path))
        throw std::runtime_error("无法创建自动备份目录");
    QDir folder(path);
    auto file = "lighttask-auto-" + now.toString("yyyyMMdd-HHmmsszzz") + ".json";
    store.backup(folder.filePath(file));
    settings.setValue("lastAutoBackup", now.toString(Qt::ISODateWithMs));
    settings.sync();
    auto files = folder.entryList({"lighttask-auto-*.json"}, QDir::Files, QDir::Name);
    files.removeAll(file);
    files.append(file);
    const int keep = qBound(1, settings.value("backupKeep", 7).toInt(), 30);
    QRegularExpression own("^lighttask-auto-[0-9]{8}-[0-9]{9}\\.json$");
    QStringList ownFiles;
    for (auto f : files)
        if (own.match(f).hasMatch())
            ownFiles << f;
    while (ownFiles.size() > keep) {
        auto oldest = ownFiles.takeFirst();
        if (!QFile::remove(folder.filePath(oldest)))
            throw std::runtime_error("旧自动备份清理失败");
    }
}
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
    clickCell(0, 2);
    require(QApplication::activePopupWidget() == nullptr, "first operation click only selects");
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
    require(tasks->font().pixelSize() == 20 && tasks->height() == 320,
            "settings change floating font and visible rows");
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
