#pragma once
#include <QColor>
#include <QHash>
#include <QRegularExpression>
#include <QSettings>
#include <QString>

// One palette for style.qss (as @name), the settings stylesheet and the painted widgets.
// Text roles are chosen for 4.5:1 on white, control borders and focus for 3:1.
namespace theme {
inline const QHash<QString, QString> &tokens() {
    static const QHash<QString, QString> values{
        {"ink", "#1f2328"},          // primary text
        {"ink-muted", "#5f6670"},    // secondary text, headings, details
        {"ink-soft", "#6b7280"},     // placeholders, weekends
        {"ink-control", "#4b5360"},  // text buttons
        {"ink-disabled", "#a8aeb8"}, // disabled text (exempt from contrast)
        {"ink-outside", "#aab0ba"},  // other months' calendar dates (exempt from contrast)
        {"accent", "#2563eb"},       // today, selection, checked states, primary action
        {"accent-hover", "#1d4fd8"},
        {"accent-ink", "#1d4ed8"},   // accent text on light tints
        {"focus", "#2563eb"},        // focus rings
        {"danger", "#c53030"},       // overdue dates and destructive actions only
        {"control-border", "#7e8591"}, // 3:1 on surface, canvas, row-hover and selection
        {"control-off", "#e3e5e9"},  // disabled switch track and primary button
        {"line", "#e6e8ec"},         // hairlines between regions
        {"line-strong", "#d9dce1"},  // menus and popups
        {"surface", "#ffffff"},
        {"canvas", "#f4f5f7"},       // list pane, settings navigation, status line
        {"tint", "#f1f3f5"},         // quiet chips and previews
        {"hover", "#eceef1"},        // buttons
        {"row-hover", "#e9ebef"},    // list rows, visible on both canvas and surface
        {"pressed", "#e0e3e8"},
        {"selection", "#e0eafd"},
        {"handle", "#c4c8cf"},       // scrollbar handle
        {"handle-hover", "#9ea4ae"},
    };
    return values;
}
inline QColor color(const char *name) {
    Q_ASSERT(tokens().contains(name));
    return QColor(tokens().value(name));
}
// Windows "Make text bigger" factor; Qt 5 ignores it, so the stylesheets apply it.
inline qreal textScale = 1.0;
inline qreal systemTextScale() {
    QSettings accessibility("HKEY_CURRENT_USER\\Software\\Microsoft\\Accessibility", QSettings::NativeFormat);
    return qBound(100, accessibility.value("TextScaleFactor", 100).toInt(), 225) / 100.0;
}
// Swaps @name references for hex values so later passes (floating scaling) see plain colours.
inline QString resolve(const QString &style) {
    static const QRegularExpression reference("@([a-z][a-z-]*)");
    QString out;
    int position = 0;
    auto matches = reference.globalMatch(style);
    while (matches.hasNext()) {
        auto match = matches.next();
        Q_ASSERT(tokens().contains(match.captured(1)));
        out += style.mid(position, match.capturedStart() - position) + tokens().value(match.captured(1));
        position = match.capturedEnd();
    }
    return out + style.mid(position);
}
// Font sizes follow the factor, and so does any length written with the "tx" unit
// (controls that must keep pace with the text beside them, such as check indicators).
inline QString scaleText(const QString &style, qreal factor = textScale) {
    static const QRegularExpression size("(font-size\\s*:\\s*|\\b)([0-9]+)(px|tx)");
    QString out;
    int position = 0;
    auto matches = size.globalMatch(style);
    while (matches.hasNext()) {
        auto match = matches.next();
        out += style.mid(position, match.capturedStart() - position);
        if (match.captured(3) == "px" && match.captured(1).isEmpty())
            out += match.captured(0);
        else
            out += match.captured(1) + QString::number(qRound(match.captured(2).toInt() * factor)) + "px";
        position = match.capturedEnd();
    }
    return out + style.mid(position);
}
inline QString style(const QString &qss) {
    return scaleText(resolve(qss));
}
// style.qss with tokens resolved but before text scaling; the floating window scales it itself.
inline QString baseStyle;
} // namespace theme
