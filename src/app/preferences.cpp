#include "app/controller.h"
#include <algorithm>

bool Controller::visibleItem(const Item &i) const {
    return !i.done || (settings.value("showCompleted", true).toBool() &&
                       visibleInActive(i, QDateTime::currentDateTimeUtc(),
                                       settings.value("retentionHours", 48).toInt()));
}
QList<Item> Controller::sortedItems(QList<Item> input) const {
    const auto mode = settings.value("sortMode", "date").toString();
    bool undatedLast = settings.value("undatedLast", true).toBool();
    std::stable_sort(input.begin(), input.end(), [&](const Item &a, const Item &b) {
        if (a.done != b.done)
            return !a.done;
        if (mode == "name")
            return QString::localeAwareCompare(a.title, b.title) < 0;
        if (mode == "newest")
            return a.id > b.id;
        if (a.date.isEmpty() != b.date.isEmpty())
            return undatedLast ? !a.date.isEmpty() : a.date.isEmpty();
        if (a.date != b.date)
            return a.date < b.date;
        if (a.time != b.time)
            return a.time < b.time;
        return a.id < b.id;
    });
    return input;
}
void Controller::applyPreferences() {
    bool validDefault = false;
    for (auto category : store.categories())
        if (category.id == settings.value("defaultType", "other").toString())
            validDefault = true;
    if (!validDefault)
        settings.setValue("defaultType", "other");
    setTop(settings.value("alwaysOnTop", false).toBool());
    applyFloatingAppearance();
    scaleFloating(true);
    for (auto list : {pins, tasks, plans, all, dayItems, floatDay})
        for (auto key : QStringList{"showDates", "showTimes", "showProgress"})
            list->setProperty(key.toUtf8(), settings.value(key, true));
    for (auto calendar : {cal, floatCal}) {
        QSignalBlocker b(calendar);
        calendar->setFirstDayOfWeek(Qt::DayOfWeek(settings.value("weekStart", int(Qt::Monday)).toInt()));
        calendar->displayOutside = settings.value("outsideDates", true).toBool();
        calendar->maxDots = settings.value("calendarDots", 3).toInt();
        calendar->showCompleted = settings.value("showCompleted", true).toBool() &&
                                  settings.value("calendarCompleted", true).toBool();
        calendar->retentionHours = settings.value("retentionHours", 48).toInt();
        calendar->repaintCells();
    }
    items = store.all();
    updateLists();
    if (!dirty && !selected)
        fillEditor();
    settings.sync();
    if (!testing)
        maybeAutoBackup();
}
void Controller::closeWindow(bool floatingWindow) {
    persistGeometry();
    auto mode = settings.value("closeBehavior", "switch").toString();
    if (mode == "exit") {
        if (!leaveEditor())
            return;
        qApp->quit();
        return;
    }
    if (mode == "tray" && tray) {
        (floatingWindow ? floating : manager)->hide();
        return;
    }
    (floatingWindow ? floating : manager)->hide();
    auto next = floatingWindow ? manager : floating;
    next->show();
    next->raise();
}
void Controller::maybeAutoBackup(bool force) {
    auto frequency = settings.value("backupFrequency", "off").toString();
    if (!force && frequency == "off")
        return;
    auto last = QDateTime::fromString(settings.value("lastAutoBackup").toString(), Qt::ISODateWithMs);
    int hours = frequency == "weekly" ? 168 : 24;
    auto now = QDateTime::currentDateTimeUtc();
    if (!force && last.isValid() && last.secsTo(now) < hours * 3600)
        return;
    auto path = settings.value("backupDirectory", QFileInfo(settings.fileName()).absolutePath() + "/backups")
                    .toString();
    if (!QDir().mkpath(path))
        throw std::runtime_error("无法创建自动备份目录");
    QDir folder(path);
    auto file = "lighttask-auto-" + now.toString("yyyyMMdd-HHmmsszzz") + ".json";
    store.backup(folder.filePath(file));
    settings.setValue("lastAutoBackup", now.toString(Qt::ISODateWithMs));
    settings.sync();
    auto files = folder.entryList({"lighttask-auto-*.json"}, QDir::Files, QDir::Name);
    files.removeAll(file);
    files.append(file);
    const int keep = qBound(1, settings.value("backupKeep", 7).toInt(), 30);
    QRegularExpression own("^lighttask-auto-[0-9]{8}-[0-9]{9}\\.json$");
    QStringList ownFiles;
    for (auto f : files)
        if (own.match(f).hasMatch())
            ownFiles << f;
    while (ownFiles.size() > keep) {
        auto oldest = ownFiles.takeFirst();
        if (!QFile::remove(folder.filePath(oldest)))
            throw std::runtime_error("旧自动备份清理失败");
    }
}
