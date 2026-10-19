#include "ui.h"
#include "settings.h"
#include <algorithm>
static QHash<QString, Category> categoryCache;
static constexpr int ROW_HEIGHT = 30;

QColor groupColor(const QString &g) {
    return QColor(categoryCache.value(g, {g, g, "#aaaeb8"}).color);
}
QString groupName(const QString &g) {
    return categoryCache.value(g, {g, g, "#aaaeb8"}).name;
}

namespace {
qreal widgetScale(const QWidget *widget) {
    for (auto current = widget; current; current = current->parentWidget())
        if (current->property("uiScale").isValid())
            return current->property("uiScale").toDouble();
    return 1.0;
}
QColor surfaceColor(const QWidget *widget, QColor color, const char *key) {
    for (auto current = widget; current; current = current->parentWidget())
        if (current->property(key).isValid()) {
            color.setAlphaF(current->property(key).toDouble());
            break;
        }
    return color;
}
QColor foregroundColor(const QWidget *widget, QColor color) {
    return surfaceColor(widget, color, "foregroundOpacity");
}
int scaled(const QWidget *widget, int value) {
    return qMax(1, qRound(value * widgetScale(widget)));
}
class RowDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &idx) const override {
        return QSize(scaled(option.widget, 220),
                     scaled(option.widget, idx.data(Qt::UserRole).toLongLong() ? ROW_HEIGHT : 26));
    }
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override {
        p->save();
        const qreal scale = widgetScale(opt.widget);
        p->scale(scale, scale);
        p->setRenderHint(QPainter::Antialiasing);
        QRect r = QRect(qRound(opt.rect.x() / scale), qRound(opt.rect.y() / scale),
                        qRound(opt.rect.width() / scale), qRound(opt.rect.height() / scale))
                      .adjusted(2, 1, -2, -1);
        QFont baseFont = opt.font;
        if (baseFont.pixelSize() > 0)
            baseFont.setPixelSize(qMax(1, qRound(baseFont.pixelSize() / scale)));
        else
            baseFont.setPointSizeF(baseFont.pointSizeF() / scale);
        p->setFont(baseFont);
        if (!idx.data(Qt::UserRole).toLongLong()) {
            p->setPen(foregroundColor(opt.widget, QColor("#7f8b9c")));
            p->drawText(r.adjusted(6, 0, 0, 0), Qt::AlignVCenter, idx.data().toString());
            p->restore();
            return;
        }
        if (opt.state & QStyle::State_Selected)
            p->fillRect(r, surfaceColor(opt.widget, QColor("#eaf1fb"), "surfaceOpacity"));
        else if (opt.state & QStyle::State_MouseOver)
            p->fillRect(r, surfaceColor(opt.widget, QColor("#f4f7fb"), "surfaceOpacity"));
        const QColor c = idx.data(Qt::UserRole + 1).value<QColor>();
        bool plan = idx.data(Qt::UserRole + 2).toBool(), done = idx.data(Qt::UserRole + 3).toBool();
        if (done)
            p->setOpacity(0.62);
        p->setPen(QPen(c, 2));
        p->setBrush(plan ? QBrush(Qt::NoBrush) : QBrush(c));
        p->drawEllipse(QPointF(r.left() + 10, r.center().y()), 4, 4);
        QString detail = idx.data(Qt::UserRole + 4).toString();
        QFont small = baseFont;
        small.setPixelSize(12);
        p->setFont(small);
        int dw = qMin(117, QFontMetrics(small).horizontalAdvance(detail));
        p->setPen(foregroundColor(opt.widget, QColor("#8b94a2")));
        p->drawText(QRect(r.right() - dw - 29, r.top(), dw, r.height()), Qt::AlignVCenter | Qt::AlignRight,
                    detail);
        QFont main = baseFont;
        main.setStrikeOut(done);
        p->setFont(main);
        p->setPen(foregroundColor(opt.widget, QColor(done ? "#6f7d90" : "#293448")));
        QRect text = r.adjusted(24, 0, -dw - 37, 0);
        p->drawText(
            text, Qt::AlignVCenter,
            QFontMetrics(main).elidedText(idx.data().toString(), Qt::ElideRight, qMax(0, text.width())));
        QRect check(r.right() - 23, r.center().y() - 9, 18, 18);
        p->setPen(QPen(QColor("#a3b4c9"), 1));
        p->setBrush(done ? QColor("#7995b6") : Qt::white);
        p->drawRoundedRect(check, 3, 3);
        if (done) {
            p->setPen(QPen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p->drawLine(check.left() + 4, check.center().y(), check.left() + 8, check.bottom() - 4);
            p->drawLine(check.left() + 8, check.bottom() - 4, check.right() - 3, check.top() + 4);
        }
        p->restore();
    }
};
QPushButton *button(const QString &text, const QString &name = {}) {
    auto b = new QPushButton(text);
    if (!name.isEmpty())
        b->setObjectName(name);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
QLabel *heading(const QString &s) {
    auto l = new QLabel(s);
    l->setObjectName("heading");
    l->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return l;
}
QWidget *quickBar(const QString &placeholder, const std::function<void(QString)> &add) {
    auto w = new QWidget;
    w->setObjectName("quickAddBar");
    w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto h = new QHBoxLayout(w);
    h->setContentsMargins(0, 6, 0, 0);
    auto input = new QLineEdit;
    input->setMaxLength(200);
    input->setPlaceholderText(placeholder);
    auto b = button("+", "primary");
    b->setAccessibleName("添加事项");
    h->addWidget(input);
    h->addWidget(b);
    auto submit = [input, add] {
        if (!input->text().trimmed().isEmpty()) {
            add(input->text());
            input->clear();
        }
    };
    QObject::connect(b, &QPushButton::clicked, w, submit);
    QObject::connect(input, &QLineEdit::returnPressed, w, submit);
    return w;
}
} // namespace
TaskList::TaskList(QWidget *parent) : QListWidget(parent) {
    setItemDelegate(new RowDelegate(this));
    setMouseTracking(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    verticalScrollBar()->setSingleStep(ROW_HEIGHT);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setFrameShape(QFrame::NoFrame);
    setMinimumWidth(260);
    setAccessibleName("事项列表，点击编辑，右侧按钮完成或记录进度");
    connect(this, &QListWidget::itemClicked, this, [this](QListWidgetItem *i) {
        if (pressedButton != Qt::LeftButton)
            return;
        auto id = i->data(Qt::UserRole).toLongLong();
        if (!id)
            return;
        const QPoint p = viewport()->mapFromGlobal(QCursor::pos());
        if (p.x() > viewport()->width() - scaled(this, 30) && !property("previewOnly").toBool()) {
            if (complete)
                complete(id);
        } else if (edit)
            edit(id);
    });
    connect(this, &QListWidget::itemActivated, this, [this](QListWidgetItem *i) {
        if (edit && i->data(Qt::UserRole).toLongLong())
            edit(i->data(Qt::UserRole).toLongLong());
    });
}
void TaskList::fill(const QList<Item> &items, bool headings) {
    QSignalBlocker block(this);
    clear();
    int last = -1;
    for (const auto &i : items) {
        int section = i.done ? 3 : i.pinned ? 0 : i.type == "task" ? 1 : 2;
        if (headings && section != last) {
            auto h = new QListWidgetItem(QStringList{"置顶", "Task", "计划", "已完成"}[section], this);
            h->setFlags(Qt::NoItemFlags);
            last = section;
        }
        auto row = new QListWidgetItem(i.title, this);
        row->setData(Qt::UserRole, i.id);
        row->setData(Qt::UserRole + 1, groupColor(i.group));
        row->setData(Qt::UserRole + 2, i.type == "plan");
        row->setData(Qt::UserRole + 3, i.done);
        row->setData(Qt::UserRole + 5, i.pinned);
        bool dates = !property("showDates").isValid() || property("showDates").toBool();
        bool times = !property("showTimes").isValid() || property("showTimes").toBool();
        bool counts = !property("showProgress").isValid() || property("showProgress").toBool();
        QString marker;
        if (i.type == "plan") {
            if (counts)
                marker = QString("%1/%2").arg(i.progress).arg(i.target);
        } else {
            if (dates && !i.date.isEmpty())
                marker = i.date.mid(5);
            if (times && !i.time.isEmpty())
                marker += (marker.isEmpty() ? QString() : " ") + i.time;
        }
        row->setData(Qt::UserRole + 4, marker);
        row->setToolTip(i.title + "\n" + groupName(i.group) + " · " + (i.date.isEmpty() ? "未安排" : i.date) +
                        (i.time.isEmpty() ? QString() : " " + i.time) + "\n" +
                        (i.type == "plan" ? "点击对号记录进度；达标后完成" : "点击对号切换完成"));
    }
}
void TaskList::startDrag(Qt::DropActions) {
    if (property("previewOnly").toBool())
        return;
    auto i = currentItem();
    if (!i || !i->data(Qt::UserRole).toLongLong())
        return;
    auto mime = new QMimeData;
    mime->setData("application/x-lighttask-id", QByteArray::number(i->data(Qt::UserRole).toLongLong()));
    auto drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
}
Calendar::Calendar(QWidget *parent) : QCalendarWidget(parent) {
    setFirstDayOfWeek(Qt::Monday);
    setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    setGridVisible(false);
    setNavigationBarVisible(true);
    setAcceptDrops(true);
    setMinimumSize(330, 250);
    auto table = findChild<QAbstractItemView *>();
    if (table) {
        table->setAcceptDrops(false);
        table->viewport()->setAcceptDrops(false);
        table->viewport()->installEventFilter(this);
    }
    QTextCharFormat weekend;
    weekend.setForeground(QColor("#8995a5"));
    setWeekdayTextFormat(Qt::Saturday, weekend);
    setWeekdayTextFormat(Qt::Sunday, weekend);
}
void Calendar::paintCell(QPainter *p, const QRect &r, const QDate &d) const {
    if (!displayOutside && d.month() != monthShown()) {
        p->fillRect(r, surfaceColor(this, QColor(Qt::white), "surfaceOpacity"));
        return;
    }
    QCalendarWidget::paintCell(p, r, d);
    const qreal scale = widgetScale(this);
    const QRect area(qRound(r.x() / scale), qRound(r.y() / scale), qRound(r.width() / scale),
                     qRound(r.height() / scale));
    QList<Item> hits;
    for (const auto &stored : items) {
        auto i = planState(stored, d);
        if ((!i.done ||
             (showCompleted && visibleInActive(i, QDateTime::currentDateTimeUtc(), retentionHours))) &&
            planOccurs(i, d) && (i.type == "task" || i.showCalendar))
            hits << i;
    }
    if (hits.isEmpty())
        return;
    int shown = hits.size() > maxDots ? qMax(0, maxDots - 1) : hits.size(),
        width = shown * 8 + (hits.size() > maxDots ? 20 : 0), left = area.center().x() - width / 2;
    p->save();
    p->scale(scale, scale);
    p->setRenderHint(QPainter::Antialiasing);
    for (int n = 0; n < shown; ++n) {
        auto i = hits[n];
        p->setOpacity(i.done ? 0.4 : 1.0);
        p->setPen(QPen(groupColor(i.group), 1));
        p->setBrush(i.type == "plan" ? QBrush(Qt::NoBrush) : QBrush(groupColor(i.group)));
        p->drawEllipse(QPointF(left + n * 8 + 3, area.bottom() - 6), 2.5, 2.5);
    }
    if (hits.size() > maxDots) {
        QFont font = p->font();
        font.setPointSize(7);
        p->setFont(font);
        p->setPen(foregroundColor(this, QColor("#737e91")));
        p->drawText(QRect(left + shown * 8, area.bottom() - 14, 25, 14), Qt::AlignVCenter,
                    "+" + QString::number(hits.size() - shown));
    }
    p->restore();
}
QDate Calendar::dateAt(const QPoint &p) const {
    auto table = findChild<QAbstractItemView *>();
    if (!table)
        return {};
    auto idx = table->indexAt(table->viewport()->mapFrom(this, p));
    if (!idx.isValid() || idx.row() < 1 || idx.column() < 1)
        return {};
    int firstNumber = table->model()->index(1, 1).data().toInt();
    QDate first(yearShown(), monthShown(), 1);
    if (firstNumber > 7)
        first = first.addMonths(-1);
    first = QDate(first.year(), first.month(), firstNumber);
    const auto result = first.addDays((idx.row() - 1) * 7 + idx.column() - 1);
    return !displayOutside && result.month() != monthShown() ? QDate() : result;
}
bool Calendar::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto mouse = static_cast<QMouseEvent *>(event);
        auto viewport = qobject_cast<QWidget *>(watched);
        if (mouse->button() == Qt::LeftButton && viewport &&
            dateAt(viewport->mapTo(this, mouse->pos())).isValid()) {
            mouseDateSelection = true;
            if (dateGesture)
                dateGesture(true);
        }
    }
    if (event->type() == QEvent::MouseButtonRelease && mouseDateSelection) {
        QTimer::singleShot(0, this, [this] {
            mouseDateSelection = false;
            if (dateGesture)
                dateGesture(false);
        });
    }

    if (!displayOutside &&
        (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease ||
         event->type() == QEvent::MouseButtonDblClick)) {
        auto viewport = qobject_cast<QWidget *>(watched);
        auto mouse = static_cast<QMouseEvent *>(event);
        if (viewport && !dateAt(viewport->mapTo(this, mouse->pos())).isValid())
            return true;
    }
    return QCalendarWidget::eventFilter(watched, event);
}
void Calendar::dragEnterEvent(QDragEnterEvent *e) {
    if (previewOnly) {
        e->ignore();
        return;
    }
    if (e->mimeData()->hasFormat("application/x-lighttask-id"))
        e->acceptProposedAction();
}
void Calendar::dragMoveEvent(QDragMoveEvent *e) {
    if (previewOnly) {
        e->ignore();
        return;
    }
    if (e->mimeData()->hasFormat("application/x-lighttask-id") && dateAt(e->pos()).isValid())
        e->acceptProposedAction();
}
void Calendar::dropEvent(QDropEvent *e) {
    if (previewOnly) {
        e->ignore();
        return;
    }
    auto d = dateAt(e->pos());
    if (d.isValid() && e->mimeData()->hasFormat("application/x-lighttask-id")) {
        if (schedule)
            schedule(e->mimeData()->data("application/x-lighttask-id").toLongLong(), d);
        e->acceptProposedAction();
    }
}

