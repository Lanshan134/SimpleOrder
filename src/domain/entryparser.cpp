#include "domain/entryparser.h"
#include <algorithm>

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
int entryNumber(const QString &value, bool digitSequence = false) {
    if (QRegularExpression("^[0-9]+$").match(value).hasMatch()) {
        bool ok = false;
        int result = value.toInt(&ok);
        return ok ? result : -1;
    }
    const QString digits = "零一二三四五六七八九";
    QString text = value;
    text.replace("〇", "零");
    text.replace("两", "二");
    if (!QRegularExpression("^[零一二三四五六七八九十百千]+$").match(text).hasMatch())
        return -1;
    if (!text.contains(QRegularExpression("[十百千]"))) {
        if (text.size() > 1 && !digitSequence && !text.startsWith("零"))
            return -1;
        int result = 0;
        for (auto ch : text) {
            result = result * 10 + digits.indexOf(ch);
            if (result > 9999)
                return -1;
        }
        return result;
    }
    int total = 0, current = -1, lastUnit = 10000;
    for (auto ch : text) {
        int digit = digits.indexOf(ch);
        if (digit >= 0) {
            if (digit == 0) {
                if (current > 0)
                    return -1;
                current = 0;
            } else {
                if (current > 0)
                    return -1;
                current = digit;
            }
        } else {
            int unit = ch == QChar(0x5341) ? 10 : ch == QChar(0x767e) ? 100 : 1000;
            if (unit >= lastUnit || current == 0 || (current < 0 && unit != 10))
                return -1;
            total += (current < 0 ? 1 : current) * unit;
            current = -1;
            lastUnit = unit;
        }
    }
    return total + qMax(0, current);
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
    const QString digits = "0-9零〇一二两三四五六七八九十百千";
    const QString number = "[" + digits + "]{1,12}";
    const QString left = "(?<![" + digits + "A-Za-z:第])";
    const QString right = "(?![" + digits + ":])";
    addDates(R"((?<![0-9A-Za-z])\d{4}[-/]\d{1,2}[-/]\d{1,2}(?![0-9/-]))");
    addDates(left + "(?:" + number + "年)?" + number + "月" + number + "(?:日|号)?" + right);
    addDates(R"((?<![0-9A-Za-z/-])\d{1,2}/\d{1,2}(?![0-9/-]))");
    addDates(R"((?:本周|下周|周|星期)[一二三四五六日天])");
    addDates("今天|明天|后天|" + left + number + "天后");
    const QString period = "(?:(?:上午|下午|晚上|中午|凌晨|早上))?";
    auto times = tokens(text, left + period + number + ":" + number + right);
    for (auto t : tokens(text, left + period + number + "点(?:半|" + number + "分?)?" + right))
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
        else if (value.endsWith("天后")) {
            int days = entryNumber(value.left(value.size() - 2));
            if (days >= 0 && days <= 9999)
                out.date = today.addDays(days);
        } else if (value.contains("周") || value.contains("星期")) {
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
            auto parts = value.split(QRegularExpression("[^" + digits + "]+"), Qt::SkipEmptyParts);
            if (parts.size() == 3)
                out.date = QDate(entryNumber(parts[0], true), entryNumber(parts[1]), entryNumber(parts[2]));
            else if (parts.size() == 2)
                out.date = nextAnnual(entryNumber(parts[0]), entryNumber(parts[1]), today);
        }
        if (!out.date.isValid() || out.date.year() < 1 || out.date.year() > 9999) {
            out.error = "日期无效，请检查年月日";
            return out;
        }
    }
    if (!times.isEmpty()) {
        const auto value = times.first().value;
        auto numbers = value.split(QRegularExpression("[^" + digits + "]+"), Qt::SkipEmptyParts);
        int hour = entryNumber(numbers[0]);
        int minute = numbers.size() == 2 ? entryNumber(numbers[1]) : value.endsWith("半") ? 30 : 0;
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
