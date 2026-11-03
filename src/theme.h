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
        {"ink", "#293448"},          // primary text
        {"ink-muted", "#5f6b7c"},    // secondary text, headings, details
        {"ink-soft", "#66758a"},     // placeholders, weekends
        {"ink-control", "#5b687a"},  // text buttons, combo arrow
        {"ink-tab", "#56657c"},      // inactive tabs and choices
        {"ink-group", "#5a6d86"},    // group box titles
        {"ink-disabled", "#a6adba"}, // disabled text (exempt from contrast)
        {"accent", "#4a7099"},       // primary buttons, checked states
        {"accent-hover", "#3f6189"},
        {"accent-ink", "#315f9b"},   // accent text on light tints
        {"focus", "#456d98"},        // focus rings
        {"danger", "#b65353"},
        {"control-border", "#808ea3"},
        {"control-off", "#e4e8ee"},  // disabled switch track
        {"line", "#e1e7f0"},         // panel and card borders
        {"line-strong", "#d8e1ec"},  // tab and header rules
        {"surface", "#ffffff"},
        {"canvas", "#edf0f4"},       // manager background
        {"panel", "#f7f9fc"},        // sub-panels, dialogs
        {"tint", "#edf2f8"},         // navigation, headers, quiet chips
        {"tint-strong", "#e8edf4"},  // inactive editor tabs
        {"hover", "#eaf1fb"},
        {"row-hover", "#f4f7fb"},
        {"pressed", "#d6e4f4"},
        {"selection", "#dce8f6"},
        {"track", "#f1f4f8"},        // scrollbar track
        {"handle", "#c3ccd8"},       // scrollbar handle
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
inline QString scaleText(const QString &style, qreal factor = textScale) {
    static const QRegularExpression size("(font-size\\s*:\\s*)([0-9]+)px");
    QString out;
    int position = 0;
    auto matches = size.globalMatch(style);
    while (matches.hasNext()) {
        auto match = matches.next();
        out += style.mid(position, match.capturedStart() - position) + match.captured(1) +
               QString::number(qRound(match.captured(2).toInt() * factor)) + "px";
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
