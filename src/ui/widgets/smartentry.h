#pragma once
#include "domain/entryparser.h"
#include <QPlainTextEdit>
#include <functional>

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
