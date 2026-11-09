#include "ui/widgets/widgettools.h"
#include "ui/widgets/calendar.h"
#include "ui/widgets/editorwidgets.h"
#include "ui/widgets/smartentry.h"
#include "ui/theme/appearance.h"

namespace {
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
// A button inside a line edit, drawn on a 16-unit grid scaled by the Windows text size.
class FieldButton : public QAbstractButton {
    bool calendar;

  public:
    FieldButton(bool isCalendar, const QString &label, QLineEdit *edit) : QAbstractButton(edit), calendar(isCalendar) {
        setObjectName(calendar ? "fieldCalendar" : "fieldClear");
        setAccessibleName(label);
        setToolTip(label);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
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
        if (hasFocus()) {
            p.setPen(QPen(theme::color("focus"), 1.5 * unit));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(rect()).adjusted(1, 1, -1, -1), 4 * unit, 4 * unit);
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
}

QPushButton *button(const QString &text, const QString &name) {
    auto b = new QPushButton(text);
    b->setAutoDefault(false);
    if (!name.isEmpty())
        b->setObjectName(name);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}
QLabel *heading(const QString &s, const QString &name) {
    auto l = new QLabel(s);
    l->setObjectName(name);
    l->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return l;
}
QPushButton *glyphButton(Glyph glyph, const QString &label, const QString &name) {
    return new GlyphButton(glyph, label, name);
}

QIcon typeDot(const QString &color) {
    return drawnIcon([&](QPainter &p) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(color));
        p.drawEllipse(QPointF(8, 8), 4, 4);
    });
}
QWidget *quickBar(const QString &placeholder, const std::function<bool(QString)> &add) {
    auto w = new QWidget;
    w->setObjectName("quickAddBar");
    w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto h = new QHBoxLayout(w);
    h->setContentsMargins(0, 6, 0, 0);
    h->setSpacing(6);
    auto input = new SmartEntry;
    input->setPlaceholderText(placeholder);
    input->onSubmit(add);
    input->setAccessibleName(placeholder);
    auto b = new GlyphButton(Glyph::Plus, "添加事项", "quickAdd");
    b->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    h->addWidget(input);
    h->addWidget(b);
    QObject::connect(b, &QPushButton::clicked, input, [input] { input->submitEntry(); });
    return w;
}

void arrangeFieldButtons(QWidget *root) {
    for (auto layout : root->findChildren<QObject *>("fieldButtonLayout"))
        static_cast<FieldButtonLayout *>(layout)->arrange();
}

void addClearButton(QLineEdit *edit) {
    auto clear = new FieldButton(false, "清空", edit);
    QObject::connect(clear, &QAbstractButton::clicked, edit, [edit] {
        edit->clear();
        edit->setFocus();
    });
    fieldLayout(edit)->arrange();
}
void attachDatePicker(QLineEdit *edit, std::function<void()> onDateSelected) {
    auto picker = new FieldButton(true, "选择日期", edit);
    fieldLayout(edit)->arrange();
    QObject::connect(picker, &QAbstractButton::clicked, edit, [edit, onDateSelected] {
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
        const auto pick = [edit, popup, onDateSelected](QDate d) {
            if (!popup->isVisible())
                return;
            edit->setText(d.toString(Qt::ISODate));
            if (onDateSelected)
                onDateSelected();
            popup->close();
        };
        QObject::connect(calendar, &QCalendarWidget::clicked, popup, pick);
        QObject::connect(calendar, &QCalendarWidget::activated, popup, pick);
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
