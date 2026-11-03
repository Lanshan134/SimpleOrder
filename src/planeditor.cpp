#include "planeditor.h"
namespace {
QComboBox *combo(const QStringList &labels, const QStringList &values) {
    auto box = new QComboBox;
    for (int n = 0; n < labels.size(); ++n)
        box->addItem(labels[n], values[n]);
    return box;
}
QSpinBox *number() {
    auto n = new QSpinBox;
    n->setRange(1, 9999);
    n->setSuffix(" 次");
    return n;
}
} // namespace
PlanEditor::PlanEditor(QWidget *parent) : QWidget(parent) {
    setObjectName("planEditor");
    auto root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);
    ruleType =
        combo({"周期次数", "指定日期", "间隔重复", "累计目标"}, {"quota", "dates", "interval", "total"});
    ruleType->setObjectName("planRule");
    ruleType->setParent(this);
    pages = new QStackedWidget;
    root->addWidget(pages);
    auto quota = new QWidget;
    auto q = new QVBoxLayout(quota);
    q->setContentsMargins(0, 0, 0, 0);
    period = combo({"每天", "每周", "每月"}, {"day", "week", "month"});
    period->setCurrentIndex(1);

    target = number();
    target->setObjectName("quotaTarget");
    q->addWidget(new EditorRow(editorField("计数周期", period), editorField("目标次数", target)));
    pages->addWidget(quota);

    auto dates = new QWidget;
    auto d = new QVBoxLayout(dates);
    d->setContentsMargins(0, 0, 0, 0);
    dateMode = combo({"按周", "按月"}, {"week", "month"});
    dateMode->setParent(dates);
    dateMode->hide();
    auto modeTabs = new QTabBar;
    modeTabs->setObjectName("dateModeTabs");
    modeTabs->addTab("按周");
    modeTabs->addTab("按月");
    modeTabs->setExpanding(false);
    d->addWidget(modeTabs, 0, Qt::AlignLeft);
    connect(modeTabs, &QTabBar::currentChanged, dateMode, &QComboBox::setCurrentIndex);
    connect(dateMode, QOverload<int>::of(&QComboBox::currentIndexChanged), modeTabs,
            &QTabBar::setCurrentIndex);
    dayPages = new QStackedWidget;
    d->addWidget(dayPages);
    for (int mode = 0; mode < 2; ++mode) {
        auto panel = new QWidget;
        auto grid = new QGridLayout(panel);
        grid->setContentsMargins(0, 3, 0, 3);
        grid->setSpacing(4);
        int count = mode == 0 ? 7 : 32;
        for (int n = 0; n < count; ++n) {
            QString label = mode == 0 ? QStringList{"一", "二", "三", "四", "五", "六", "日"}[n]
                                      : (n == 31 ? "月末" : QString::number(n + 1));
            auto b = new QPushButton(label);
            b->setCheckable(true);
            b->setMinimumHeight(32);
            b->setObjectName("dayChoice");
            b->setAccessibleName(mode == 0 ? "星期" + label : label + (n == 31 ? "" : "日"));
            grid->addWidget(b, n / 7, n % 7);
            (mode == 0 ? weekDays : monthDays).append(b);
            connect(b, &QPushButton::toggled, this, [this] {
                updateView();
                if (changed)
                    changed();
            });
        }
        dayPages->addWidget(panel);
    }
    weekDays[QDate::currentDate().dayOfWeek() - 1]->setChecked(true);
    monthDays[QDate::currentDate().day() - 1]->setChecked(true);
    missing = combo({"跳过该次", "改为当月最后一天"}, {"skip", "last"});
    missingBox = editorField("当月没有所选日期时", missing);
    d->addWidget(missingBox);
    pages->addWidget(dates);

    auto intervals = new QWidget;
    auto in = new QVBoxLayout(intervals);
    in->setContentsMargins(0, 0, 0, 0);
    interval = number();
    interval->setSuffix(" 天");

    basis = combo({"固定日期", "完成后顺延"}, {"fixed", "after"});
    in->addWidget(new EditorRow(editorField("重复间隔", interval), editorField("计算方式", basis)));
    first = new QLineEdit(QDate::currentDate().toString(Qt::ISODate));
    first->setPlaceholderText("YYYY-MM-DD");
    first->setClearButtonEnabled(true);

    pages->addWidget(intervals);

    auto total = new QWidget;
    auto t = new QVBoxLayout(total);
    t->setContentsMargins(0, 0, 0, 0);
    totalTarget = number();
    totalTarget->setObjectName("totalTarget");
    t->addWidget(editorField("目标次数", totalTarget));
    pages->addWidget(total);
    start = new QLineEdit;
    end = new QLineEdit;
    clock = new QLineEdit;
    for (auto date : {start, end}) {
        date->setPlaceholderText("不限");
        date->setToolTip("YYYY-MM-DD；留空表示不限。");
        date->setClearButtonEnabled(true);
    }
    startPages = new QStackedWidget;
    startPages->addWidget(editorField("开始日期", start));
    startPages->addWidget(editorField("首次日期（必填）", first));
    root->addWidget(new EditorRow(startPages, editorField("结束日期", end)));
    clock->setPlaceholderText("不指定");
    clock->setToolTip("HH:mm；可留空，仅作记录。");
    clock->setClearButtonEnabled(true);
    clockBox = new EditorRow(editorField("时间", clock), nullptr);
    root->addWidget(clockBox);
    dateHint = new QLabel;
    dateHint->setWordWrap(true);
    dateHint->setObjectName("hint");
    dateHint->hide();
    preview = new QLabel;
    preview->setWordWrap(true);
    preview->setObjectName("rulePreview");
    root->addWidget(preview);
    for (auto c : {ruleType, period, dateMode, missing, basis})
        connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
            updateView();
            if (changed)
                changed();
        });
    for (auto n : {target, totalTarget, interval})
        connect(n, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] {
            updateView();
            if (changed)
                changed();
        });
    for (auto e : {start, end, first, clock})
        connect(e, &QLineEdit::textChanged, this, [this] {
            updateView();
            if (changed)
                changed();
        });
    updateView();
}
QJsonObject PlanEditor::rule() const {
    auto k = ruleType->currentData().toString();
    QJsonObject r{{"kind", k},
                  {"start", k == "interval" ? QString() : start->text().trimmed()},
                  {"end", end->text().trimmed()}};
    if (k == "quota")
        r.insert("period", period->currentData().toString());
    if (k == "dates") {
        r.insert("mode", dateMode->currentData().toString());
        r.insert("missing", missing->currentData().toString());
        QJsonArray days;
        const auto buttons = dateMode->currentIndex() == 0 ? weekDays : monthDays;
        for (int n = 0; n < buttons.size(); ++n)
            if (buttons[n]->isChecked())
                days.append(n == 31 ? 0 : n + 1);
        r.insert("days", days);
    }
    if (k == "interval") {
        r.insert("first", first->text().trimmed());
        r.insert("interval", interval->value());
        r.insert("basis", basis->currentData().toString());
    }
    return r;
}
int PlanEditor::goal() const {
    auto k = ruleType->currentData().toString();
    return k == "quota" ? target->value() : k == "total" ? totalTarget->value() : 1;
}
void PlanEditor::selectKind(const QString &k) {
    ruleType->setCurrentIndex(qMax(0, ruleType->findData(k)));
}
void PlanEditor::load(const Item &i) {
    auto r = i.rule;
    selectKind(r.value("kind").toString("total"));
    period->setCurrentIndex(qMax(0, period->findData(r.value("period").toString("week"))));
    target->setValue(i.target);
    totalTarget->setValue(i.target);
    dateMode->setCurrentIndex(r.value("mode").toString("week") == "week" ? 0 : 1);
    missing->setCurrentIndex(r.value("missing").toString() == "last" ? 1 : 0);
    basis->setCurrentIndex(r.value("basis").toString() == "after" ? 1 : 0);
    interval->setValue(r.value("interval").toInt(1));
    first->setText(r.value("first").toString(QDate::currentDate().toString(Qt::ISODate)));
    start->setText(r.value("start").toString());
    end->setText(r.value("end").toString());
    clock->setText(i.time);
    auto days = r.value("days").toArray();
    for (int n = 0; n < weekDays.size(); ++n)
        weekDays[n]->setChecked(days.isEmpty() ? n == QDate::currentDate().dayOfWeek() - 1
                                               : days.contains(n + 1));
    for (int n = 0; n < monthDays.size(); ++n)
        monthDays[n]->setChecked(days.isEmpty() ? n == QDate::currentDate().day() - 1
                                                : days.contains(n == 31 ? 0 : n + 1));
    updateView();
}
void PlanEditor::updateView() {
    // Constructors set initial selections before all controls exist.
    if (!preview)
        return;
    auto k = ruleType->currentData().toString();
    pages->setCurrentIndex(ruleType->currentIndex());
    for (int n = 0; n < pages->count(); ++n)
        pages->widget(n)->setSizePolicy(QSizePolicy::Preferred, n == pages->currentIndex()
                                                                    ? QSizePolicy::Preferred
                                                                    : QSizePolicy::Ignored);
    dayPages->setCurrentIndex(dateMode->currentIndex());
    for (int n = 0; n < dayPages->count(); ++n)
        dayPages->widget(n)->setSizePolicy(QSizePolicy::Preferred, n == dayPages->currentIndex()
                                                                       ? QSizePolicy::Preferred
                                                                       : QSizePolicy::Ignored);
    bool high = false;
    for (int n = 28; n < 31; ++n)
        high |= monthDays[n]->isChecked();
    missingBox->setVisible(k == "dates" && dateMode->currentIndex() == 1 && high);
    startPages->setCurrentIndex(k == "interval" ? 1 : 0);
    clockBox->setVisible(k != "total");
    QString text;
    if (k == "quota")
        text = period->currentText() + "完成 " + QString::number(goal()) + " 次";
    if (k == "dates") {
        QStringList labels;
        auto buttons = dateMode->currentIndex() == 0 ? weekDays : monthDays;
        for (auto b : buttons)
            if (b->isChecked())
                labels << b->text();
        text = (dateMode->currentIndex() == 0 ? "每周：" : "每月：") + labels.join("、");
    }
    if (k == "interval")
        text = QString("每隔 %1 天 · %2").arg(interval->value()).arg(basis->currentText());
    if (k == "total")
        text = QString("累计完成 %1 次").arg(goal());
    dateHint->setText(k == "quota"   ? "每天零点、每周一或每月 1 日开始新周期，历史计数保留。"
                      : k == "dates" ? "每个执行日单独勾选完成。"
                      : k == "interval"
                          ? (basis->currentIndex() == 0 ? "按首次日期推算，逾期不会改变之后的日期。"
                                                        : "完成后再间隔指定天数；未完成时保持待办。")
                          : "日历只标记结束日期；未设置结束日期时不显示日历标记。");
    Item sample;
    sample.type = "plan";
    sample.rule = rule();
    sample.target = goal();
    try {
        validatePlan(sample);
        if (k != "total") {
            auto candidate = QDate::currentDate();
            auto earliest =
                QDate::fromString(rule().value(k == "interval" ? "first" : "start").toString(), Qt::ISODate);
            if (earliest.isValid() && earliest > candidate)
                candidate = earliest;
            for (int n = 0; n < 3660; ++n, candidate = candidate.addDays(1))
                if (planOccurs(sample, candidate)) {
                    text += " · 下次：" + candidate.toString("yyyy-MM-dd ddd");
                    break;
                }
        }
        if (k != "total" && !clock->text().trimmed().isEmpty())
            text += " · " + clock->text().trimmed();
        ruleType->setToolTip(text + "\n" + dateHint->text());
        preview->clear();
        preview->hide();
    } catch (const std::exception &e) {
        preview->setText(QString::fromUtf8(e.what()));
        preview->show();
    }
}