Controller::Controller(Store &s, const QString &path, bool test)
    : store(s), settings(path, QSettings::IniFormat), testing(test) {
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
    switchButton->setText(calendarView ? "☷ 列表" : "▦ 日历");
    floating->findChild<QLabel *>("windowTitle")->setText(calendarView ? "Calendar" : "Tasks");
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
        p.setBrush(QColor("#557da9"));
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(1, 1, 30, 30, 8, 8);
        p.setPen(Qt::white);
        p.drawText(pix.rect(), Qt::AlignCenter, "✓");
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
void Controller::buildFloating() {
    floating = new Window(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint |
                          Qt::WindowTitleHint);
    floating->setObjectName("floatingWindow");
    floating->setWindowTitle("简序 · Tasks");
    floating->resize(420, 480);
    floating->customSurface = true;
    floating->setAttribute(Qt::WA_TranslucentBackground);
    floating->setMinimumWidth(390);
    floating->setMinimumHeight(340);
    auto grip = new QSizeGrip(floating);
    grip->setFixedSize(16, 16);
    grip->setToolTip("拖动调整悬浮窗大小");
    auto outer = new QVBoxLayout(floating);
    outer->setContentsMargins(12, 10, 12, 10);
    auto titleBar = new QWidget;
    auto head = new QHBoxLayout(titleBar);
    head->setContentsMargins(0, 0, 0, 0);
    auto label = new QLabel("Tasks");
    label->setObjectName("windowTitle");
    head->addWidget(label);
    head->addStretch();
    switchButton = button("▦ 日历");
    auto manage = button("管理");
    manage->setObjectName("openManager");
    manage->setToolTip("打开完整管理窗口");
    auto config = button("设置");
    head->addWidget(switchButton);
    head->addWidget(manage);
    head->addWidget(config);
    outer->addWidget(titleBar);
    floating->setDragHandle(titleBar);
    stack = new QStackedWidget;
    stack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    outer->addWidget(stack);
    auto listPage = new QWidget;
    auto lv = new QVBoxLayout(listPage);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(2);
    lv->setSizeConstraint(QLayout::SetMinAndMaxSize);
    lv->setAlignment(Qt::AlignTop);
    auto section = [&](const QString &s, TaskList *&list, QPushButton *&more) {
        auto h = new QHBoxLayout;
        h->addWidget(heading(s));
        h->addStretch();
        more = button("", "expandButton");
        more->setFlat(true);
        h->addWidget(more);
        lv->addLayout(h);
        list = new TaskList;
        bindList(list);
        lv->addWidget(list);
    };
    section("置顶", pins, pinMore);
    lv->addWidget(heading("Task"));
    tasks = new TaskList;
    tasks->setFixedHeight(5 * ROW_HEIGHT);
    bindList(tasks);
    lv->addWidget(tasks);
    section("计划", plans, planMore);
    lv->addWidget(quickBar("添加事项（默认无日期）", [this](QString t) { guard([&] { add(t); }); }));
    auto listScroll = new QScrollArea;
    listScroll->setObjectName("floatingScroll");
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setWidgetResizable(true);
    listScroll->setWidget(listPage);
    stack->addWidget(listScroll);
    auto calendarPage = new QWidget;
    auto cv = new QVBoxLayout(calendarPage);
    cv->setContentsMargins(0, 0, 0, 0);
    floatCal = new Calendar;
    cv->addWidget(floatCal);
    floatDay = new TaskList;
    floatDay->setFixedHeight(4 * ROW_HEIGHT);
    bindList(floatDay);
    cv->addWidget(floatDay);
    cv->addWidget(quickBar("添加到选中日期", [this](QString t) { guard([&] { add(t, day); }); }));
    stack->addWidget(calendarPage);

    connect(switchButton, &QPushButton::clicked, this, [this, label] {
        calendarView = !calendarView;
        stack->setCurrentIndex(calendarView ? 1 : 0);
        label->setText(calendarView ? "Calendar" : "Tasks");
        switchButton->setText(calendarView ? "☷ 列表" : "▦ 日历");
    });
    connect(manage, &QPushButton::clicked, this, [this] {
        manager->show();
        manager->raise();
        manager->activateWindow();
    });
    connect(config, &QPushButton::clicked, this, [this] { settingsDialog(); });

    connect(pinMore, &QPushButton::clicked, this, [this] {
        pinExpand = !pinExpand;
        updateLists();
    });
    connect(planMore, &QPushButton::clicked, this, [this] {
        planExpand = !planExpand;
        updateLists();
    });

    floatCal->schedule = [this](qint64 id, QDate d) { guard([&] { schedule(id, d); }); };
}
void Controller::setupFloatingScaling() {
    floatingBaseStyle = qApp->styleSheet();
    for (auto layout : floating->findChildren<QLayout *>())
        floatingLayouts.append({layout, layout->contentsMargins(), layout->spacing()});
    floating->setProperty("uiScale", 1.0);
    floating->onResize = [this] { scaleFloating(); };
    scaleFloating();
}
void Controller::scaleFloating(bool force) {
    if (scalingFloating)
        return;
    // The area-derived factor responds to either resize axis and applies uniformly
    // to text and geometry. Always scale from the original metrics, never cumulatively.
    const qreal factor =
        qBound(0.7,
               settings.value("fontSize", 15).toDouble() / 15.0 *
                   (settings.value("scaleWithWindow", true).toBool()
                        ? qSqrt(qreal(floating->width()) * floating->height() / (420.0 * 480.0))
                        : 1.0),
               3.0);
    if (!force && qAbs(factor - floatingScale) < 0.005)
        return;
    scalingFloating = true;
    const qreal previous = floatingScale;
    floatingScale = factor;
    floating->setProperty("uiScale", factor);
    QString style;
    int offset = 0;
    QRegularExpression pixels("([0-9]+(?:\\.[0-9]+)?)px");
    auto matches = pixels.globalMatch(floatingBaseStyle);
    while (matches.hasNext()) {
        auto match = matches.next();
        style += floatingBaseStyle.mid(offset, match.capturedStart() - offset);
        style += QString::number(qMax(1, qRound(match.captured(1).toDouble() * factor))) + "px";
        offset = match.capturedEnd();
    }
    style += floatingBaseStyle.mid(offset);
    const auto recolor = [&](QString text, const QRegularExpression &pattern, qreal alpha) {
        QString out;
        int position = 0;
        auto matches = pattern.globalMatch(text);
        while (matches.hasNext()) {
            auto match = matches.next();
            out += text.mid(position, match.capturedStart() - position);
            QColor color(match.captured(2));
            out += match.captured(1) + QString("rgba(%1,%2,%3,%4)")
                                           .arg(color.red())
                                           .arg(color.green())
                                           .arg(color.blue())
                                           .arg(qRound(alpha * 255));
            position = match.capturedEnd();
        }
        return out + text.mid(position);
    };
    style = recolor(
        style, QRegularExpression("((?:background|background-color)\\s*:\\s*)(#[a-fA-F0-9]{6}|white)\\b"),
        floating->backgroundOpacity);
    style = recolor(
        style, QRegularExpression("(?<![a-zA-Z-])((?:selection-)?color\\s*:\\s*)(#[a-fA-F0-9]{6}|white)\\b"),
        floating->property("foregroundOpacity").isValid() ? floating->property("foregroundOpacity").toDouble()
                                                          : 1.0);
    floating->setStyleSheet(style);
    auto px = [factor](int value) { return qRound(value * factor); };
    for (auto metrics : floatingLayouts) {
        auto m = metrics.margins;
        metrics.layout->setContentsMargins(px(m.left()), px(m.top()), px(m.right()), px(m.bottom()));
        if (metrics.spacing >= 0)
            metrics.layout->setSpacing(px(metrics.spacing));
    }
    const int row = scaled(floating, ROW_HEIGHT);
    tasks->setFixedHeight(settings.value("taskRows", 5).toInt() * row);
    floatDay->setFixedHeight(4 * row);
    for (auto list : {pins, plans})
        list->setFixedHeight(qMax(1, qMin(6, list->count())) * row);
    for (auto list : {pins, tasks, plans, floatDay}) {
        const int position = list->verticalScrollBar()->value();
        list->setMinimumWidth(px(260));
        list->verticalScrollBar()->setSingleStep(row);
        list->doItemsLayout();
        list->verticalScrollBar()->setValue(qRound(position * factor / previous));
        list->viewport()->update();
    }
    floatCal->setMinimumSize(px(330), px(250));
    floatCal->repaintCells();
    for (auto button : floatCal->findChildren<QToolButton *>())
        button->setIconSize(QSize(px(16), px(16)));
    if (auto grip = floating->findChild<QSizeGrip *>())
        grip->setFixedSize(px(16), px(16));
    floating->update();
    scalingFloating = false;
}
void Controller::buildManager() {
    manager = new Window;
    manager->setObjectName("managerWindow");
    manager->setWindowTitle("简序 · 完整管理窗口");
    manager->resize(1120, 880);
    manager->setMinimumSize(980, 730);
    auto root = new QVBoxLayout(manager);
    root->setContentsMargins(20, 16, 20, 16);
    auto header = new QHBoxLayout;
    auto label = new QLabel("完整管理窗口");
    label->setObjectName("windowTitle");
    header->addWidget(label);
    header->addStretch();
    auto showFloat = button("悬浮窗"), settingsButton = button("⚙ 设置"), exit = button("退出");
    header->addWidget(showFloat);
    header->addWidget(settingsButton);
    header->addWidget(exit);
    root->addLayout(header);
    auto split = new QSplitter;
    root->addWidget(split, 1);
    auto left = new QFrame;
    left->setObjectName("subpanel");
    auto lv = new QVBoxLayout(left);
    lv->addWidget(heading("全部事项 · 点击编辑，拖动改期"));
    search = new QLineEdit;
    search->setPlaceholderText("搜索事项…");
    lv->addWidget(search);
    showHistory = new QCheckBox("显示历史已完成事项");
    lv->addWidget(showHistory);
    connect(showHistory, &QCheckBox::toggled, this, [this] {
        items = store.all();
        for (auto &i : items)
            i = planState(i, QDate::currentDate());
        updateLists();
    });
    all = new TaskList;
    bindList(all);
    lv->addWidget(all, 1);
    lv->addWidget(quickBar("新建 Task（无日期）", [this](QString t) { guard([&] { add(t); }); }));
    split->addWidget(left);
    auto right = new QFrame;
    right->setObjectName("subpanel");
    auto rv = new QVBoxLayout(right);
    cal = new Calendar;
    cal->setMinimumHeight(250);
    rv->addWidget(cal);
    auto todayButton = button("今天");
    auto dh = new QHBoxLayout;
    dateLabel = new QLabel;
    dh->addWidget(dateLabel);
    dh->addStretch();
    auto dateAdd = button("+ 添加到此日期", "primary");
    dateAdd->setObjectName("addDateItem");
    dh->addWidget(todayButton);
    dh->addWidget(dateAdd);
    rv->addLayout(dh);
    dayItems = new TaskList;
    dayItems->setMinimumHeight(60);
    dayItems->setMaximumHeight(108);
    bindList(dayItems);
    rv->addWidget(dayItems);
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
    rv->addWidget(editorHeader);
    auto scroll = new EditorScrollArea;
    scroll->setWheelHeader(editorHeader);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    editor = new QWidget;
    editor->setObjectName("detail");
    scroll->setWidget(editor);
    rv->addWidget(scroll, 1);
    auto form = new QVBoxLayout(editor);
    form->setContentsMargins(12, 10, 12, 10);
    form->setSpacing(8);

    title = new QLineEdit;
    title->setMaxLength(200);
    date = new QLineEdit;
    date->installEventFilter(this);
    date->setPlaceholderText("YYYY-MM-DD（可清空）");
    time = new QLineEdit;
    time->setPlaceholderText("HH:mm（可清空）");
    group = new QComboBox;
    for (auto c : store.categories())
        group->addItem(c.name, c.id);
    type = new QComboBox(editor);
    type->hide();
    type->addItem("Task · 任务", "task");
    type->addItem("Plan · 计划", "plan");
    timeKind = new QComboBox(editor);
    timeKind->hide();
    timeKind->addItem("截止时间", "due");
    timeKind->addItem("开始时间", "start");
    form->addWidget(editorField("名称", title));
    planEditor = new PlanEditor;
    typeRuleRow = new EditorRow(editorField("类型", group), editorField("计划规则", planEditor->selector()));
    form->addWidget(typeRuleRow);
    taskDateBox = editorField("日期（可选）", date);
    taskTimeBox = editorField("时间（可选，仅记录）", time);
    auto taskDates = new EditorRow(taskDateBox, taskTimeBox);
    form->addWidget(taskDates);
    date->setClearButtonEnabled(true);
    time->setClearButtonEnabled(true);
    form->addWidget(planEditor);
    target = planEditor->totalTarget;
    // Completion belongs to the list. Keep the backing controls for compatibility.
    progress = new QSpinBox(editor);
    progress->setRange(0, 9999);
    progress->hide();
    notes = new QPlainTextEdit;
    notes->setMinimumHeight(64);
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
    auto save = button("保存修改", "primary"), cancel = button("放弃修改"), del = button("删除");
    saveButton = save;
    deleteButton = del;
    actions->addWidget(save);
    actions->addWidget(cancel);
    actions->addStretch();
    actions->addWidget(del);
    rv->addLayout(actions);
    split->addWidget(right);
    split->setSizes({370, 600});
    status = new QLabel("事项保存在本机 SQLite · 从左侧拖动事项到日历可改期");
    status->setObjectName("hint");
    root->addWidget(status);
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
    connect(todayButton, &QPushButton::clicked, this, [this] { setDate(QDate::currentDate()); });
    connect(dateAdd, &QPushButton::clicked, this, [this] {
        if (!leaveEditor())
            return;
        selected = 0;
        fillEditor();
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
        });
    });
    auto changed = [this] {
        if (!refreshing) {
            dirty = hasEditorChanges();
            status->setText(dirty ? "草稿暂存中 · 点击保存后写入事项" : "内容未改变");
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
    pinMore->setText(pinExpand ? "收起"
                               : "+" + QString::number(p.size() - settings.value("pinRows", 2).toInt()));
    planMore->setText(planExpand ? "收起"
                                 : "+" + QString::number(pl.size() - settings.value("planRows", 2).toInt()));
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
    dayItems->setFixedHeight(qMax(1, qMin(2, dayRows.size())) * ROW_HEIGHT);
    dateLabel->setText(day.toString("yyyy-MM-dd") + QString(" · %1 项").arg(dayRows.size()));
    for (auto list : {all, pins, tasks, plans, dayItems, floatDay}) {
        for (int n = 0; n < list->count(); ++n)
            if (list->item(n)->data(Qt::UserRole).toLongLong() == selected)
                list->setCurrentRow(n);
        list->verticalScrollBar()->setValue(positions.value(list));
    }
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
                    : hasEditorChanges() ? "草稿暂存中 · 点击保存后写入事项"
                                         : "已保存 · 本机 SQLite · 拖动事项到日历可改期");
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
    QDialog dialog(nullptr, Qt::Window | Qt::WindowCloseButtonHint);
    itemWindow = &dialog;
    dialog.setWindowModality(Qt::NonModal);
    dialog.setWindowTitle(settingsPreview ? "查看事项" : "编辑事项");
    dialog.resize(650, type->currentIndex() == 1 ? 680 : 500);
    dialog.setMinimumSize(500, 460);
    auto body = new QVBoxLayout(&dialog);
    body->addWidget(editorHeader);
    body->addWidget(scroll, 1);
    body->addLayout(actionRow);
    auto close = button("关闭");
    body->addWidget(close);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::close);
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
    };
    if (settingsPreview) {
        day = d;
        selectDay(d);
        updateLists();
        return;
    }
    if (editTaskDate) {
        day = d;
        selectDay(d);
        date->setText(d.toString(Qt::ISODate));
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
void Controller::add(const QString &t, QDate d) {
    if (settingsPreview)
        return;
    if (!leaveEditor())
        return;
    Item i;
    i.title = t;
    i.group = settings.value("defaultType", "other").toString();
    i.showCalendar = settings.value("planDefaultCalendar", false).toBool();
    i.date = d.isValid() ? d.toString(Qt::ISODate) : QString();
    selected = store.save(i).id;
    if (d.isValid())
        day = d;
    refresh();
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
    status->setText(enabled ? "设置预览中 · 可移动窗口、展开和浏览，事项暂不可编辑"
                            : "设置已关闭 · 可以继续编辑事项");
}
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
    require(tasks->sizeHintForRow(0) == 45 && tasks->height() == 225, "scaled row and viewport height");
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
            for (int y = 9; y < 19; ++y)
                for (int x = 278; x < 288; ++x) {
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
    manager->grab().save(dir + "/manager.png");
    setDate(QDate::currentDate().addDays(2));
    title->setText("在选中日期填写详细新事项");
    notes->setPlainText("填写类型、时间、备注后点击添加事项。");
    QApplication::processEvents();
    manager->grab().save(dir + "/new-task.png");
    dirty = false;
    fillEditor();
    editorSmokeTest(dir);
    draftSmokeTest(dir);
    calendarEditorSmokeTest();
    require(settingsSmokeTest(dir), "classified settings regression");
    return true;
}

void Window::setDragHandle(QWidget *handle) {
    handle->installEventFilter(this);
    for (auto label : handle->findChildren<QLabel *>())
        label->installEventFilter(this);
    handle->setCursor(Qt::SizeAllCursor);
}
bool Window::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            dragging = true;
            dragOffset = mouse->globalPos() - frameGeometry().topLeft();
            return true;
        }
    }
    if (event->type() == QEvent::MouseMove && dragging) {
        auto mouse = static_cast<QMouseEvent *>(event);
        if (mouse->buttons() & Qt::LeftButton) {
            move(mouse->globalPos() - dragOffset);
            return true;
        }
        dragging = false;
    }
    if (event->type() == QEvent::MouseButtonRelease && dragging) {
        dragging = false;
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
void Window::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    if (!customSurface)
        return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor border("#dce4ef"), background(Qt::white);
    border.setAlphaF(backgroundOpacity);
    background.setAlphaF(backgroundOpacity);
    p.setPen(QPen(border, 1));
    p.setBrush(background);
    int radius = windowFlags().testFlag(Qt::FramelessWindowHint) ? scaled(this, 12) : 0;
    p.drawRoundedRect(QRectF(rect()).adjusted(.5, .5, -.5, -.5), radius, radius);
}
void Window::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (onResize)
        onResize();
    if (auto grip = findChild<QSizeGrip *>()) {
        grip->move(width() - scaled(this, 17), height() - scaled(this, 17));
        grip->raise();
    }
}

