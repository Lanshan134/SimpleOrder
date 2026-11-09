#include "app/controller.h"
#include "ui/windows/settings.h"
#include "ui/widgets/smartentry.h"
#include "ui/widgets/widgettools.h"
#include <algorithm>

void Controller::buildFloating() {
    floating = new Window(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint |
                          Qt::WindowTitleHint);
    floating->setObjectName("floatingWindow");
    floating->setWindowTitle("简序 · 悬浮窗");
    floating->resize(floatingSize(420, 480));
    floating->customSurface = true;
    floating->setAttribute(Qt::WA_TranslucentBackground);
    floating->setMinimumSize(floatingSize(390, 340));
    auto grip = new QSizeGrip(floating);
    grip->setFixedSize(16, 16);
    grip->setToolTip("拖动调整悬浮窗大小");
    auto outer = new QVBoxLayout(floating);
    outer->setContentsMargins(12, 10, 12, 10);
    auto titleBar = new QWidget;
    auto head = new QHBoxLayout(titleBar);
    head->setContentsMargins(0, 0, 0, 0);
    auto label = new QLabel("任务");
    label->setObjectName("windowTitle");
    head->addWidget(label);
    floatDate = new QLabel;
    floatDate->setObjectName("floatingDate");
    head->addWidget(floatDate);
    head->addStretch();
    switchButton = button("日历");
    auto manage = button("管理");
    manage->setObjectName("openManager");
    manage->setToolTip("打开完整管理窗口");
    auto config = button("设置");
    head->addWidget(switchButton);
    head->addWidget(manage);
    head->addWidget(config);
    outer->addWidget(titleBar);
    floating->setDragHandle(titleBar);
    stack = new QStackedWidget;
    stack->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Ignored);
    outer->addWidget(stack);
    auto listPage = new QWidget;
    auto lv = new QVBoxLayout(listPage);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(0);
    auto section = [&](const QString &s, QLabel *&title, TaskList *&list, QPushButton *&more) {
        auto h = new QHBoxLayout;
        title = heading(s, "sectionHeading");
        h->addWidget(title);
        h->addStretch();
        more = button("", "expandButton");
        more->setFlat(true);
        h->addWidget(more);
        lv->addLayout(h);
        list = new TaskList;
        list->setAccessibleName(s + "事项");
        list->setProperty("alignChecks", true);
        bindList(list);
        lv->addWidget(list);
    };
    section("置顶", pinHeading, pins, pinMore);
    taskHeading = heading("任务", "sectionHeading");
    lv->addWidget(taskHeading);
    tasks = new TaskList;
    tasks->setAccessibleName("任务");
    tasks->setProperty("alignChecks", true);
    tasks->setProperty("emptyText", "没有任务");
    // The configured rows when they fit, more when the window has room, and fewer when it is short,
    // so the page itself rarely scrolls and no band sits empty above quick add.
    tasks->setMinimumHeight(ROW_HEIGHT);
    tasks->setProperty("preferredRows", 5);
    bindList(tasks);
    lv->addWidget(tasks, 1);
    section("计划", planHeading, plans, planMore);
    auto listScroll = new QScrollArea;
    listScroll->setObjectName("floatingScroll");
    listScroll->setFrameShape(QFrame::NoFrame);
    listScroll->setWidgetResizable(true);
    listScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listScroll->setWidget(listPage);
    // Quick add stays anchored at the bottom of the window, however long the lists are.
    auto listHost = new QWidget;
    auto hostLayout = new QVBoxLayout(listHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);
    hostLayout->addWidget(listScroll, 1);
    hostLayout->addWidget(quickBar("添加事项", [this](QString t) {
        bool created = false;
        guard([&] { created = add(t); });
        return created;
    }));
    stack->addWidget(listHost);
    auto calendarPage = new QWidget;
    auto cv = new QVBoxLayout(calendarPage);
    cv->setContentsMargins(0, 0, 0, 0);
    auto calendarContent = new QWidget;
    auto calendarLayout = new QVBoxLayout(calendarContent);
    calendarLayout->setContentsMargins(0, 0, 0, 0);
    calendarLayout->setSpacing(6);
    calendarLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    floatCal = new Calendar;
    calendarLayout->addWidget(floatCal, 1);
    floatDay = new TaskList;
    floatDay->setAccessibleName("选中日期的事项");
    floatDay->setProperty("dayList", true);
    floatDay->setProperty("emptyText", "这一天没有事项");
    floatDay->setFixedHeight(4 * ROW_HEIGHT);
    bindList(floatDay);
    calendarLayout->addWidget(floatDay);
    // The stack may be shorter than the calendar's minimum plus its day list.
    // Scroll that content instead of forcing overlapping minimum-size children.
    // Keep quick entry outside the scroller so it remains reachable.
    auto calendarScroll = new QScrollArea;
    calendarScroll->setObjectName("floatingCalendarScroll");
    calendarScroll->setFrameShape(QFrame::NoFrame);
    calendarScroll->setWidgetResizable(true);
    calendarScroll->setWidget(calendarContent);
    cv->addWidget(calendarScroll, 1);
    cv->addWidget(quickBar("添加到选中日期", [this](QString t) {
        bool created = false;
        guard([&] { created = add(t, day); });
        return created;
    }));
    stack->addWidget(calendarPage);

    connect(switchButton, &QPushButton::clicked, this, [this, label] {
        calendarView = !calendarView;
        stack->setCurrentIndex(calendarView ? 1 : 0);
        label->setText(calendarView ? "日历" : "任务");
        switchButton->setText(calendarView ? "列表" : "日历");
    });
    connect(manage, &QPushButton::clicked, this, [this] {
        manager->show();
        manager->raise();
        manager->activateWindow();
    });
    connect(config, &QPushButton::clicked, this, [this] { settingsDialog(); });

    connect(pinMore, &QPushButton::clicked, this, [this] {
        pinExpand = !pinExpand;
        updateLists();
    });
    connect(planMore, &QPushButton::clicked, this, [this] {
        planExpand = !planExpand;
        updateLists();
    });

    floatCal->schedule = [this](qint64 id, QDate d) { guard([&] { schedule(id, d); }); };
    // Start focus in the list rather than ringing the first button in the window.
    tasks->setFocus();
}
void Controller::setupFloatingScaling() {
    // Text size is folded into the floating factor, so start from the unscaled sheet.
    floatingBaseStyle = theme::baseStyle;
    floatingBaseStyle.replace(QRegularExpression("([0-9]+)tx"), "\\1px");
    for (auto layout : floating->findChildren<QLayout *>())
        floatingLayouts.append({layout, layout->contentsMargins(), layout->spacing()});
    floating->setProperty("uiScale", 1.0);
    // Restyling on every resize step makes drags stutter; coalesce them. Tests stay synchronous.
    auto rescale = new QTimer(this);
    rescale->setSingleShot(true);
    rescale->setInterval(40);
    connect(rescale, &QTimer::timeout, this, [this] { scaleFloating(); });
    floating->onResize = [this, rescale] {
        if (testing)
            scaleFloating();
        else
            rescale->start();
    };
    scaleFloating();
}
void Controller::scaleFloating(bool force) {
    if (scalingFloating)
        return;
    // The area-derived factor responds to either resize axis and applies uniformly
    // to text and geometry. Always scale from the original metrics, never cumulatively.
    // Windows text size enlarges the default window, so with window scaling the size carries it.
    const qreal font = settings.value("fontSize", 15).toDouble() / 15.0;
    const qreal factor =
        qBound(0.7,
               settings.value("scaleWithWindow", true).toBool()
                   ? font * qSqrt(qreal(floating->width()) * floating->height() / (420.0 * 480.0))
                   : font * theme::textScale,
               3.0);
    if (!force && qAbs(factor - floatingScale) < 0.005)
        return;
    scalingFloating = true;
    const qreal previous = floatingScale;
    floatingScale = factor;
    floating->setProperty("uiScale", factor);
    QString style;
    int offset = 0;
    QRegularExpression pixels("([0-9]+(?:\\.[0-9]+)?)px");
    auto matches = pixels.globalMatch(floatingBaseStyle);
    while (matches.hasNext()) {
        auto match = matches.next();
        style += floatingBaseStyle.mid(offset, match.capturedStart() - offset);
        style += QString::number(qMax(1, qRound(match.captured(1).toDouble() * factor))) + "px";
        offset = match.capturedEnd();
    }
    style += floatingBaseStyle.mid(offset);
    const auto recolor = [&](QString text, const QRegularExpression &pattern, qreal alpha) {
        QString out;
        int position = 0;
        auto matches = pattern.globalMatch(text);
        while (matches.hasNext()) {
            auto match = matches.next();
            out += text.mid(position, match.capturedStart() - position);
            QColor color(match.captured(2));
            out += match.captured(1) + QString("rgba(%1,%2,%3,%4)")
                                           .arg(color.red())
                                           .arg(color.green())
                                           .arg(color.blue())
                                           .arg(qRound(alpha * 255));
            position = match.capturedEnd();
        }
        return out + text.mid(position);
    };
    style = recolor(
        style, QRegularExpression("((?:background|background-color)\\s*:\\s*)(#[a-fA-F0-9]{6}|white)\\b"),
        floating->backgroundOpacity);
    style = recolor(
        style, QRegularExpression("(?<![a-zA-Z-])((?:selection-)?color\\s*:\\s*)(#[a-fA-F0-9]{6}|white)\\b"),
        floating->property("foregroundOpacity").isValid() ? floating->property("foregroundOpacity").toDouble()
                                                          : 1.0);
    // Control borders fade with the text. Quick add is the window's main action: its fill ignores the
    // see-through background and stops fading at 70%, so it never reads as disabled.
    const qreal textAlpha = floating->property("foregroundOpacity").isValid()
                                ? floating->property("foregroundOpacity").toDouble()
                                : 1.0;
    style = recolor(style, QRegularExpression("(border(?:-[a-z]+)?\\s*:\\s*[^;{}#]*)(#[a-fA-F0-9]{6})\\b"),
                    textAlpha);
    const auto fill = [textAlpha](const char *token) {
        const QColor color = theme::color(token);
        return QString("rgba(%1,%2,%3,%4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(qRound(qMax(textAlpha, 0.7) * 255));
    };
    style += QString("QPushButton#quickAdd{background:%1;}QPushButton#quickAdd:hover{background:%2;}"
                     "QPushButton#quickAdd:disabled{background:%3;}")
                 .arg(fill("accent"), fill("accent-hover"), fill("control-off"));
    floating->setStyleSheet(style);
    // Polishing resets each input's palette, so the placeholder's opacity goes on afterwards.
    for (auto input : floating->findChildren<QLineEdit *>()) {
        auto palette = input->palette();
        palette.setColor(QPalette::PlaceholderText, foregroundColor(floating, theme::color("ink-soft")));
        input->setPalette(palette);
    }
    auto px = [factor](int value) { return qRound(value * factor); };
    for (auto metrics : floatingLayouts) {
        auto m = metrics.margins;
        metrics.layout->setContentsMargins(px(m.left()), px(m.top()), px(m.right()), px(m.bottom()));
        if (metrics.spacing >= 0)
            metrics.layout->setSpacing(px(metrics.spacing));
    }
    for (auto input : floating->findChildren<SmartEntry *>()) {
        input->setFixedHeight(px(36));
        auto palette = input->palette();
        palette.setColor(QPalette::PlaceholderText, foregroundColor(floating, theme::color("ink-soft")));
        input->setPalette(palette);
    }
    const int row = scaled(floating, ROW_HEIGHT);
    tasks->setMinimumHeight(row);
    tasks->setProperty("preferredRows", settings.value("taskRows", 5).toInt());
    tasks->updateGeometry();
    floatDay->setFixedHeight(4 * row);
    for (auto list : {pins, plans})
        list->setFixedHeight(qMax(1, qMin(6, list->count())) * row);
    for (auto list : {pins, tasks, plans, floatDay}) {
        const int position = list->verticalScrollBar()->value();
        list->setMinimumWidth(px(260));
        list->verticalScrollBar()->setSingleStep(row);
        list->doItemsLayout();
        list->verticalScrollBar()->setValue(qRound(position * factor / previous));
        list->viewport()->update();
    }
    floatCal->setMinimumSize(px(330), px(250));
    floatCal->repaintCells();
    if (auto grip = floating->findChild<QSizeGrip *>())
        grip->setFixedSize(px(16), px(16));
    tasks->snapToRows();
    floating->update();
    scalingFloating = false;
}
void Controller::applyFloatingAppearance() {
    bool visible = floating->isVisible();
    floating->setWindowFlag(Qt::FramelessWindowHint, settings.value("frameless", true).toBool());
    floating->setWindowOpacity(1.0);
    floating->backgroundOpacity = qBound(0, settings.value("opacity", 100).toInt(), 100) / 100.0;
    floating->setProperty("surfaceOpacity", floating->backgroundOpacity);
    floating->setProperty("foregroundOpacity",
                          qBound(MIN_TEXT_OPACITY, settings.value("textOpacity", 100).toInt(), 100) / 100.0);
    // Only weekends carry a format, as on the manager calendar, so weekday headers keep the muted header ink.
    for (int weekday = 6; weekday <= 7; ++weekday) {
        QTextCharFormat format;
        format.setForeground(foregroundColor(floatCal, theme::color("ink-soft")));
        floatCal->setWeekdayTextFormat(Qt::DayOfWeek(weekday), format);
    }
    auto headerFormat = floatCal->headerTextFormat();
    headerFormat.setForeground(foregroundColor(floatCal, theme::color("ink-muted")));
    floatCal->setHeaderTextFormat(headerFormat);
    for (auto input : floating->findChildren<QLineEdit *>()) {
        auto palette = input->palette();
        palette.setColor(QPalette::PlaceholderText, foregroundColor(floating, theme::color("ink-soft")));
        input->setPalette(palette);
    }
    if (auto grip = floating->findChild<QSizeGrip *>())
        grip->setVisible(settings.value("frameless", true).toBool());
    if (visible)
        floating->show();
    scaleFloating(true);
    floating->update();
    settings.sync();
}
