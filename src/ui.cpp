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
// Stacked floating lists keep a hidden scrollbar's width free so their check columns line up.
int checkInset(const QWidget *widget) {
    auto list = qobject_cast<const QAbstractScrollArea *>(widget);
    if (!list || !list->property("alignChecks").toBool() || list->verticalScrollBar()->isVisibleTo(list))
        return 0;
    return list->verticalScrollBar()->sizeHint().width();
}
class RowDelegate : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &idx) const override {
        return QSize(scaled(option.widget, 220), scaled(option.widget, ROW_HEIGHT));
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
        const int right = r.right() - qRound(checkInset(opt.widget) / scale);
        QString detail = idx.data(Qt::UserRole + 4).toString();
        if (!idx.data(Qt::UserRole).toLongLong()) {
            QFont head = baseFont;
            head.setPixelSize(13);
            head.setWeight(QFont::DemiBold);
            p->setFont(head);
            p->setPen(foregroundColor(opt.widget, theme::color("ink-muted")));
            const QRect area = r.adjusted(6, 6, 0, 0);
            p->drawText(area, Qt::AlignVCenter, idx.data().toString());
            p->drawText(QRect(area.left(), area.top(), right - area.left() - 6, area.height()),
                        Qt::AlignVCenter | Qt::AlignRight, detail);
            p->restore();
            return;
        }
        p->setPen(Qt::NoPen);
        if (opt.state & QStyle::State_Selected) {
            p->setBrush(surfaceColor(opt.widget, theme::color("selection"), "surfaceOpacity"));
            p->drawRoundedRect(r, 6, 6);
        } else if (opt.state & QStyle::State_MouseOver) {
            p->setBrush(surfaceColor(opt.widget, theme::color("row-hover"), "surfaceOpacity"));
            p->drawRoundedRect(r, 6, 6);
        }
        bool plan = idx.data(Qt::UserRole + 2).toBool(), done = idx.data(Qt::UserRole + 3).toBool();
        const QRectF check(r.left() + 6, r.center().y() - 9, 18, 18);
        const QColor ring = foregroundColor(opt.widget, theme::color(done ? "accent" : "control-border"));
        p->setPen(QPen(ring, 1.5));
        p->setBrush(done ? ring : surfaceColor(opt.widget, theme::color("surface"), "surfaceOpacity"));
        p->drawEllipse(check.adjusted(.75, .75, -.75, -.75));
        if (done) {
            const QPointF centre = check.center();
            const QPointF tick[] = {centre + QPointF(-4, 0), centre + QPointF(-1, 3), centre + QPointF(4, -3)};
            p->setPen(QPen(foregroundColor(opt.widget, Qt::white), 1.8, Qt::SolidLine, Qt::RoundCap,
                           Qt::RoundJoin));
            p->drawPolyline(tick, 3);
        }
        // Done rows fade the dot only; strikethrough and a lighter tone carry the state at readable contrast.
        const QColor c = foregroundColor(opt.widget, idx.data(Qt::UserRole + 1).value<QColor>());
        p->setOpacity(done ? 0.62 : 1.0);
        p->setPen(QPen(c, 1.5));
        p->setBrush(plan ? QBrush(Qt::NoBrush) : QBrush(c));
        p->drawEllipse(QPointF(right - 9, r.center().y()), 3.5, 3.5);
        p->setOpacity(1.0);
        QFont small = baseFont;
        small.setPixelSize(12);
        p->setFont(small);
        int dw = qMin(130, QFontMetrics(small).horizontalAdvance(detail));
        const int urgency = done ? 0 : idx.data(Qt::UserRole + 6).toInt();
        p->setPen(foregroundColor(opt.widget, theme::color(urgency == 2   ? "danger"
                                                           : urgency == 1 ? "accent-ink"
                                                                          : "ink-muted")));
        p->drawText(QRect(right - dw - 20, r.top(), dw, r.height()), Qt::AlignVCenter | Qt::AlignRight, detail);
        QFont main = baseFont;
        main.setStrikeOut(done);
        p->setFont(main);
        p->setPen(foregroundColor(opt.widget, theme::color(done ? "ink-muted" : "ink")));
        QRect text = r.adjusted(32, 0, right - r.right() - dw - 28, 0);
        p->drawText(
            text, Qt::AlignVCenter,
            QFontMetrics(main).elidedText(idx.data().toString(), Qt::ElideRight, qMax(0, text.width())));
        // QListWidget's own focus rect is disabled in style.qss; draw one that meets 3:1.
        if ((opt.state & QStyle::State_HasFocus) && opt.widget && opt.widget->hasFocus()) {
            p->setPen(QPen(theme::color("focus"), 2));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 5, 5);
        }
        p->restore();
    }
};
// Relative words for the days people scan for; urgency colours today (1) and overdue items (2).
QString dateMarker(const QString &iso, int *urgency) {
    const QDate date = QDate::fromString(iso, Qt::ISODate), today = QDate::currentDate();
    *urgency = !date.isValid() ? 0 : date < today ? 2 : date == today ? 1 : 0;
    if (!date.isValid())
        return iso.mid(5);
    if (date == today)
        return "今天";
    if (date == today.addDays(1))
        return "明天";
    if (date == today.addDays(-1))
        return "昨天";
    return date.year() == today.year() ? iso.mid(5) : iso;
}
QPushButton *button(const QString &text, const QString &name = {}) {
    auto b = new QPushButton(text);
    if (!name.isEmpty())
        b->setObjectName(name);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
QLabel *heading(const QString &s, const QString &name = "heading") {
    auto l = new QLabel(s);
    l->setObjectName(name);
    l->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return l;
}
// Drawn so they scale with the window and fade with the floating text like everything else.
enum class Glyph { Plus, Left, Right };
class GlyphButton : public QPushButton {
    Glyph glyph;

  public:
    GlyphButton(Glyph kind, const QString &label, const QString &name) : glyph(kind) {
        setObjectName(name);
        setAccessibleName(label);
        setToolTip(label);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    QSize sizeHint() const override {
        const int side = qRound((glyph == Glyph::Plus ? 32 : 26) * widgetScale(this));
        return QSize(side, side);
    }
    QSize minimumSizeHint() const override {
        return sizeHint();
    }

  protected:
    void paintEvent(QPaintEvent *event) override {
        QPushButton::paintEvent(event);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal unit = widgetScale(this);
        // The plus stays solid white so it remains legible on its faded fill.
        const QColor ink = !isEnabled() ? foregroundColor(this, theme::color("ink-disabled"))
                           : glyph == Glyph::Plus ? QColor(Qt::white)
                                                  : foregroundColor(this, theme::color("ink-control"));
        p.setPen(QPen(ink, 1.6 * unit, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF c = QRectF(rect()).center();
        if (glyph == Glyph::Plus) {
            p.drawLine(c + QPointF(-5 * unit, 0), c + QPointF(5 * unit, 0));
            p.drawLine(c + QPointF(0, -5 * unit), c + QPointF(0, 5 * unit));
        } else {
            const qreal side = glyph == Glyph::Left ? -2.5 * unit : 2.5 * unit;
            const QPointF chevron[] = {c + QPointF(-side, -5 * unit), c + QPointF(side, 0),
                                       c + QPointF(-side, 5 * unit)};
            p.drawPolyline(chevron, 3);
        }
    }
};
// Drawn on a 16-unit grid at the current text size.
QIcon drawnIcon(const std::function<void(QPainter &)> &draw) {
    const int side = qRound(32 * theme::textScale);
    QPixmap pixmap(side, side);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(2);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(theme::textScale, theme::textScale);
    p.setPen(QPen(theme::color("ink-muted"), 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    draw(p);
    return QIcon(pixmap);
}
QIcon typeDot(const QString &color) {
    return drawnIcon([&](QPainter &p) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(color));
        p.drawEllipse(QPointF(8, 8), 4, 4);
    });
}
// A button inside a line edit, drawn on a 16-unit grid scaled by the Windows text size.
class FieldButton : public QAbstractButton {
    bool calendar;

  public:
    FieldButton(bool isCalendar, const QString &label, QLineEdit *edit) : QAbstractButton(edit), calendar(isCalendar) {
        setObjectName(calendar ? "fieldCalendar" : "fieldClear");
        setAccessibleName(label);
        setToolTip(label);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
    }

  protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal unit = theme::textScale;
        if (isEnabled() && underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(theme::color("hover"));
            p.drawRoundedRect(QRectF(rect()), 4 * unit, 4 * unit);
        }
        p.translate(QRectF(rect()).center());
        p.scale(unit, unit);
        p.translate(-8, -8);
        const QColor ink = theme::color(!isEnabled() ? "ink-disabled" : underMouse() ? "ink" : "ink-muted");
        p.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        if (calendar) {
            p.drawRoundedRect(QRectF(2.5, 3.5, 11, 10), 2, 2);
            p.drawLine(QPointF(2.5, 6.5), QPointF(13.5, 6.5));
            p.drawLine(QPointF(5.5, 2), QPointF(5.5, 4.5));
            p.drawLine(QPointF(10.5, 2), QPointF(10.5, 4.5));
        } else {
            p.drawLine(QPointF(4.5, 4.5), QPointF(11.5, 11.5));
            p.drawLine(QPointF(11.5, 4.5), QPointF(4.5, 11.5));
        }
    }
};
// Keeps a line edit's field buttons at its right edge and its text clear of them.
class FieldButtonLayout : public QObject {
    QLineEdit *edit;

  public:
    explicit FieldButtonLayout(QLineEdit *field) : QObject(field), edit(field) {
        // Found by name: without Q_OBJECT, a findChild on this type would match any QObject.
        setObjectName("fieldButtonLayout");
        edit->installEventFilter(this);
        connect(edit, &QLineEdit::textChanged, this, [this] { arrange(); });
    }
    void arrange() {
        const int side = qRound(20 * theme::textScale), inset = qRound(4 * theme::textScale);
        int right = edit->width() - inset;
        int used = 0;
        for (auto name : {"fieldCalendar", "fieldClear"}) {
            auto button = edit->findChild<QAbstractButton *>(name, Qt::FindDirectChildrenOnly);
            if (!button)
                continue;
            const bool show = button->objectName() == "fieldCalendar" ||
                              (!edit->text().isEmpty() && !edit->isReadOnly() && edit->isEnabled());
            button->setVisible(show);
            if (!show)
                continue;
            right -= side;
            button->setGeometry(right, (edit->height() - side) / 2, side, side);
            used += side;
        }
        edit->setTextMargins(0, 0, used ? used + inset : 0, 0);
    }

  protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::EnabledChange)
            arrange();
        return false;
    }
};
FieldButtonLayout *fieldLayout(QLineEdit *edit) {
    auto layout = edit->findChild<QObject *>("fieldButtonLayout", Qt::FindDirectChildrenOnly);
    return layout ? static_cast<FieldButtonLayout *>(layout) : new FieldButtonLayout(edit);
}
QWidget *quickBar(const QString &placeholder, const std::function<void(QString)> &add) {
    auto w = new QWidget;
    w->setObjectName("quickAddBar");
    w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto h = new QHBoxLayout(w);
    h->setContentsMargins(0, 6, 0, 0);
    h->setSpacing(6);
    auto input = new QLineEdit;
    input->setMaxLength(200);
    input->setPlaceholderText(placeholder);
    input->setAccessibleName(placeholder);
    auto b = new GlyphButton(Glyph::Plus, "添加事项", "quickAdd");
    b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
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
void addClearButton(QLineEdit *edit) {
    auto clear = new FieldButton(false, "清空", edit);
    QObject::connect(clear, &QAbstractButton::clicked, edit, [edit] {
        edit->clear();
        edit->setFocus();
    });
    fieldLayout(edit)->arrange();
}
void attachDatePicker(QLineEdit *edit) {
    auto picker = new FieldButton(true, "选择日期", edit);
    fieldLayout(edit)->arrange();
    QObject::connect(picker, &QAbstractButton::clicked, edit, [edit] {
        auto popup = new QFrame(edit, Qt::Popup);
        popup->setObjectName("datePopup");
        popup->setAttribute(Qt::WA_DeleteOnClose);
        if (edit->window()->testAttribute(Qt::WA_DontShowOnScreen))
            popup->setAttribute(Qt::WA_DontShowOnScreen);
        auto layout = new QVBoxLayout(popup);
        layout->setContentsMargins(8, 8, 8, 8);
        auto calendar = new Calendar;
        layout->addWidget(calendar);
        const QDate current = QDate::fromString(edit->text().trimmed(), Qt::ISODate);
        calendar->setSelectedDate(current.isValid() ? current : QDate::currentDate());
        const auto pick = [edit, popup](QDate d) {
            edit->setText(d.toString(Qt::ISODate));
            popup->close();
        };
        QObject::connect(calendar, &QCalendarWidget::clicked, popup, pick);
        QObject::connect(calendar->todayButton, &QPushButton::clicked, popup,
                         [pick] { pick(QDate::currentDate()); });
        popup->adjustSize();
        const QRect screen = edit->screen()->availableGeometry();
        QPoint at = edit->mapToGlobal(QPoint(0, edit->height() + 4));
        if (at.y() + popup->height() > screen.bottom())
            at.setY(edit->mapToGlobal(QPoint()).y() - popup->height() - 4);
        at.setX(qMin(at.x(), screen.right() - popup->width()));
        popup->move(at);
        popup->show();
        calendar->setFocus();
    });
}
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
    setAccessibleName("事项列表");
    setAccessibleDescription("回车编辑，空格完成或记录一次进度，菜单键打开更多操作");
    connect(this, &QListWidget::itemClicked, this, [this](QListWidgetItem *i) {
        if (pressedButton != Qt::LeftButton)
            return;
        auto id = i->data(Qt::UserRole).toLongLong();
        if (!id)
            return;
        const QPoint p = viewport()->mapFromGlobal(QCursor::pos());
        if (p.x() < scaled(this, 30) && !property("previewOnly").toBool()) {
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
    const auto sectionOf = [](const Item &i) { return i.done ? 3 : i.pinned ? 0 : i.type == "task" ? 1 : 2; };
    QHash<int, int> sectionCounts;
    if (headings)
        for (const auto &i : items)
            ++sectionCounts[sectionOf(i)];
    for (const auto &i : items) {
        int section = sectionOf(i);
        if (headings && section != last) {
            auto h = new QListWidgetItem(QStringList{"置顶", "任务", "计划", "已完成"}[section], this);
            h->setData(Qt::AccessibleTextRole,
                       QString("分组：%1，%2 项").arg(h->text()).arg(sectionCounts.value(section)));
            h->setData(Qt::UserRole + 4, QString::number(sectionCounts.value(section)));
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
        int urgency = 0;
        if (i.type == "plan") {
            if (counts)
                marker = QString("%1/%2").arg(i.progress).arg(i.target);
        } else {
            // A list for one chosen day already says which day it is.
            if (dates && !i.date.isEmpty() && !property("dayList").toBool())
                marker = dateMarker(i.date, &urgency);
            if (times && !i.time.isEmpty())
                marker += (marker.isEmpty() ? QString() : " ") + i.time;
        }
        row->setData(Qt::UserRole + 4, marker);
        row->setData(Qt::UserRole + 6, urgency);
        // The checkbox, dot and marker are painted; spell out their state for assistive technology.
        QStringList spoken{i.title,
                           i.type == "plan" ? QString("计划，进度 %1/%2%3")
                                                  .arg(i.progress)
                                                  .arg(i.target)
                                                  .arg(i.done ? "，已达标" : "")
                                            : QString(i.done ? "任务，已完成" : "任务，未完成")};
        if (i.pinned)
            spoken << "置顶";
        if (i.type != "plan" && !i.date.isEmpty())
            spoken << i.date + (i.time.isEmpty() ? QString() : " " + i.time);
        spoken << groupName(i.group);
        row->setData(Qt::AccessibleTextRole, spoken.join("，"));
        row->setToolTip(i.title + "\n" + groupName(i.group) + " · " + (i.date.isEmpty() ? "未安排" : i.date) +
                        (i.time.isEmpty() ? QString() : " " + i.time) + "\n" +
                        (i.type == "plan" ? "点击圆圈记录进度；达标后完成" : "点击圆圈切换完成") +
                        "\n拖到日历可改期");
    }
    viewport()->update();
}
// A list with preferred rows asks for that height, then takes spare room or gives it up as the window requires.
QSize TaskList::sizeHint() const {
    const QSize base = QListWidget::sizeHint();
    const int rows = property("preferredRows").toInt();
    return rows ? QSize(base.width(), rows * scaled(this, ROW_HEIGHT)) : base;
}
// A list that stretches shows whole rows only, so no sliver of the next row sits against what follows.
void TaskList::snapToRows() {
    if (!property("preferredRows").toInt())
        return;
    const int row = scaled(this, ROW_HEIGHT);
    setViewportMargins(0, 0, 0, height() >= row ? height() % row : 0);
}
void TaskList::resizeEvent(QResizeEvent *event) {
    snapToRows();
    QListWidget::resizeEvent(event);
}
void TaskList::paintEvent(QPaintEvent *event) {
    QListWidget::paintEvent(event);
    const QString empty = property("emptyText").toString();
    if (count() || empty.isEmpty())
        return;
    QPainter p(viewport());
    p.setPen(foregroundColor(this, theme::color("ink-muted")));
    p.drawText(viewport()->rect(), Qt::AlignCenter, empty);
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
    setHorizontalHeaderFormat(QCalendarWidget::ShortDayNames);
    setGridVisible(false);
    // One plain title with its controls beside it, instead of Qt's split month and year buttons.
    setNavigationBarVisible(false);
    setAcceptDrops(true);
    setMinimumSize(330, 250);
    auto table = findChild<QAbstractItemView *>();
    if (table) {
        table->setAcceptDrops(false);
        table->viewport()->setAcceptDrops(false);
        table->viewport()->installEventFilter(this);
        table->viewport()->setMouseTracking(true);
    }
    auto bar = new QWidget(this);
    auto barLayout = new QHBoxLayout(bar);
    barLayout->setContentsMargins(0, 0, 0, 4);
    barLayout->setSpacing(2);
    titleButton = button("", "calendarTitle");
    titleButton->setToolTip("选择年份和月份");
    todayButton = button("今天");
    auto previous = new GlyphButton(Glyph::Left, "上个月", "calendarArrow");
    auto next = new GlyphButton(Glyph::Right, "下个月", "calendarArrow");
    barLayout->addWidget(titleButton);
    barLayout->addStretch();
    barLayout->addWidget(todayButton);
    barLayout->addWidget(previous);
    barLayout->addWidget(next);
    if (auto column = qobject_cast<QVBoxLayout *>(layout()))
        column->insertWidget(0, bar);
    connect(titleButton, &QPushButton::clicked, this, [this] { chooseMonth(); });
    connect(todayButton, &QPushButton::clicked, this, [this] { setSelectedDate(QDate::currentDate()); });
    connect(previous, &QPushButton::clicked, this, &QCalendarWidget::showPreviousMonth);
    connect(next, &QPushButton::clicked, this, &QCalendarWidget::showNextMonth);
    connect(this, &QCalendarWidget::currentPageChanged, this, [this] { updateHeader(); });
    connect(this, &QCalendarWidget::selectionChanged, this, [this] { updateHeader(); });
    updateHeader();
    QTextCharFormat weekend;
    weekend.setForeground(theme::color("ink-soft"));
    setWeekdayTextFormat(Qt::Saturday, weekend);
    setWeekdayTextFormat(Qt::Sunday, weekend);
    QTextCharFormat header;
    header.setForeground(theme::color("ink-muted"));
    header.setBackground(Qt::transparent);
    header.setFontWeight(QFont::Normal);
    setHeaderTextFormat(header);
}
void Calendar::paintCell(QPainter *p, const QRect &r, const QDate &d) const {
    const bool outside = d.month() != monthShown();
    if (!displayOutside && outside)
        return;
    const qreal scale = widgetScale(this);
    auto table = findChild<QAbstractItemView *>();
    const bool isToday = d == QDate::currentDate(), isSelected = d == selectedDate();
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const QRectF cell = QRectF(r).adjusted(2 * scale, 2 * scale, -2 * scale, -2 * scale);
    if (isSelected || d == dropDate || (d == hoverDate && table)) {
        p->setPen(Qt::NoPen);
        p->setBrush(surfaceColor(this, theme::color(isSelected || d == dropDate ? "selection" : "row-hover"),
                                 "surfaceOpacity"));
        p->drawRoundedRect(cell, 6 * scale, 6 * scale);
    }
    // The view's outline is off in style.qss; mark the keyboard position and the drop target.
    if ((isSelected && table && table->hasFocus()) || d == dropDate) {
        p->setPen(QPen(theme::color("focus"), 2 * scale));
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(QRectF(r).adjusted(scale, scale, -scale, -scale), 6 * scale, 6 * scale);
    }
    QFont numberFont = table ? table->font() : font();
    numberFont.setWeight(isToday ? QFont::DemiBold : QFont::Normal);
    p->setFont(numberFont);
    const QFontMetricsF metrics(numberFont);
    // The circle hugs the digits, so it stays clear of the marks below at every text size.
    // A calendar that never shows marks, like the date picker, centres its numbers instead of leaving room.
    const bool hasMarks = !items.isEmpty();
    const qreal radius =
        hasMarks ? qMin(metrics.capHeight() * .95, (r.height() - 11 * scale) / 2) : metrics.capHeight() * .95;
    const QPointF numberCentre(r.center().x() + .5,
                               hasMarks ? r.top() + qMax(radius + 3 * scale, r.height() * .38) : QRectF(r).center().y());
    if (isToday) {
        p->setPen(Qt::NoPen);
        p->setBrush(foregroundColor(this, theme::color("accent")));
        p->drawEllipse(numberCentre, radius, radius);
    }
    const auto weekdayInk = weekdayTextFormat(Qt::DayOfWeek(d.dayOfWeek())).foreground();
    p->setPen(foregroundColor(this, isToday      ? QColor(Qt::white)
                                    : outside    ? theme::color("ink-outside")
                                    : isSelected ? theme::color("ink")
                                    : weekdayInk.style() != Qt::NoBrush ? weekdayInk.color()
                                                                        : theme::color("ink")));
    p->drawText(QRectF(r.left(), numberCentre.y() - metrics.height() / 2, r.width(), metrics.height()),
                Qt::AlignCenter, QString::number(d.day()));
    p->restore();
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
        const QColor mark = foregroundColor(this, groupColor(i.group));
        p->setPen(QPen(mark, 1));
        p->setBrush(i.type == "plan" ? QBrush(Qt::NoBrush) : QBrush(mark));
        p->drawEllipse(QPointF(left + n * 8 + 3, area.bottom() - 4), 2.5, 2.5);
    }
    if (hits.size() > maxDots) {
        QFont font = p->font();
        font.setPointSize(7);
        p->setFont(font);
        p->setPen(foregroundColor(this, theme::color("ink-muted")));
        p->drawText(QRect(left + shown * 8, area.bottom() - 14, 25, 14), Qt::AlignVCenter,
                    "+" + QString::number(hits.size() - shown));
    }
    p->restore();
}
void Calendar::updateHeader() {
    if (!titleButton)
        return;
    titleButton->setText(QString("%1 年 %2 月").arg(yearShown()).arg(monthShown()));
    const QDate today = QDate::currentDate();
    todayButton->setEnabled(selectedDate() != today || yearShown() != today.year() ||
                            monthShown() != today.month());
}
void Calendar::chooseMonth() {
    QMenu menu(this);
    menu.addAction(QString("上一年（%1）").arg(yearShown() - 1),
                   [this] { setCurrentPage(yearShown() - 1, monthShown()); });
    menu.addAction(QString("下一年（%1）").arg(yearShown() + 1),
                   [this] { setCurrentPage(yearShown() + 1, monthShown()); });
    menu.addSeparator();
    for (int month = 1; month <= 12; ++month) {
        auto action = menu.addAction(QString("%1 月").arg(month),
                                     [this, month] { setCurrentPage(yearShown(), month); });
        action->setCheckable(true);
        action->setChecked(month == monthShown());
    }
    menu.exec(titleButton->mapToGlobal(QPoint(0, titleButton->height())));
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
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::Leave) {
        QDate under;
        if (auto viewport = qobject_cast<QWidget *>(watched); viewport && event->type() == QEvent::MouseMove)
            under = dateAt(viewport->mapTo(this, static_cast<QMouseEvent *>(event)->pos()));
        if (under != hoverDate) {
            hoverDate = under;
            updateCells();
        }
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
    const QDate target = e->mimeData()->hasFormat("application/x-lighttask-id") ? dateAt(e->pos()) : QDate();
    if (target != dropDate) {
        dropDate = target;
        updateCells();
    }
    if (target.isValid())
        e->acceptProposedAction();
    else
        e->ignore();
}
void Calendar::dragLeaveEvent(QDragLeaveEvent *) {
    dropDate = QDate();
    updateCells();
}
void Calendar::dropEvent(QDropEvent *e) {
    dropDate = QDate();
    updateCells();
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
void Controller::buildFloating() {
    floating = new Window(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint |
                          Qt::WindowTitleHint);
    floating->setObjectName("floatingWindow");
    floating->setWindowTitle("简序 · 悬浮窗");
    floating->resize(floatingSize(420, 480));
    floating->customSurface = true;
    floating->setAttribute(Qt::WA_TranslucentBackground);
    floating->setMinimumSize(floatingSize(390, 340));
    auto grip = new QSizeGrip(floating);
    grip->setFixedSize(16, 16);
    grip->setToolTip("拖动调整悬浮窗大小");
    auto outer = new QVBoxLayout(floating);
    outer->setContentsMargins(12, 10, 12, 10);
    auto titleBar = new QWidget;
    auto head = new QHBoxLayout(titleBar);
    head->setContentsMargins(0, 0, 0, 0);
    auto label = new QLabel("任务");
    label->setObjectName("windowTitle");
    head->addWidget(label);
    floatDate = new QLabel;
    floatDate->setObjectName("floatingDate");
    head->addWidget(floatDate);
    head->addStretch();
    switchButton = button("日历");
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
    lv->setSpacing(0);
    auto section = [&](const QString &s, QLabel *&title, TaskList *&list, QPushButton *&more) {
        auto h = new QHBoxLayout;
        title = heading(s, "sectionHeading");
        h->addWidget(title);
        h->addStretch();
        more = button("", "expandButton");
        more->setFlat(true);
        h->addWidget(more);
        lv->addLayout(h);
        list = new TaskList;
        list->setAccessibleName(s + "事项");
        list->setProperty("alignChecks", true);
        bindList(list);
        lv->addWidget(list);
    };
    section("置顶", pinHeading, pins, pinMore);
    taskHeading = heading("任务", "sectionHeading");
    lv->addWidget(taskHeading);
    tasks = new TaskList;
    tasks->setAccessibleName("任务");
    tasks->setProperty("alignChecks", true);
    tasks->setProperty("emptyText", "没有任务");
    // The configured rows when they fit, more when the window has room, and fewer when it is short,
    // so the page itself rarely scrolls and no band sits empty above quick add.
    tasks->setMinimumHeight(ROW_HEIGHT);
    tasks->setProperty("preferredRows", 5);
    bindList(tasks);
    lv->addWidget(tasks, 1);
    section("计划", planHeading, plans, planMore);
    auto listScroll = new QScrollArea;
    listScroll->setObjectName("floatingScroll");
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setWidgetResizable(true);
    listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listScroll->setWidget(listPage);
    // Quick add stays anchored at the bottom of the window, however long the lists are.
    auto listHost = new QWidget;
    auto hostLayout = new QVBoxLayout(listHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);
    hostLayout->addWidget(listScroll, 1);
    hostLayout->addWidget(quickBar("添加事项", [this](QString t) { guard([&] { add(t); }); }));
    stack->addWidget(listHost);
    auto calendarPage = new QWidget;
    auto cv = new QVBoxLayout(calendarPage);
    cv->setContentsMargins(0, 0, 0, 0);
    floatCal = new Calendar;
    cv->addWidget(floatCal);
    floatDay = new TaskList;
    floatDay->setAccessibleName("选中日期的事项");
    floatDay->setProperty("dayList", true);
    floatDay->setProperty("emptyText", "这一天没有事项");
    floatDay->setFixedHeight(4 * ROW_HEIGHT);
    bindList(floatDay);
    cv->addWidget(floatDay);
    cv->addWidget(quickBar("添加到选中日期", [this](QString t) { guard([&] { add(t, day); }); }));
    stack->addWidget(calendarPage);

    connect(switchButton, &QPushButton::clicked, this, [this, label] {
        calendarView = !calendarView;
        stack->setCurrentIndex(calendarView ? 1 : 0);
        label->setText(calendarView ? "日历" : "任务");
        switchButton->setText(calendarView ? "列表" : "日历");
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
    connect(floatCal, &QCalendarWidget::selectionChanged, this,
            [this] { setDate(floatCal->selectedDate()); });
    floatCal->schedule = [this](qint64 id, QDate d) { guard([&] { schedule(id, d); }); };
    // Start focus in the list rather than ringing the first button in the window.
    tasks->setFocus();
}
void Controller::setupFloatingScaling() {
    // Text size is folded into the floating factor, so start from the unscaled sheet.
    floatingBaseStyle = theme::baseStyle;
    floatingBaseStyle.replace(QRegularExpression("([0-9]+)tx"), "\\1px");
    for (auto layout : floating->findChildren<QLayout *>())
        floatingLayouts.append({layout, layout->contentsMargins(), layout->spacing()});
    floating->setProperty("uiScale", 1.0);
    // Restyling on every resize step makes drags stutter; coalesce them. Tests stay synchronous.
    auto rescale = new QTimer(this);
    rescale->setSingleShot(true);
    rescale->setInterval(40);
    connect(rescale, &QTimer::timeout, this, [this] { scaleFloating(); });
    floating->onResize = [this, rescale] {
        if (testing)
            scaleFloating();
        else
            rescale->start();
    };
    scaleFloating();
}
void Controller::scaleFloating(bool force) {
    if (scalingFloating)
        return;
    // The area-derived factor responds to either resize axis and applies uniformly
    // to text and geometry. Always scale from the original metrics, never cumulatively.
    // Windows text size enlarges the default window, so with window scaling the size carries it.
    const qreal font = settings.value("fontSize", 15).toDouble() / 15.0;
    const qreal factor =
        qBound(0.7,
               settings.value("scaleWithWindow", true).toBool()
                   ? font * qSqrt(qreal(floating->width()) * floating->height() / (420.0 * 480.0))
                   : font * theme::textScale,
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
    // Control borders fade with the text. Quick add is the window's main action: its fill ignores the
    // see-through background and stops fading at 70%, so it never reads as disabled.
    const qreal textAlpha = floating->property("foregroundOpacity").isValid()
                                ? floating->property("foregroundOpacity").toDouble()
                                : 1.0;
    style = recolor(style, QRegularExpression("(border(?:-[a-z]+)?\\s*:\\s*[^;{}#]*)(#[a-fA-F0-9]{6})\\b"),
                    textAlpha);
    const auto fill = [textAlpha](const char *token) {
        const QColor color = theme::color(token);
        return QString("rgba(%1,%2,%3,%4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(qRound(qMax(textAlpha, 0.7) * 255));
    };
    style += QString("QPushButton#quickAdd{background:%1;}QPushButton#quickAdd:hover{background:%2;}"
                     "QPushButton#quickAdd:disabled{background:%3;}")
                 .arg(fill("accent"), fill("accent-hover"), fill("control-off"));
    floating->setStyleSheet(style);
    // Polishing resets each input's palette, so the placeholder's opacity goes on afterwards.
    for (auto input : floating->findChildren<QLineEdit *>()) {
        auto palette = input->palette();
        palette.setColor(QPalette::PlaceholderText, foregroundColor(floating, theme::color("ink-soft")));
        input->setPalette(palette);
    }
    auto px = [factor](int value) { return qRound(value * factor); };
    for (auto metrics : floatingLayouts) {
        auto m = metrics.margins;
        metrics.layout->setContentsMargins(px(m.left()), px(m.top()), px(m.right()), px(m.bottom()));
        if (metrics.spacing >= 0)
            metrics.layout->setSpacing(px(metrics.spacing));
    }
    const int row = scaled(floating, ROW_HEIGHT);
    tasks->setMinimumHeight(row);
    tasks->setProperty("preferredRows", settings.value("taskRows", 5).toInt());
    tasks->updateGeometry();
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
    if (auto grip = floating->findChild<QSizeGrip *>())
        grip->setFixedSize(px(16), px(16));
    tasks->snapToRows();
    floating->update();
    scalingFloating = false;
}
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
    lv->addWidget(quickBar("添加事项", [this](QString t) { guard([&] { add(t); }); }));
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
    title->setMaxLength(200);
    date = new QLineEdit;
    date->setPlaceholderText("如 2026-11-05，可留空");
    time = new QLineEdit;
    time->setPlaceholderText("如 14:30，可留空");
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
    taskDateBox = editorField("日期（可选）", date);
    taskTimeBox = editorField("时间（可选，仅记录）", time);
    auto taskDates = new EditorRow(taskDateBox, taskTimeBox);
    form->addWidget(taskDates);
    addClearButton(date);
    addClearButton(time);
    attachDatePicker(date);
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
    connect(cal, &QCalendarWidget::selectionChanged, this, [this] { setDate(cal->selectedDate()); });
    cal->schedule = [this](qint64 id, QDate d) { guard([&] { schedule(id, d); }); };
    connect(dateAdd, &QPushButton::clicked, this, [this] {
        if (!leaveEditor())
            return;
        selected = 0;
        fillEditor();
        openEditor();
        title->setFocus();
    });
    connect(cal, &QCalendarWidget::clicked, this, [this](QDate d) {
        if (selected)
            setDate(d);
    });
    connect(floatCal, &QCalendarWidget::clicked, this, [this](QDate d) {
        if (selected)
            setDate(d);
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
    for (auto fields : manager->findChildren<QObject *>("fieldButtonLayout"))
        static_cast<FieldButtonLayout *>(fields)->arrange();
}
void Controller::openEditor() {
    if (itemWindow || inspector->isVisibleTo(manager))
        return;
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
    return editorContents() != editorBaseline;
}
void Controller::fillEditor() {
    refreshing = true;
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
                    : hasEditorChanges() ? "有未保存的草稿"
                                         : "已保存在本机");
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
void Controller::setDate(QDate d) {
    if (refreshing || !d.isValid() || day == d)
        return;
    const bool emptyDraft =
        !selected && title->text().trimmed().isEmpty() && notes->toPlainText().isEmpty() && !dirty;
    day = d;
    {
        QSignalBlocker a(cal), b(floatCal);
        cal->setSelectedDate(d);
        floatCal->setSelectedDate(d);
    }
    // The blockers also silence the page-change signal their month titles rely on.
    cal->repaintCells();
    floatCal->repaintCells();
    // A calendar click changes a task's date in the current form, never a plan's bounds.
    // With the editor closed, an existing item keeps its date and the click only browses.
    const bool editing = !selected || itemWindow || inspector->isVisibleTo(manager);
    if (!settingsPreview && editing && type->currentData().toString() == "task") {
        date->setText(d.toString(Qt::ISODate));
        if (emptyDraft) {
            editorBaseline = editorContents();
            dirty = false;
        }
    }
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
    choose(draftedId);
    require(inspector->isVisibleTo(manager) && title->text() == "关闭面板保留草稿 未保存",
            "reopening restores the draft");
    fillEditor();
    closeEditor();
    store.remove(draftedId);
    refresh();
    editorSmokeTest(dir);
    draftSmokeTest(dir);
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
    QColor border = theme::color("line"), background = theme::color("surface");
    border.setAlphaF(backgroundOpacity);
    background.setAlphaF(backgroundOpacity);
    p.setPen(QPen(border, 1));
    p.setBrush(background);
    int radius = windowFlags().testFlag(Qt::FramelessWindowHint) ? scaled(this, 10) : 0;
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
        menu->addAction(row->data(Qt::UserRole + 3).toBool() ? "撤销完成" : "完成 / 记录一次进度");
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
            group->addItem(typeDot(c.color), c.name, c.id);
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
                          qBound(MIN_TEXT_OPACITY, settings.value("textOpacity", 100).toInt(), 100) / 100.0);
    // Only weekends carry a format, as on the manager calendar, so weekday headers keep the muted header ink.
    for (int weekday = 6; weekday <= 7; ++weekday) {
        QTextCharFormat format;
        format.setForeground(foregroundColor(floatCal, theme::color("ink-soft")));
        floatCal->setWeekdayTextFormat(Qt::DayOfWeek(weekday), format);
    }
    auto headerFormat = floatCal->headerTextFormat();
    headerFormat.setForeground(foregroundColor(floatCal, theme::color("ink-muted")));
    floatCal->setHeaderTextFormat(headerFormat);
    for (auto input : floating->findChildren<QLineEdit *>()) {
        auto palette = input->palette();
        palette.setColor(QPalette::PlaceholderText, foregroundColor(floating, theme::color("ink-soft")));
        input->setPalette(palette);
    }
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
    QLineEdit *quickInput = nullptr;
    for (auto bar : stack->currentWidget()->findChildren<QWidget *>("quickAddBar"))
        quickInput = bar->findChild<QLineEdit *>();
    require(quickInput && quickInput->text().isEmpty() && quickInput->isVisible(), "placeholder is on screen");
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