void TaskList::wheelEvent(QWheelEvent *event) {
    auto bar = verticalScrollBar();
    if (bar->maximum() > 0) {
        int delta = event->pixelDelta().y();
        if (!delta)
            delta = event->angleDelta().y() * scaled(this, ROW_HEIGHT) / 40;
        bar->setValue(bar->value() - delta);
        event->accept();
        return;
    }
    QListWidget::wheelEvent(event);
}
void TaskList::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && currentItem() && currentItem()->data(Qt::UserRole).toLongLong()) {
        if (complete && !property("previewOnly").toBool())
            complete(currentItem()->data(Qt::UserRole).toLongLong());
        event->accept();
        return;
    }
    QListWidget::keyPressEvent(event);
}
QMenu *TaskList::createItemMenu(qint64 id) {
    QListWidgetItem *row = nullptr;
    for (int n = 0; n < count(); ++n)
        if (item(n)->data(Qt::UserRole).toLongLong() == id)
            row = item(n);
    if (!row)
        return nullptr;
    auto menu = new QMenu(this);
    auto pin = menu->addAction(row->data(Qt::UserRole + 5).toBool() ? "取消置顶" : "置顶");
    pin->setEnabled(!property("previewOnly").toBool());
    pin->setObjectName("togglePin");
    connect(pin, &QAction::triggered, this, [this, id] {
        if (togglePin)
            togglePin(id);
    });
    auto doneAction =
        menu->addAction(row->data(Qt::UserRole + 3).toBool() ? "撤销完成" : "✓ 完成 / 记录一次进度");
    doneAction->setEnabled(!property("previewOnly").toBool());
    connect(doneAction, &QAction::triggered, this, [this, id] {
        if (complete)
            complete(id);
    });
    auto editAction = menu->addAction(property("previewOnly").toBool() ? "查看事项" : "编辑事项");
    connect(editAction, &QAction::triggered, this, [this, id] {
        if (contextEdit)
            contextEdit(id);
        else if (edit)
            edit(id);
    });
    return menu;
}
void TaskList::setPreviewOnly(bool enabled) {
    setProperty("previewOnly", enabled);
    setDragEnabled(!enabled);
    viewport()->update();
}
void TaskList::contextMenuEvent(QContextMenuEvent *event) {
    auto row = event->reason() == QContextMenuEvent::Keyboard
                   ? currentItem()
                   : itemAt(viewport()->mapFrom(this, event->pos()));
    if (!row)
        return;
    auto menu = createItemMenu(row->data(Qt::UserRole).toLongLong());
    if (menu) {
        menu->exec(event->globalPos());
        delete menu;
    }
}

