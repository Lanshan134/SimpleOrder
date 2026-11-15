#pragma once
#include "domain/item.h"
#include "ui/theme/theme.h"
#include <QWidget>

// Floating text opacity floor (percent); also clamps older settings files.
inline constexpr int MIN_TEXT_OPACITY = 40;
// Floating window sizes at 100% text; Windows text size enlarges them so scaled text still fits.
inline QSize floatingSize(int width, int height) {
    return QSize(qRound(width * theme::textScale), qRound(height * theme::textScale));
}
QColor groupColor(const QString &group);
QString groupName(const QString &group);
inline constexpr int ROW_HEIGHT = 30;
qreal widgetScale(const QWidget *widget);
QColor surfaceColor(const QWidget *widget, QColor color, const char *key);
QColor foregroundColor(const QWidget *widget, QColor color);
int scaled(const QWidget *widget, int value);
void updateCategoryColors(const QList<Category> &categories);
