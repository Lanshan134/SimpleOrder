#include "domain/item.h"

bool visibleInActive(const Item &item, const QDateTime &now, int retentionHours) {
    if (!item.done)
        return true;
    if (retentionHours < 0)
        return true;
    const auto at = QDateTime::fromString(item.completedAt, Qt::ISODateWithMs);
    return at.isValid() && at.secsTo(now) < qint64(retentionHours) * 60 * 60;
}