void Controller::reloadCategories() {
    categoryCache.clear();
    for (auto c : store.categories())
        categoryCache.insert(c.id, c);
    if (group) {
        QSignalBlocker b(group);
        auto id = group->currentData().toString();
        group->clear();
        for (auto c : store.categories())
            group->addItem(c.name, c.id);
        group->setCurrentIndex(qMax(0, group->findData(id)));
    }
}
void Controller::applyFloatingAppearance() {
    bool visible = floating->isVisible();
    floating->setWindowFlag(Qt::FramelessWindowHint, settings.value("frameless", true).toBool());
    floating->setWindowOpacity(1.0);
    floating->backgroundOpacity = qBound(0, settings.value("opacity", 100).toInt(), 100) / 100.0;
    floating->setProperty("surfaceOpacity", floating->backgroundOpacity);
    floating->setProperty("foregroundOpacity",
                          qBound(0, settings.value("textOpacity", 100).toInt(), 100) / 100.0);
    for (int weekday = 1; weekday <= 7; ++weekday) {
        QTextCharFormat format;
        format.setForeground(foregroundColor(floatCal, QColor(weekday >= 6 ? "#8995a5" : "#293448")));
        floatCal->setWeekdayTextFormat(Qt::DayOfWeek(weekday), format);
    }
    auto headerFormat = floatCal->headerTextFormat();
    headerFormat.setForeground(foregroundColor(floatCal, QColor("#293448")));
    floatCal->setHeaderTextFormat(headerFormat);
    if (auto grip = floating->findChild<QSizeGrip *>())
        grip->setVisible(settings.value("frameless", true).toBool());
    if (visible)
        floating->show();
    scaleFloating(true);
    floating->update();
    settings.sync();
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
                    dialog->resize(500, 460);
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
                    dialog->resize(500, 460);
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
    settings.setValue("textOpacity", 35);
    applyFloatingAppearance();
    require(qAbs(floating->backgroundOpacity - .2) < .01 &&
                qAbs(foregroundColor(tasks, QColor("#293448")).alphaF() - .35) < .01 &&
                floating->styleSheet() != backgroundStyle &&
                foregroundColor(all, QColor("#293448")).alpha() == 255,
            "text transparency changes independently of background and manager");
    require(qAbs(floatCal->weekdayTextFormat(Qt::Saturday).foreground().color().alphaF() - .35) < .01,
            "calendar weekend text follows foreground opacity");
    floating->grab().save(dir + "/floating-background-20-text-35.png");
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
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QApplication::sendEvent(date, &press);
    };
    auto clickDate = [&](Calendar *calendar, QDate target) {
        calendar->setCurrentPage(target.year(), target.month());
        auto table = calendar->findChild<QAbstractItemView *>();
        QApplication::processEvents();
        int firstNumber = table->model()->index(1, 1).data().toInt();
        QDate first(target.year(), target.month(), 1);
        if (firstNumber > 7)
            first = first.addMonths(-1);
        first = QDate(first.year(), first.month(), firstNumber);
        auto offset = first.daysTo(target);
        auto index = table->model()->index(1 + offset / 7, 1 + offset % 7);
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
            "explicit date-field click lets calendar edit deadline only in draft");
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
