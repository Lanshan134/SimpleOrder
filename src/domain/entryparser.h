#pragma once
#include <QtCore>

struct EntrySpan {
    int start, length;
};
struct ParsedEntry {
    QString title, error;
    QDate date;
    QTime time;
    QList<EntrySpan> spans;
};
ParsedEntry parseEntry(const QString &text, const QDate &today = QDate::currentDate());
