#include "store.h"

namespace {
QString kind(const Item &i) {
    return i.rule.value("kind").toString("total");
}
QDate from(const QJsonObject &o, const char *key) {
    return QDate::fromString(o.value(key).toString(), Qt::ISODate);
}
QString prefix(const Item &i) {
    // Rule changes start new counters; the records for earlier rules stay in the backup.
    QJsonObject config = i.rule;
    config.insert("target", i.target);
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(config).toJson(QJsonDocument::Compact),
                                                        QCryptographicHash::Sha256)
                                   .toHex()
                                   .left(16)) +
           ":";
}
void invalid(const QString &s) {
    throw std::runtime_error(s.toUtf8().constData());
}
bool within(const Item &i, const QDate &d) {
    const auto start = from(i.rule, "start"), end = from(i.rule, "end");
    return (!start.isValid() || d >= start) && (!end.isValid() || d <= end);
}
} // namespace
QString planBucket(const Item &i, const QDate &d) {
    if (kind(i) == "quota") {
        auto period = i.rule.value("period").toString();
        if (period == "week")
            return prefix(i) + d.addDays(1 - d.dayOfWeek()).toString(Qt::ISODate);
        if (period == "month")
            return prefix(i) + d.toString("yyyy-MM");
    }
    if (kind(i) == "total")
        return prefix(i) + "total";
    return prefix(i) + d.toString(Qt::ISODate);
}
bool planOccurs(const Item &i, const QDate &d) {
    if (i.type != "plan" || i.rule.isEmpty())
        return i.date == d.toString(Qt::ISODate);
    if (!within(i, d))
        return false;
    auto k = kind(i);
    if (k == "total")
        return from(i.rule, "end") == d;
    if (k == "quota")
        return true;
    if (k == "dates") {
        if (i.rule.value("mode").toString() == "week")
            return i.rule.value("days").toArray().contains(d.dayOfWeek());
        const auto days = i.rule.value("days").toArray();
        if (days.contains(d.day()))
            return true;
        if (d.day() == d.daysInMonth()) {
            if (days.contains(0))
                return true;
            if (i.rule.value("missing").toString() == "last")
                for (auto v : days)
                    if (v.toInt() > d.daysInMonth())
                        return true;
        }
        return false;
    }
    const auto first = from(i.rule, "first");
    int interval = i.rule.value("interval").toInt(1);
    if (!first.isValid() || d < first)
        return false;
    if (i.rule.value("basis").toString() != "after")
        return first.daysTo(d) % interval == 0;
    QDate latest;
    const auto p = prefix(i);
    // Query relative to d so past/future calendar dates retain their own occurrence state.
    for (auto it = i.records.begin(); it != i.records.end(); ++it) {
        auto completed = QDate::fromString(it.key().mid(p.size()), Qt::ISODate);
        if (it.key().startsWith(p) && it.value().toObject().value("count").toInt() > 0 && completed < d &&
            completed > latest)
            latest = completed;
    }
    auto next = latest.isValid() ? latest.addDays(interval) : first;
    // A postponed execution stays available until it is completed.
    return d >= next;
}
Item planState(Item i, const QDate &d) {
    if (i.type != "plan" || i.rule.isEmpty())
        return i;
    auto record = i.records.value(planBucket(i, d)).toObject();
    i.progress = record.value("count").toInt();
    i.done = i.progress >= i.target;
    i.completedAt = i.done ? record.value("at").toString() : QString();
    return i;
}
Item recordPlan(Item i, const QDate &d) {
    if (!within(i, d) || (kind(i) != "total" && !planOccurs(i, d)))
        invalid("该日期不是计划执行日，或不在有效期内");
    i = planState(i, d);
    i.progress = i.done ? 0 : qMin(i.target, i.progress + 1);
    i.done = i.progress >= i.target;
    i.completedAt = i.done ? QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) : QString();
    i.records.insert(planBucket(i, d), QJsonObject{{"count", i.progress}, {"at", i.completedAt}});
    return i;
}
void validatePlan(const Item &i) {
    if (i.type != "plan" || i.rule.isEmpty())
        return;
    if (!QStringList{"quota", "dates", "interval", "total"}.contains(kind(i)))
        invalid("计划规则无效");
    for (const auto &key : {"start", "end", "first"}) {
        auto value = i.rule.value(key).toString();
        if (!value.isEmpty() &&
            (!from(i.rule, key).isValid() || from(i.rule, key).toString(Qt::ISODate) != value))
            invalid("计划日期格式应为 YYYY-MM-DD");
    }
    auto start = from(i.rule, kind(i) == "interval" ? "first" : "start"), end = from(i.rule, "end");
    if (start.isValid() && end.isValid() && start > end)
        invalid("结束日期不能早于开始日期");
    if (kind(i) == "quota" &&
        !QStringList{"day", "week", "month"}.contains(i.rule.value("period").toString()))
        invalid("请选择每天、每周或每月");
    if (kind(i) == "dates") {
        auto mode = i.rule.value("mode").toString();
        if (!QStringList{"week", "month"}.contains(mode))
            invalid("指定日期模式无效");
        auto days = i.rule.value("days").toArray();
        if (days.isEmpty())
            invalid("请至少选择一个执行日");
        for (auto day : days)
            if (!day.isDouble() || day.toDouble() != day.toInt() || day.toInt() < (mode == "week" ? 1 : 0) ||
                day.toInt() > (mode == "week" ? 7 : 31))
                invalid("执行日无效");
        if (!QStringList{"skip", "last"}.contains(i.rule.value("missing").toString("skip")))
            invalid("缺失日期处理无效");
    }
    if (kind(i) == "interval") {
        if (!from(i.rule, "first").isValid())
            invalid("请选择首次日期");
        if (i.rule.value("interval").toInt() < 1 || i.rule.value("interval").toInt() > 9999)
            invalid("重复间隔应为 1–9999 天");
        if (!QStringList{"fixed", "after"}.contains(i.rule.value("basis").toString()))
            invalid("间隔计算方式无效");
    }
    if ((kind(i) == "dates" || kind(i) == "interval") && i.target != 1)
        invalid("指定日期和间隔重复每次记录一次完成");
    for (auto it = i.records.begin(); it != i.records.end(); ++it) {
        auto r = it.value().toObject();
        if (!it.value().isObject() || r.value("count").toInt(-1) < 0 || r.value("count").toInt() > 9999)
            invalid("计划完成记录无效");
    }
}
