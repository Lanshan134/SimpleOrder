#include "ui/widgets/smartentry.h"
#include <QtWidgets>

SmartEntry::SmartEntry(QWidget *parent) : QPlainTextEdit(parent) {
    setObjectName("smartEntry");
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setTabChangesFocus(true);
    setFixedHeight(36);
    document()->setDocumentMargin(0);
    connect(this, &QPlainTextEdit::textChanged, this, [this] { updatePreview(); });
}
void SmartEntry::updatePreview() {
    const auto parsed = parseEntry(toPlainText());
    QList<QTextEdit::ExtraSelection> marks;
    for (auto span : parsed.spans) {
        QTextEdit::ExtraSelection mark;
        mark.cursor = QTextCursor(document());
        mark.cursor.setPosition(span.start);
        mark.cursor.setPosition(span.start + span.length, QTextCursor::KeepAnchor);
        QColor color = parsed.error.isEmpty() ? QColor("#397cb5") : QColor("#b44b4b");
        for (auto widget = static_cast<QWidget *>(this); widget; widget = widget->parentWidget())
            if (widget->property("foregroundOpacity").isValid()) {
                color.setAlphaF(widget->property("foregroundOpacity").toDouble());
                break;
            }
        mark.format.setForeground(color);
        marks << mark;
    }
    setExtraSelections(marks);
    auto tip = parsed.error;
    if (tip.isEmpty() && !parsed.spans.isEmpty()) {
        QStringList fields;
        if (parsed.date.isValid())
            fields << parsed.date.toString(Qt::ISODate);
        if (parsed.time.isValid())
            fields << parsed.time.toString("HH:mm");
        tip = fields.join(" · ");
    }
    setToolTip(tip);
    setProperty("entryError", parsed.error);
}
void SmartEntry::submitEntry() {
    auto value = toPlainText();
    if (value.trimmed().isEmpty())
        return;
    auto parsed = parseEntry(value);
    if (!parsed.error.isEmpty()) {
        QToolTip::showText(mapToGlobal(QPoint(0, height())), parsed.error, this);
        return;
    }
    QToolTip::hideText();
    if (submit && submit(value))
        clear();
}
void SmartEntry::changeEvent(QEvent *event) {
    QPlainTextEdit::changeEvent(event);
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::PaletteChange)
        updatePreview();
}
void SmartEntry::inputMethodEvent(QInputMethodEvent *event) {
    composing = !event->preeditString().isEmpty();
    QPlainTextEdit::inputMethodEvent(event);
}
void SmartEntry::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (composing) {
            event->accept();
            return;
        }
        submitEntry();
        event->accept();
        return;
    }
    if (!event->text().isEmpty() && event->text().at(0).isPrint() &&
        !(event->modifiers() & Qt::ControlModifier) &&
        toPlainText().size() - textCursor().selectedText().size() + event->text().size() > 1000) {
        event->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}
void SmartEntry::insertFromMimeData(const QMimeData *data) {
    auto text = data->text();
    text.replace(QRegularExpression("[\\r\\n]+"), " ");
    text = text.left(qMax(0, 1000 - toPlainText().size() + textCursor().selectedText().size()));
    textCursor().insertText(text);
}
