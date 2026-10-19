#pragma once
#include "planeditor.h"
#include "store.h"
#include <QtWidgets>
#include <functional>

QColor groupColor(const QString &group);
QString groupName(const QString &group);
class SettingsDialog;
class TaskList : public QListWidget {
    Qt::MouseButton pressedButton = Qt::LeftButton;

  public:
    std::function<void(qint64)> edit, contextEdit, complete, togglePin;
    QMenu *createItemMenu(qint64 id);
    explicit TaskList(QWidget *parent = nullptr);
    void fill(const QList<Item> &items, bool headings = false);
    void setPreviewOnly(bool enabled);

  protected:
    void mousePressEvent(QMouseEvent *event) override {
        pressedButton = event->button();
        QListWidget::mousePressEvent(event);
    }
    void startDrag(Qt::DropActions) override;
    void wheelEvent(QWheelEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
};
class Calendar : public QCalendarWidget {
  public:
    bool showCompleted = true, displayOutside = true;
    bool previewOnly = false;
    int retentionHours = 48, maxDots = 3;
    QList<Item> items;
    std::function<void(qint64, QDate)> schedule;
    explicit Calendar(QWidget *parent = nullptr);
    void repaintCells() {
        updateCells();
    }

  protected:
    bool eventFilter(QObject *, QEvent *) override;
    void paintCell(QPainter *, const QRect &, const QDate &) const override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dropEvent(QDropEvent *) override;
    QDate dateAt(const QPoint &p) const;
};
class Window : public QWidget {
    QPoint dragOffset;
    bool dragging = false;

  public:
    bool customSurface = false;
    qreal backgroundOpacity = 1.0;
    std::function<void()> onResize;
    void setDragHandle(QWidget *handle);
    std::function<void()> onClose;
    explicit Window(Qt::WindowFlags flags = Qt::Window) : QWidget(nullptr, flags) {}

  protected:
    bool eventFilter(QObject *, QEvent *) override;
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *e) override {
        if (onClose) {
            e->ignore();
            onClose();
        } else
            QWidget::closeEvent(e);
    }
};
class Controller : public QObject {
    friend class SettingsDialog;
    Store &store;
    QSettings settings;
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
    QLabel *dateLabel = nullptr, *status = nullptr;
    QWidget *editor = nullptr;
    QLineEdit *search = nullptr, *title = nullptr, *date = nullptr, *time = nullptr;
    QComboBox *group = nullptr, *type = nullptr, *timeKind = nullptr;
    QCheckBox *pinned = nullptr, *done = nullptr, *showCalendar = nullptr;
    QSpinBox *progress = nullptr, *target = nullptr;
    QPlainTextEdit *notes = nullptr;
    QTabBar *editorTabs = nullptr;
    QJsonObject editorBaseline;
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
    void add(const QString &, QDate = {});
    void settingsDialog();
    bool settingsSmokeTest(const QString &directory);
    void editorSmokeTest(const QString &directory);
    void draftSmokeTest(const QString &directory);
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
    Controller(Store &, const QString &settingsPath, bool testing = false);
    ~Controller();
    void show();
    void setTop(bool);
    bool smokeTest(const QString &directory);
};
