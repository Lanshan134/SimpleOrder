#include "ui/theme/appearance.h"

namespace {
QList<Category> categoryCache;
Category cachedCategory(const QString &id) {
    for (const auto &category : categoryCache)
        if (category.id == id)
            return category;
    return {id, id, "#aaaeb8"};
}
}

QColor groupColor(const QString &g) {
    return QColor(cachedCategory(g).color);
}
QString groupName(const QString &g) {
    return cachedCategory(g).name;
}

const QList<Category> &currentCategories() {
    return categoryCache;
}
void updateCategoryColors(const QList<Category> &categories) {
    categoryCache = categories;
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
