#include "smartentry.h"
#include <algorithm>
#include <stdexcept>
namespace {
struct Token {
    int start, length;
    QString value;
};
QList<Token> tokens(const QString &text, const QString &pattern) {
    QList<Token> out;
    auto matches = QRegularExpression(pattern).globalMatch(text);
    while (matches.hasNext()) {
        auto m = matches.next();
        out << Token{m.capturedStart(), m.capturedLength(), m.captured()};
    }
    return out;
}
bool overlaps(const QList<Token> &tokens, const Token &candidate) {
    for (const auto &t : tokens)
        if (candidate.start < t.start + t.length && t.start < candidate.start + candidate.length)
            return true;
    return false;
}
QDate nextAnnual(int month, int day, const QDate &today) {
    for (int year = today.year(); year <= qMin(9999, today.year() + 8); ++year) {
        QDate date(year, month, day);
        if (date.isValid() && date >= today)
            return date;
    }
    return {};
}
} // namespace
ParsedEntry parseEntry(const QString &source, const QDate &today) {
    ParsedEntry out;
    out.title = source.trimmed();
    if (!today.isValid()) {
        out.error = "当前日期无效";
        return out;
    }
    // Keep UTF-16 positions unchanged so highlight ranges match the original input.
    QString text = source;
    text.replace(QChar(0xff1a), ':');
    QList<Token> dates;
    auto addDates = [&](const QString &pattern) {
        for (auto t : tokens(text, pattern))
            if (!overlaps(dates, t))
                dates << t;
    };
    addDates(R"((?<![0-9A-Za-z])\d{4}[-/]\d{1,2}[-/]\d{1,2}(?![0-9/-]))");
    addDates(R"((?<![0-9A-Za-z])(?:\d{4}年)?\d{1,2}月\d{1,2}(?:日|号)?(?![0-9]))");
    addDates(R"((?<![0-9A-Za-z/-])\d{1,2}/\d{1,2}(?![0-9/-]))");
    addDates(R"((?:本周|下周|周|星期)[一二三四五六日天])");
    addDates(R"(今天|明天|后天|(?<![0-9A-Za-z])\d{1,4}天后)");
    auto times =
        tokens(text, R"((?<![0-9A-Za-z:])(?:(?:上午|下午|晚上|中午|凌晨|早上))?\d{1,2}:\d{2}(?![0-9:]))");
    for (
        auto t : tokens(
            text,
            R"((?<![0-9A-Za-z:])(?:(?:上午|下午|晚上|中午|凌晨|早上))?\d{1,2}点(?:半|\d{1,2}分?)?(?![0-9]))"))
        if (!overlaps(times, t))
            times << t;
    for (const auto &t : dates)
        out.spans << EntrySpan{t.start, t.length};
    for (const auto &t : times)
        out.spans << EntrySpan{t.start, t.length};
    if (dates.size() > 1 || times.size() > 1) {
        out.error = "存在多个日期或时间，请只保留一个";
        return out;
    }
    if (!dates.isEmpty()) {
        const auto value = dates.first().value;
        if (value == "今天")
            out.date = today;
        else if (value == "明天")
            out.date = today.addDays(1);
        else if (value == "后天")
            out.date = today.addDays(2);
        else if (value.endsWith("天后"))
            out.date = today.addDays(value.left(value.size() - 2).toInt());
        else if (value.contains("周") || value.contains("星期")) {
            auto day = QString("一二三四五六日").indexOf(value.right(1)) + 1;
            if (value.endsWith("天"))
                day = 7;
            if (value.startsWith("下周"))
                out.date = today.addDays(8 - today.dayOfWeek() + day - 1);
            else if (value.startsWith("本周"))
                out.date = today.addDays(day - today.dayOfWeek());
            else
                out.date = today.addDays((day - today.dayOfWeek() + 7) % 7);
        } else {
            auto parts = value.split(QRegularExpression("[^0-9]+"), Qt::SkipEmptyParts);
            if (parts.size() == 3)
                out.date = QDate(parts[0].toInt(), parts[1].toInt(), parts[2].toInt());
            else if (parts.size() == 2)
                out.date = nextAnnual(parts[0].toInt(), parts[1].toInt(), today);
        }
        if (!out.date.isValid()) {
            out.error = "日期无效，请检查年月日";
            return out;
        }
    }
    if (!times.isEmpty()) {
        const auto value = times.first().value;
        auto numbers = value.split(QRegularExpression("[^0-9]+"), Qt::SkipEmptyParts);
        int hour = numbers[0].toInt(), minute = numbers.size() == 2    ? numbers[1].toInt()
                                                : value.endsWith("半") ? 30
                                                                       : 0;
        const bool period = QRegularExpression("^(上午|下午|晚上|中午|凌晨|早上)").match(value).hasMatch();
        if (period && hour > 12) {
            out.error = "带上午或下午的时间请使用 0–12 点";
            return out;
        }
        if ((value.startsWith("下午") || value.startsWith("晚上")) && hour >= 1 && hour < 12)
            hour += 12;
        if (value.startsWith("中午") && hour >= 1 && hour <= 2)
            hour += 12;
        if ((value.startsWith("凌晨") || value.startsWith("晚上")) && hour == 12)
            hour = 0;
        out.time = QTime(hour, minute);
        if (!out.time.isValid()) {
            out.error = "时间无效，请使用 00:00–23:59";
            return out;
        }
    }
    auto spans = out.spans;
    std::sort(spans.begin(), spans.end(), [](EntrySpan a, EntrySpan b) { return a.start > b.start; });
    QString title = source;
    for (auto span : spans)
        title.remove(span.start, span.length);
    if (!spans.isEmpty())
        title.replace(QRegularExpression("\\s+"), " ");
    out.title = title.trimmed();
    if (out.title.isEmpty() && !out.spans.isEmpty())
        out.error = "请输入任务名称";
    return out;
}
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
void smartEntryParserTests() {
    auto check = [](bool ok, const char *name) {
        if (!ok)
            throw std::runtime_error(name);
    };
    QDate day(2026, 12, 31);
    auto p = parseEntry("交报告 明天 18:00", day);
    check(p.error.isEmpty() && p.title == "交报告" && p.date == QDate(2027, 1, 1) && p.time == QTime(18, 0),
          "tomorrow across year");
    check(parseEntry("后天下午3点 开会", day).time == QTime(15, 0), "attached natural date and time");
    check(parseEntry("浇花 3天后", day).date == day.addDays(3), "relative days");
    check(parseEntry("开会 下周一", QDate(2026, 10, 25)).date == QDate(2026, 10, 26),
          "next week Sunday boundary");
    check(parseEntry("开会 周一", QDate(2026, 10, 26)).date == QDate(2026, 10, 26), "weekday includes today");
    check(parseEntry("开会 本周一", QDate(2026, 10, 25)).date == QDate(2026, 10, 19),
          "explicit current week");
    check(parseEntry("生日 11月5日", day).date == QDate(2027, 11, 5), "annual date rolls year");
    check(parseEntry("生日 2/29", day).date == QDate(2028, 2, 29), "next valid leap day");
    check(parseEntry("会议 2026-11-05", day).date == QDate(2026, 11, 5), "explicit year kept");
    check(!parseEntry("会议 2026-02-29", day).error.isEmpty(), "invalid leap date rejected");
    check(!parseEntry("会议 4月31日", day).error.isEmpty(), "invalid month date rejected");
    check(parseEntry("学习第3章 项目2026", day).spans.isEmpty(), "ordinary numbers retained");
    check(parseEntry("开会 晚上8点半", day).time == QTime(20, 30), "evening half hour");
    check(parseEntry("开会 18：30", day).time == QTime(18, 30), "fullwidth colon");
    check(parseEntry("开会 凌晨12点", day).time == QTime(0, 0), "midnight");
    check(!parseEntry("开会 25:00", day).error.isEmpty(), "invalid hour rejected");
    check(!parseEntry("开会 下午15点", day).error.isEmpty(), "invalid period hour rejected");
    check(!parseEntry("开会 今天 明天", day).error.isEmpty(), "multiple dates rejected");
    check(!parseEntry("开会 8:00 9:00", day).error.isEmpty(), "multiple times rejected");
    check(!parseEntry("明天 18:00", day).error.isEmpty(), "missing title rejected");
    check(parseEntry("开会 18:00:30", day).spans.isEmpty(), "unsupported seconds not partially extracted");
    check(parseEntry("买菜", day).title == "买菜" && parseEntry("买菜", day).spans.isEmpty(),
          "plain title unchanged");
}
