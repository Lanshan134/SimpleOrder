#pragma once
#include "editorwidgets.h"
#include "store.h"
#include <QtWidgets>

class PlanEditor : public QWidget {
    QComboBox *ruleType, *period, *dateMode, *missing, *basis;
    QStackedWidget *pages, *dayPages;
    QSpinBox *interval;
    QLineEdit *start, *end, *first, *clock;
    QWidget *clockBox, *missingBox;
    QStackedWidget *startPages;
    QLabel *preview = nullptr, *dateHint = nullptr;
    QList<QPushButton *> weekDays, monthDays;
    std::function<void()> changed;
    void updateView();

  public:
    QSpinBox *target;
    QSpinBox *totalTarget;
    explicit PlanEditor(QWidget *parent = nullptr);
    void onChanged(std::function<void()> fn) {
        changed = fn;
    }
    QComboBox *selector() const {
        return ruleType;
    }
    void load(const Item &);
    QJsonObject rule() const;
    int goal() const;
    QString time() const {
        return clock->text().trimmed();
    }
    void selectKind(const QString &);
};
