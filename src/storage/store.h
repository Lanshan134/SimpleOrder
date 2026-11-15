#pragma once
#include "domain/item.h"
#include <QtSql>
#include <stdexcept>

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
