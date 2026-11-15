#pragma once
#include <QCloseEvent>
#include <QWidget>
#include <functional>

class Window : public QWidget {
    QPoint dragOffset;
    bool dragging = false;

  public:
    bool customSurface = false;
    qreal backgroundOpacity = 1.0;
    std::function<void()> onResize;
    void setDragHandle(QWidget *handle);
    std::function<void()> onClose;
    explicit Window(Qt::WindowFlags flags = Qt::Window) : QWidget(nullptr, flags) {}

  protected:
    bool eventFilter(QObject *, QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *e) override {
        if (onClose) {
            e->ignore();
            onClose();
        } else
            QWidget::closeEvent(e);
    }
};
