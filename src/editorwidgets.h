#pragma once
#include <QtWidgets>

// Two short fields share a row until the available width calls for stacking.
class EditorRow : public QWidget {
    QGridLayout *grid;
    QWidget *left, *right;
    bool stacked = false;
    bool secondVisible = true;
    void reflow() {
        bool narrow = width() < 480;
        grid->removeWidget(left);
        if (right)
            grid->removeWidget(right);
        grid->addWidget(left, 0, 0, 1, !right || !secondVisible || narrow ? 2 : 1);
        if (right)
            grid->addWidget(right, narrow ? 1 : 0, narrow ? 0 : 1, 1, narrow ? 2 : 1);
        stacked = narrow;
        grid->invalidate();
        grid->activate();
        setMinimumHeight(grid->minimumSize().height());
        updateGeometry();
    }

  protected:
    void showEvent(QShowEvent *event) override {
        QWidget::showEvent(event);
        reflow();
    }
    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (stacked != (width() < 480))
            reflow();
    }

  public:
    EditorRow(QWidget *a, QWidget *b, QWidget *parent = nullptr) : QWidget(parent), left(a), right(b) {
        grid = new QGridLayout(this);
        grid->setContentsMargins(0, 0, 0, 0);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(8);
        grid->setColumnStretch(0, 1);
        grid->setColumnStretch(1, 1);
        reflow();
    }
    void setSecondVisible(bool visible) {
        secondVisible = visible;
        if (right)
            right->setVisible(visible);
        reflow();
    }
};
inline QWidget *editorField(const QString &label, QWidget *input) {
    auto box = new QWidget;
    auto v = new QVBoxLayout(box);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(3);
    v->setAlignment(Qt::AlignTop);
    auto caption = new QLabel(label);
    caption->setObjectName("heading");
    v->addWidget(caption);
    v->addWidget(input);
    box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    box->setMinimumWidth(0);
    input->setMinimumWidth(0);
    return box;
}

// Child editors (including focused spins, combo popups and notes) never consume wheel input.
class EditorScrollArea : public QScrollArea {
    qreal remainder = 0;
    QPointer<QWidget> header;

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        auto origin = qobject_cast<QWidget *>(watched);
        bool belongs = false;
        // QObject ancestry includes a combo's separate popup window as well.
        if (event->type() == QEvent::Wheel && origin)
            for (auto owner = static_cast<QObject *>(origin); owner; owner = owner->parent())
                if (owner == widget() || (header && owner == header)) {
                    belongs = true;
                    break;
                }
        if (event->type() == QEvent::Wheel && belongs) {
            auto wheel = static_cast<QWheelEvent *>(event);
            qreal delta = wheel->pixelDelta().y();
            if (wheel->pixelDelta().isNull())
                delta = wheel->angleDelta().y() / 120.0 * verticalScrollBar()->singleStep() *
                        QApplication::wheelScrollLines();
            delta += remainder;
            int step = qRound(delta);
            remainder = delta - step;
            verticalScrollBar()->setValue(verticalScrollBar()->value() - step);
            wheel->accept();
            return true;
        }
        return QScrollArea::eventFilter(watched, event);
    }

  public:
    void setWheelHeader(QWidget *value) {
        header = value;
    }
    explicit EditorScrollArea(QWidget *parent = nullptr) : QScrollArea(parent) {
        qApp->installEventFilter(this);
        setWidgetResizable(true);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        verticalScrollBar()->setSingleStep(24);
    }
    ~EditorScrollArea() override {
        qApp->removeEventFilter(this);
    }
};
