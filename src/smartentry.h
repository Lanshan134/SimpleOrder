#pragma once
#include <QtWidgets>
#include <functional>

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
void smartEntryParserTests();
class SmartEntry : public QPlainTextEdit {
    Q_OBJECT
    std::function<bool(const QString &)> submit;
    void updatePreview();
    bool composing = false;

  protected:
    void keyPressEvent(QKeyEvent *) override;
    void inputMethodEvent(QInputMethodEvent *) override;
    void changeEvent(QEvent *) override;
    void insertFromMimeData(const QMimeData *) override;

  public:
    explicit SmartEntry(QWidget *parent = nullptr);
    void onSubmit(std::function<bool(const QString &)> fn) {
        submit = fn;
    }
    void submitEntry();
};
