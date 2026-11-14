#include "ui/theme/appearance.h"

namespace {
QHash<QString, Category> categoryCache;
}

QColor groupColor(const QString &g) {
    return QColor(categoryCache.value(g, {g, g, "#aaaeb8"}).color);
}
QString groupName(const QString &g) {
    return categoryCache.value(g, {g, g, "#aaaeb8"}).name;
}

void updateCategoryColors(const QList<Category> &categories) {
    categoryCache.clear();
    for (const auto &category : categories)
        categoryCache.insert(category.id, category);
}

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
