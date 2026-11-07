#include "ui/windows/window.h"
#include "ui/theme/appearance.h"
#include <QtWidgets>

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
