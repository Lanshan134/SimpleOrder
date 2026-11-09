#include "ui/widgets/tasklist.h"
#include "ui/theme/appearance.h"
#include <QtWidgets>

namespace {
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
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override {
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
