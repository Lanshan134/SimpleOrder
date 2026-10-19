#pragma once
#include "ui.h"

class SettingsDialog : public QDialog {
    friend class Controller;
    Controller &c;
    QListWidget *navigation = nullptr;
    QStackedWidget *pages = nullptr;
    QLabel *saved = nullptr;
    QPushButton *reset = nullptr;
    QTableWidget *types = nullptr;
    QComboBox *defaultType = nullptr;
    QPointer<QColorDialog> typeColorPicker;
    QHash<QString, std::function<void()>> readers;
    QWidget *page(const QString &title, const QString &description);
    QFormLayout *section(QWidget *, const QString &);
    QCheckBox *toggle(QFormLayout *, const QString &, const QString &, bool);
    QSpinBox *number(QFormLayout *, const QString &, const QString &, int, int, int,
                     const QString &suffix = {});
    QComboBox *choices(QFormLayout *, const QString &, const QString &,
                       const QList<QPair<QString, QVariant>> &, QVariant);
    void commit(const QString &, const QVariant &);
    void floatingPage();
    void typePage();
    void displayPage();
    void calendarPage();
    void startupPage();
    void dataPage();
    void aboutPage();
    void populateTypes();
    void refreshDefaultTypes();
    void addType();
    bool saveTypeName(const QString &id, const QString &name);
    void activateTypeCell(int row, int column, bool alreadySelected);
    void chooseTypeColor(const QString &id);
    void deleteType();
    void moveType(int);
    void importData(bool fromBackup = false);
    void resetPage();

  public:
    explicit SettingsDialog(Controller &, QWidget *parent = nullptr);
};
