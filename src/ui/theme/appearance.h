#pragma once
#include "domain/item.h"
#include "ui/theme/theme.h"
#include <QWidget>

// Floating text opacity floor (percent); also clamps older settings files.
inline constexpr int MIN_TEXT_OPACITY = 40;
// Below this floating background opacity the optional text halo switches on.
inline constexpr qreal HALO_BELOW = 0.85;
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
// Light-text counterpart of a palette colour when the floating window uses light text.
QColor floatingTone(const QWidget *widget, QColor color, const char *key);
class QGraphicsEffect;
// Outlines everything drawn in an area so it stays legible over any desktop; set it on that area.
QGraphicsEffect *haloEffect(const QWidget *window, QWidget *area);
int scaled(const QWidget *widget, int value);
// The categories in their settings order, as last loaded.
const QList<Category> &currentCategories();
void updateCategoryColors(const QList<Category> &categories);
