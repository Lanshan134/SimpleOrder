#include "ui/theme/appearance.h"
#include <QGraphicsEffect>
#include <QPainter>
#include <QtMath>

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
QColor floatingTone(const QWidget *widget, QColor color, const char *key) {
    bool light = false;
    // While the halo is on, secondary text uses the primary ink.
    for (auto current = widget; current; current = current->parentWidget())
        if (current->property("strongInk").isValid()) {
            if (current->property("strongInk").toBool() && QByteArray(key) == "foregroundOpacity" &&
                QStringList{"#5f6670", "#6b7280", "#4b5360"}.contains(color.name()))
                color = theme::color("ink");
            break;
        }
    for (auto current = widget; current; current = current->parentWidget())
        if (current->property("lightText").isValid()) {
            light = current->property("lightText").toBool();
            break;
        }
    if (!light)
        return color;
    // Light text sits on a dark surface; accents and type colours stay as they are.
    static const QHash<QString, QString> ink{
        {"#1f2328", "#f5f7fa"}, {"#5f6670", "#d3d8e0"}, {"#6b7280", "#c6ccd6"}, {"#4b5360", "#e4e8ee"},
        {"#a8aeb8", "#98a0ac"}, {"#aab0ba", "#8e96a3"}, {"#7e8591", "#b9c0ca"}, {"#1d4ed8", "#a9c5ff"},
        {"#c53030", "#ff9d9d"}, {"#c4c8cf", "#6d7480"}, {"#9ea4ae", "#8b93a0"}, {"#e6e8ec", "#3a404a"}};
    static const QHash<QString, QString> surface{
        {"#ffffff", "#1f2328"}, {"#e0eafd", "#2c4372"}, {"#e9ebef", "#353b45"}, {"#eceef1", "#353b45"},
        {"#e0e3e8", "#454c58"}, {"#f1f3f5", "#2b3038"}, {"#e6e8ec", "#3a404a"}, {"#c4c8cf", "#6d7480"}};
    const auto &table = QByteArray(key) == "surfaceOpacity" ? surface : ink;
    const auto mapped = table.value(color.name());
    return mapped.isEmpty() ? color : QColor(mapped);
}
namespace {
class HaloEffect : public QGraphicsEffect {
    const QWidget *window;

  public:
    explicit HaloEffect(const QWidget *w) : window(w) {}
    QRectF boundingRectFor(const QRectF &r) const override {
        return r.adjusted(-3, -3, 3, 3);
    }

  protected:
    void draw(QPainter *p) override {
        QPoint offset;
        const QPixmap source = sourcePixmap(Qt::LogicalCoordinates, &offset, PadToEffectiveBoundingRect);
        QImage tint = source.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
        // Faint fills (check interiors, the input, selection) are surface, not ink: no halo for them.
        const int floor = qBound(0, qRound(window->property("surfaceOpacity").toDouble() * 255) + 8, 200);
        for (int y = 0; y < tint.height(); ++y) {
            auto line = reinterpret_cast<QRgb *>(tint.scanLine(y));
            for (int x = 0; x < tint.width(); ++x)
                if (qAlpha(line[x]) < floor)
                    line[x] = 0;
        }
        {
            QPainter t(&tint);
            t.setCompositionMode(QPainter::CompositionMode_SourceIn);
            t.fillRect(QRect(QPoint(), source.size() / source.devicePixelRatio()),
                       floatingTone(window, theme::color("surface"), "surfaceOpacity"));
        }
        p->save();
        p->setRenderHint(QPainter::SmoothPixmapTransform);
        // A soft outer ring under a solid inner one.
        for (qreal ring : {2.0, 1.0}) {
            p->setOpacity(ring > 1 ? .22 : .8);
            for (int step = 0; step < 8; ++step) {
                const qreal angle = step * 3.14159265 / 4;
                p->drawImage(QPointF(offset) + QPointF(qCos(angle), qSin(angle)) * ring * widgetScale(window), tint);
            }
        }
        p->setOpacity(1);
        p->restore();
        p->drawPixmap(offset, source);
    }
};
} // namespace
QGraphicsEffect *haloEffect(const QWidget *window) {
    return new HaloEffect(window);
}
QColor surfaceColor(const QWidget *widget, QColor color, const char *key) {
    color = floatingTone(widget, color, key);
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
