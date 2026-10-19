#include "store.h"

static void fail(const QString &s) {
    throw std::runtime_error(s.toUtf8().constData());
}
bool visibleInActive(const Item &item, const QDateTime &now, int retentionHours) {
    if (!item.done)
        return true;
    if (retentionHours < 0)
        return true;
    const auto at = QDateTime::fromString(item.completedAt, Qt::ISODateWithMs);
    return at.isValid() && at.secsTo(now) < qint64(retentionHours) * 60 * 60;
}
void Store::run(const QString &sql) {
    QSqlQuery q(db);
    if (!q.exec(sql))
        fail(q.lastError().text());
}
Store::Store(const QString &path) {
    connection = QUuid::createUuid().toString();
    db = QSqlDatabase::addDatabase("QSQLITE", connection);
    db.setDatabaseName(path);
    if (!db.open())
        fail("无法打开数据库：" + db.lastError().text());
    run("PRAGMA busy_timeout=5000");
    run("CREATE TABLE IF NOT EXISTS items(id INTEGER PRIMARY KEY AUTOINCREMENT,title TEXT NOT NULL,grp TEXT "
        "NOT NULL,type TEXT NOT NULL,date TEXT,time TEXT,time_kind TEXT,notes TEXT,pinned INTEGER,done "
        "INTEGER,show_calendar INTEGER,progress INTEGER,target INTEGER,completed_at TEXT)");
    if (!db.record("items").contains("completed_at")) {
        const QString backup =
            path + ".before-v02-" + QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmsszzz") + ".bak";
        if (!QFile::copy(path, backup))
            fail("升级前备份失败，未修改旧数据库");
        run("ALTER TABLE items ADD COLUMN completed_at TEXT");
    }
    if (!db.record("items").contains("rule") || !db.record("items").contains("records")) {
        const auto backup =
            path + ".before-plan-" + QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmsszzz") + ".bak";
        if (!QFile::copy(path, backup))
            fail("计划升级前备份失败");
        if (!db.transaction())
            fail(db.lastError().text());
        try {
            if (!db.record("items").contains("rule"))
                run("ALTER TABLE items ADD COLUMN rule TEXT");
            if (!db.record("items").contains("records"))
                run("ALTER TABLE items ADD COLUMN records TEXT");
            if (!db.commit())
                fail(db.lastError().text());
        } catch (...) {
            db.rollback();
            throw;
        }
    }
    run("CREATE TABLE IF NOT EXISTS categories(id TEXT PRIMARY KEY,name TEXT NOT NULL,color TEXT NOT NULL)");
    QSqlQuery count(db);
    count.exec("SELECT count(*) FROM categories");
    count.next();
    if (!count.value(0).toInt()) {
        for (const auto &c : QList<Category>{{"study", "学习研究", "#6b97c6"},
                                             {"project", "项目", "#a68ac5"},
                                             {"life", "生活", "#80a995"},
                                             {"other", "其他", "#aaaeb8"}})
            saveCategory(c);
    }
    QSqlQuery stamp(db);
    stamp.prepare(
        "UPDATE items SET completed_at=? WHERE done=1 AND (completed_at IS NULL OR completed_at='')");
    stamp.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!stamp.exec())
        fail(stamp.lastError().text());
    run("CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY,value TEXT)");
    run("INSERT OR IGNORE INTO meta(key,value) VALUES('schema','1')");
    run("UPDATE meta SET value='3' WHERE key='schema'");
}
Store::~Store() {
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connection);
}
QList<Item> Store::all() const {
    QSqlQuery q(db);
    if (!q.exec("SELECT * FROM items ORDER BY id"))
        fail(q.lastError().text());
    QList<Item> out;
    while (q.next()) {
        Item i;
        i.id = q.value(0).toLongLong();
        i.title = q.value(1).toString();
        i.group = q.value(2).toString();
        i.type = q.value(3).toString();
        i.date = q.value(4).toString();
        i.time = q.value(5).toString();
        i.timeKind = q.value(6).toString();
        i.notes = q.value(7).toString();
        i.pinned = q.value(8).toBool();
        i.done = q.value(9).toBool();
        i.showCalendar = q.value(10).toBool();
        i.progress = q.value(11).toInt();
        i.target = q.value(12).toInt();
        i.completedAt = q.value(13).toString();
        i.rule = QJsonDocument::fromJson(q.value(14).toByteArray()).object();
        i.records = QJsonDocument::fromJson(q.value(15).toByteArray()).object();
        out << i;
    }
    return out;
}
Item Store::save(Item i) {
    i.title = i.title.trimmed();
    if (i.title.isEmpty() || i.title.size() > 200)
        fail("事项名称应为 1–200 个字符");
    QSqlQuery category(db);
    category.prepare("SELECT id FROM categories WHERE id=?");
    category.addBindValue(i.group);
    if (!category.exec() || !category.next() || !QStringList{"task", "plan"}.contains(i.type))
        fail("事项种类或类型无效");
    if (i.done && i.completedAt.isEmpty())
        i.completedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    if (!i.done)
        i.completedAt.clear();
    if (!i.completedAt.isEmpty() && !QDateTime::fromString(i.completedAt, Qt::ISODateWithMs).isValid())
        fail("完成时间无效");
    if (!i.date.isEmpty() && (!QDate::fromString(i.date, Qt::ISODate).isValid() ||
                              QDate::fromString(i.date, Qt::ISODate).toString(Qt::ISODate) != i.date))
        fail("日期格式应为 YYYY-MM-DD");
    if (!i.time.isEmpty() && (!QTime::fromString(i.time, "HH:mm").isValid() ||
                              QTime::fromString(i.time, "HH:mm").toString("HH:mm") != i.time))
        fail("时间格式应为 HH:mm");
    if (!QStringList{"start", "due"}.contains(i.timeKind))
        fail("时间语义无效");
    if (i.target < 1 || i.target > 9999 || i.progress < 0 || i.progress > i.target)
        fail("计划进度必须在 0 到目标次数之间");
    validatePlan(i);
    QSqlQuery q(db);
    QString fields = "title=?,grp=?,type=?,date=?,time=?,time_kind=?,notes=?,pinned=?,done=?,show_calendar=?,"
                     "progress=?,target=?,completed_at=?,rule=?,records=?";
    if (i.id)
        q.prepare("UPDATE items SET " + fields + " WHERE id=?");
    else
        q.prepare("INSERT INTO "
                  "items(title,grp,type,date,time,time_kind,notes,pinned,done,show_calendar,progress,target,"
                  "completed_at,rule,records) "
                  "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    for (const auto &v :
         QVariantList{i.title, i.group, i.type, i.date, i.time, i.timeKind, i.notes, i.pinned, i.done,
                      i.showCalendar, i.progress, i.target, i.completedAt,
                      QString::fromUtf8(QJsonDocument(i.rule).toJson(QJsonDocument::Compact)),
                      QString::fromUtf8(QJsonDocument(i.records).toJson(QJsonDocument::Compact))})
        q.addBindValue(v);
    if (i.id)
        q.addBindValue(i.id);
    if (!q.exec())
        fail(q.lastError().text());
    if (!i.id)
        i.id = q.lastInsertId().toLongLong();
    return i;
}
void Store::remove(qint64 id) {
    QSqlQuery q(db);
    q.prepare("DELETE FROM items WHERE id=?");
    q.addBindValue(id);
    if (!q.exec())
        fail(q.lastError().text());
}
void Store::seed() {
    if (!all().isEmpty())
        return;
    QStringList names = {"每日英语学习",   "研究记录整理", "健身计划",     "参加组会",
                         "提交实验报告",   "修改实验代码", "购买生活用品", "整理笔记",
                         "备份服务器文件", "每周阅读论文", "每周运动",     "月度资料整理"};
    for (int n = 0; n < names.size(); ++n) {
        Item i;
        i.title = names[n];
        i.group = QStringList{"study", "project", "life"}[n % 3];
        i.type = n == 2 || n >= 9 ? "plan" : "task";
        i.pinned = n < 3;
        if (n >= 3 && n <= 6)
            i.date = QDate::currentDate().addDays(n - 3).toString(Qt::ISODate);
        if (n == 3)
            i.time = "14:00";
        if (n == 4)
            i.time = "18:00";
        if (i.type == "plan") {
            i.target = 3;
            i.progress = 1;
            i.showCalendar = n == 10;
            if (n == 10)
                i.date = QDate::currentDate().addDays(2).toString(Qt::ISODate);
        }
        save(i);
    }
}
QJsonObject Store::json(const Item &i) {
    return {{"id", double(i.id)},
            {"title", i.title},
            {"group", i.group},
            {"type", i.type},
            {"date", i.date},
            {"time", i.time},
            {"timeKind", i.timeKind},
            {"notes", i.notes},
            {"pinned", i.pinned},
            {"done", i.done},
            {"showCalendar", i.showCalendar},
            {"progress", i.progress},
            {"target", i.target},
            {"completedAt", i.completedAt},
            {"rule", i.rule},
            {"records", i.records}};
}
Item Store::parse(const QJsonObject &o) {
    if (!o["title"].isString() || !o["type"].isString() || !o["group"].isString())
        fail("备份缺少必要字段");
    for (const QString &k : QStringList{"pinned", "done", "showCalendar"})
        if (o.contains(k) && !o[k].isBool())
            fail("备份布尔字段无效：" + k);
    for (const QString &k : QStringList{"progress", "target"})
        if (o.contains(k) && (!o[k].isDouble() || o[k].toDouble() != o[k].toInt()))
            fail("备份次数字段无效：" + k);
    for (const QString &k : QStringList{"date", "time", "notes", "completedAt"})
        if (o.contains(k) && !o[k].isNull() && !o[k].isString())
            fail("备份文本字段无效：" + k);
    Item i;
    i.title = o["title"].toString();
    i.group = o["group"].toString();
    i.type = o["type"].toString();
    i.date = o["date"].toString();
    i.time = o["time"].toString();
    i.timeKind = o["timeKind"].toString("due");
    i.notes = o["notes"].toString();
    i.pinned = o["pinned"].toBool();
    i.done = o["done"].toBool();
    i.showCalendar = o["showCalendar"].toBool();
    i.progress = o["progress"].toInt();
    i.target = o["target"].toInt(1);
    i.completedAt = o["completedAt"].toString();
    for (const auto &key : {"rule", "records"})
        if (o.contains(key) && !o[key].isObject())
            fail("计划配置或记录格式无效");
    i.rule = o["rule"].toObject();
    i.records = o["records"].toObject();
    return i;
}
void Store::backup(const QString &path) const {
    QJsonArray arr;
    for (const auto &i : all())
        arr.append(json(i));
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        fail(f.errorString());
    QJsonArray types;
    for (auto c : categories())
        types.append(QJsonObject{{"id", c.id}, {"name", c.name}, {"color", c.color}});
    const QByteArray bytes =
        QJsonDocument(QJsonObject{{"version", 3}, {"items", arr}, {"categories", types}}).toJson();
    if (f.write(bytes) != bytes.size() || !f.commit())
        fail(f.errorString());
}
void Store::importJson(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        fail(f.errorString());
    if (f.size() > 20 * 1024 * 1024)
        fail("备份超过 20 MB 限制");
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject() || !doc.object()["items"].isArray())
        fail("备份 JSON 格式无效");
    auto arr = doc.object()["items"].toArray();
    if (arr.size() > 10000)
        fail("备份事项过多");
    if (!db.transaction())
        fail(db.lastError().text());
    try {
        run("DELETE FROM items");
        if (doc.object().contains("categories")) {
            if (!doc.object()["categories"].isArray())
                fail("类型数据无效");
            auto cats = doc.object()["categories"].toArray();
            if (cats.size() > 200)
                fail("类型过多");
            run("DELETE FROM categories");
            QSet<QString> ids;
            for (auto v : cats) {
                auto o = v.toObject();
                Category c{o["id"].toString(), o["name"].toString(), o["color"].toString()};
                if (ids.contains(c.id))
                    fail("类型 ID 重复");
                ids.insert(c.id);
                saveCategory(c);
            }
            if (!ids.contains("other"))
                fail("备份缺少默认类型");
        }
        for (auto value : arr) {
            if (!value.isObject())
                fail("备份事项格式无效");
            save(parse(value.toObject()));
        }
        if (!db.commit())
            fail(db.lastError().text());
    } catch (...) {
        db.rollback();
        throw;
    }
}
QList<Category> Store::categories() const {
    QSqlQuery q(db);
    if (!q.exec("SELECT id,name,color FROM categories ORDER BY rowid"))
        fail(q.lastError().text());
    QList<Category> out;
    while (q.next())
        out << Category{q.value(0).toString(), q.value(1).toString(), q.value(2).toString()};
    return out;
}
void Store::saveCategory(const Category &input) {
    Category c = input;
    c.name = c.name.trimmed();
    if (!QRegularExpression("^[a-zA-Z0-9_-]{1,80}$").match(c.id).hasMatch() || c.name.isEmpty() ||
        c.name.size() > 40 || !QRegularExpression("^#[a-fA-F0-9]{6}$").match(c.color).hasMatch())
        fail("类型名称应为 1–40 字，颜色必须有效");
    for (auto old : categories())
        if (old.id != c.id && old.name.compare(c.name, Qt::CaseInsensitive) == 0)
            fail("类型名称不能重复");
    QSqlQuery q(db);
    q.prepare("INSERT INTO categories(id,name,color) VALUES(?,?,?) ON CONFLICT(id) DO UPDATE SET "
              "name=excluded.name,color=excluded.color");
    q.addBindValue(c.id);
    q.addBindValue(c.name);
    q.addBindValue(c.color);
    if (!q.exec())
        fail(q.lastError().text());
}
void Store::removeCategory(const QString &id, const QString &destination) {
    if (id == "other")
        fail("默认类型不能删除，可以改名与换色");
    bool valid = false;
    for (auto c : categories())
        if (c.id == destination && destination != id)
            valid = true;
    if (!valid)
        fail("请选择有效的转移类型");
    if (!db.transaction())
        fail(db.lastError().text());
    try {
        QSqlQuery q(db);
        q.prepare("UPDATE items SET grp=? WHERE grp=?");
        q.addBindValue(destination);
        q.addBindValue(id);
        if (!q.exec())
            fail(q.lastError().text());
        q.prepare("DELETE FROM categories WHERE id=?");
        q.addBindValue(id);
        if (!q.exec())
            fail(q.lastError().text());
        if (!db.commit())
            fail(db.lastError().text());
    } catch (...) {
        db.rollback();
        throw;
    }
}
void Store::clearItems() {
    run("DELETE FROM items");
}
void Store::reorderCategories(const QStringList &ids) {
    QStringList current;
    for (auto c : categories())
        current << c.id;
    if (ids.size() != current.size() ||
        QSet<QString>(ids.begin(), ids.end()) != QSet<QString>(current.begin(), current.end()))
        fail("类型排序无效");
    if (!db.transaction())
        fail(db.lastError().text());
    try {
        run("UPDATE categories SET rowid=-rowid");
        int order = 0;
        for (auto id : ids) {
            QSqlQuery q(db);
            q.prepare("UPDATE categories SET rowid=? WHERE id=?");
            q.addBindValue(++order);
            q.addBindValue(id);
            if (!q.exec())
                fail(q.lastError().text());
        }
        if (!db.commit())
            fail(db.lastError().text());
    } catch (...) {
        db.rollback();
        throw;
    }
}
