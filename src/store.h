#pragma once
#include <QtCore>
#include <QtSql>
#include <stdexcept>

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

class Store {
    QString connection;
    QSqlDatabase db;
    void run(const QString &sql);

  public:
    explicit Store(const QString &path);
    ~Store();
    QList<Item> all() const;
    Item save(Item item);
    void remove(qint64 id);
    void seed();
    void backup(const QString &path) const;
    void importJson(const QString &path);
    QList<Category> categories() const;
    void saveCategory(const Category &category);
    void removeCategory(const QString &id, const QString &destination = "other");
    void clearItems();
    void reorderCategories(const QStringList &ids);
    static QJsonObject json(const Item &i);
    static Item parse(const QJsonObject &obj);
};
