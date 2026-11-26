#pragma once
#include <QtWidgets>

// The friend QEvent names for tests; QApplication only passes window-system wheel input up to parent widgets.
class QSpontaneKeyEvent {
  public:
    static void mark(QEvent *event) {
        event->setSpontaneous();
    }
};
// QApplication::sendEvent clears the mark again, so the event goes straight to notify().
inline bool sendWheel(QWidget *target, QPoint local, int angle) {
    QWheelEvent event(QPointF(local), QPointF(target->mapToGlobal(local)), QPoint(), QPoint(0, angle),
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QSpontaneKeyEvent::mark(&event);
    qApp->notify(target, &event);
    QApplication::processEvents();
    return event.isAccepted();
}
