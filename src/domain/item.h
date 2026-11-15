#pragma once
#include <QtCore>

struct Item {
    qint64 id = 0;
    QString title, group = "other", type = "task", date, time, timeKind = "due", notes;
    bool pinned = false, done = false, showCalendar = false;
    int progress = 0, target = 1;
    QString completedAt;
    QJsonObject rule, records;
};
struct Category {
    QString id, name, color;
};
bool visibleInActive(const Item &item, const QDateTime &now = QDateTime::currentDateTimeUtc(),
                     int retentionHours = 48);
QString planBucket(const Item &, const QDate &);
bool planOccurs(const Item &, const QDate &);
Item planState(Item, const QDate &);
Item recordPlan(Item, const QDate &);
void validatePlan(const Item &);
