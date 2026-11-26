#include "ui/widgets/categorypopup.h"
#include "domain/entryparser.h"
#include "ui/theme/appearance.h"
#include "ui/widgets/widgettools.h"

CategoryPopup::CategoryPopup(QWidget *input, std::function<QString()> before,
                             std::function<void(int, const Category &)> accepted)
    : QObject(input), edit(input), textBeforeCursor(before), accept(accepted) {
    edit->installEventFilter(this);
}
void CategoryPopup::hide() {
    if (list)
        list->hide();
}
void CategoryPopup::choose(const QListWidgetItem *item) {
    const QString id = item->data(Qt::UserRole).toString();
    hide();
    for (const auto &category : currentCategories())
        if (category.id == id) {
            accept(tagStart, category);
            return;
        }
}
void CategoryPopup::refresh() {
    const QString text = textBeforeCursor();
    tagStart = text.size() - 1;
    while (tagStart >= 0 && !startsTag(text, tagStart))
        --tagStart;
    if (tagStart < 0 || tagStart != dismissed)
        dismissed = -1;
    typed = text.mid(tagStart + 1);
    const bool spaced = typed.contains(QRegularExpression("\\s"));
    // The exact name first, then names that start with the text, then names that contain it.
    QList<Category> found;
    bool complete = tagStart < 0 || dismissed >= 0;
    if (!complete)
        for (int rank = 0; rank < (spaced ? 2 : 3); ++rank)
            for (const auto &c : currentCategories()) {
                const int match = c.name.compare(typed, Qt::CaseInsensitive) == 0      ? 0
                                  : c.name.startsWith(typed, Qt::CaseInsensitive)      ? 1
                                  : c.name.contains(typed, Qt::CaseInsensitive)        ? 2
                                                                                       : 3;
                if (match == rank)
                    found << c;
                // A finished tag followed by a space needs no further suggestion.
                complete |= spaced && c.name.compare(typed.trimmed(), Qt::CaseInsensitive) == 0;
            }
    if (complete || found.isEmpty()) {
        hide();
        return;
    }
    auto window = edit->window();
    if (!list || list->parentWidget() != window) {
        delete list;
        list = new QListWidget(window);
        list->setObjectName("categoryPopup");
        list->setAccessibleName("类型候选");
        list->setFocusPolicy(Qt::NoFocus);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) { choose(item); });
        window->installEventFilter(this);
    }
    list->clear();
    for (const auto &c : found) {
        auto item = new QListWidgetItem(typeDot(c.color), c.name, list);
        item->setData(Qt::UserRole, c.id);
    }
    // Enter on a bare `#`, or after a space, still submits the entry.
    list->setCurrentRow(typed.isEmpty() || spaced ? -1 : 0);
    // Opaque whatever the window's own transparency, and sized from the input's text.
    const int px = QFontInfo(edit->font()).pixelSize();
    list->setIconSize(QSize(px + 2, px + 2));
    list->setStyleSheet(QString("QListWidget{background:%1;color:%2;border:1px solid %3;border-radius:6px;"
                                "padding:4px;font-size:%6px;}"
                                "QListWidget::item{height:%7px;border-radius:4px;padding:0 6px;}"
                                "QListWidget::item:hover{background:%4;}"
                                "QListWidget::item:selected{background:%5;color:%2;}")
                            .arg(theme::color("surface").name(), theme::color("ink").name(),
                                 theme::color("line-strong").name(), theme::color("row-hover").name(),
                                 theme::color("selection").name())
                            .arg(px)
                            .arg(px * 2));
    list->ensurePolished();
    const int rowHeight = qMax(px * 2, list->sizeHintForRow(0));
    // Below the input when a full list fits there, else on the roomier side; filtering never moves it across.
    const QPoint at = edit->mapTo(window, QPoint());
    const int below = at.y() + edit->height() + 4, roomBelow = window->height() - below, roomAbove = at.y() - 4;
    const bool under = roomBelow >= 6 * rowHeight + 10 || roomBelow >= roomAbove;
    const int height = qMin(qMin(6, found.size()) * rowHeight + 10, under ? roomBelow : roomAbove);
    list->setGeometry(at.x(), under ? below : roomAbove - height, qMin(px * 13, window->width() - 16), height);
    list->show();
    list->raise();
}
bool CategoryPopup::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::InputMethod)
        composing = !static_cast<QInputMethodEvent *>(event)->preeditString().isEmpty();
    if (!isOpen())
        return false;
    if (event->type() == QEvent::Resize || (watched == edit && (event->type() == QEvent::FocusOut ||
                                                                event->type() == QEvent::Hide)))
        hide();
    if (watched != edit || event->type() != QEvent::KeyPress || composing)
        return false;
    const int key = static_cast<QKeyEvent *>(event)->key();
    const auto item = list->currentItem();
    if (key == Qt::Key_Up || key == Qt::Key_Down) {
        list->setCurrentRow(qBound(0, list->currentRow() + (key == Qt::Key_Down ? 1 : -1), list->count() - 1));
        return true;
    }
    if (key == Qt::Key_Escape) {
        dismissed = tagStart;
        hide();
        return true;
    }
    const bool enter = key == Qt::Key_Return || key == Qt::Key_Enter;
    if (!enter && key != Qt::Key_Tab)
        return false;
    // With nothing left to complete, Enter submits and Tab moves on as usual.
    if (!item || (enter && item->text().compare(typed, Qt::CaseInsensitive) == 0)) {
        hide();
        return false;
    }
    choose(item);
    return true;
}
