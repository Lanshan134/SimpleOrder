#pragma once
#include "storage/store.h"
#include "ui/theme/appearance.h"
#include "ui/widgets/calendar.h"
#include "ui/widgets/planeditor.h"
#include "ui/widgets/tasklist.h"
#include "ui/windows/window.h"
#include <QtWidgets>
#include <functional>

class SettingsDialog;

class Controller : public QObject {
    friend class SettingsDialog;
    Store &store;
    QSettings settings;
    const bool systemStartupEnabled;
    bool testing = false, refreshing = false, pinExpand = false, planExpand = false, dirty = false,
         calendarView = false;
    qint64 selected = 0;
    QDate day = QDate::currentDate();
    QList<Item> items;
    Window *floating = nullptr, *manager = nullptr;
    QStackedWidget *stack = nullptr;
    TaskList *pins = nullptr, *tasks = nullptr, *plans = nullptr, *all = nullptr, *dayItems = nullptr,
             *floatDay = nullptr;
    QPushButton *pinMore = nullptr, *planMore = nullptr, *switchButton = nullptr, *saveButton = nullptr,
                *deleteButton = nullptr;
    QWidget *editorHeader = nullptr;
    QCheckBox *showHistory = nullptr;
    Calendar *cal = nullptr, *floatCal = nullptr;
    QLabel *dateLabel = nullptr, *status = nullptr, *floatDate = nullptr, *pinHeading = nullptr,
           *taskHeading = nullptr, *planHeading = nullptr;
    QWidget *editor = nullptr;
    // The editor panel in the manager opens only while an item is being edited.
    QSplitter *managerSplit = nullptr;
    QFrame *inspector = nullptr;
    QPushButton *closeEditorButton = nullptr;
    void openEditor();
    void closeEditor();
    void scaleManager();
    QLineEdit *search = nullptr, *title = nullptr, *date = nullptr, *time = nullptr;
    QComboBox *group = nullptr, *type = nullptr, *timeKind = nullptr;
    QCheckBox *pinned = nullptr, *done = nullptr, *showCalendar = nullptr;
    QSpinBox *progress = nullptr, *target = nullptr;
    QPlainTextEdit *notes = nullptr;
    QTabBar *editorTabs = nullptr;
    QJsonObject editorBaseline;
    bool dateManuallyEdited = false;
    bool parseEditorTitle();
    void smartEntrySmokeTest(const QString &);
    bool dateFieldActive = false, calendarDateEditPending = false;
    int dateFocusGeneration = 0;
    bool eventFilter(QObject *, QEvent *) override;
    QPointer<QDialog> itemWindow;
    QJsonObject editorContents() const;
    bool hasEditorChanges() const;
    PlanEditor *planEditor = nullptr;
    EditorRow *typeRuleRow = nullptr;
    QWidget *taskDateBox = nullptr, *taskTimeBox = nullptr;
    void updateEditorMode();
    QSystemTrayIcon *tray = nullptr;
    QPointer<SettingsDialog> settingsWindow;
    bool settingsPreview = false;
    void setSettingsPreview(bool enabled);
    void buildFloating();
    struct LayoutMetrics {
        QLayout *layout;
        QMargins margins;
        int spacing;
    };
    QList<LayoutMetrics> floatingLayouts;
    qreal floatingScale = 1.0;
    bool scalingFloating = false;
    QString floatingBaseStyle;
    void setupFloatingScaling();
    void scaleFloating(bool force = false);
    void buildManager();
    void fillEditor();
    void updateLists();
    void bindList(TaskList *);
    void refresh();
    void choose(qint64);
    void editFloatingItem(qint64);
    void setDate(QDate);
    bool leaveEditor();
    bool saveEditor();
    void complete(qint64, QDate occurrence = {});
    void schedule(qint64, QDate);
    bool add(const QString &, QDate = {});
    void settingsDialog();
    void layoutSmokeTest(const QString &directory);
    bool settingsSmokeTest(const QString &directory);
    void editorSmokeTest(const QString &directory);
    void draftSmokeTest(const QString &directory);
    void calendarEditorSmokeTest();
    void applyPreferences();
    bool visibleItem(const Item &) const;
    QList<Item> sortedItems(QList<Item>) const;
    void maybeAutoBackup(bool force = false);
    void closeWindow(bool floatingWindow);
    void reloadCategories();
    void applyFloatingAppearance();
    void persistGeometry();
    void guard(const std::function<void()> &);

  public:
    Controller(Store &, const QString &settingsPath, bool testing = false, bool separateData = false);
    ~Controller();
    void show();
    void setTop(bool);
    bool smokeTest(const QString &directory);
};
