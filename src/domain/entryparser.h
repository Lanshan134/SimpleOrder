#pragma once
#include "domain/item.h"

struct EntrySpan {
    int start, length;
};
struct ParsedEntry {
    QString title, error, group;
    QDate date;
    QTime time;
    QList<EntrySpan> spans;
};
// Whether the character at this position opens a `#` tag; not in `C#` or a link, for example.
bool startsTag(const QString &text, int at);
ParsedEntry parseEntry(const QString &text, const QDate &today = QDate::currentDate(),
                       const QList<Category> &categories = {});
