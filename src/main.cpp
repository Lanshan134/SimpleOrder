#include "ui.h"
#include <iostream>

int main(int argc, char **argv) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);
    app.setApplicationName("LightTask");
    app.setOrganizationName("LightTaskLocal");
    // Resolve the legacy storage path before adopting the new application identity.
    const auto existingDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    app.setApplicationName("SimpleOrder");
    app.setApplicationDisplayName("简序 · SimpleOrder");
    app.setApplicationVersion("0.2.0");
    app.setFont(QFont("Microsoft YaHei UI", 11));
    QFile style(QCoreApplication::applicationDirPath() + "/style.qss");
    if (style.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(style.readAll()));
    bool test = app.arguments().contains("--self-test");
    QString dataDir;
    if (test) {
        dataDir = QDir::currentPath() + "/test-results/run-" +
                  QDateTime::currentDateTime().toString("yyyyMMdd-HHmmsszzz");
    } else
        dataDir = existingDataDir;
    QDir().mkpath(dataDir);
    QLockFile lock(dataDir + "/app.lock");
    if (!test && !lock.tryLock(100)) {
        QMessageBox::information(nullptr, "简序", "已有一个简序实例正在运行，请检查任务栏或系统托盘。");
        return 0;
    }
    try {
        Store store(dataDir + "/tasks.sqlite");
        if (!QFile::exists(dataDir + "/settings.ini"))
            store.seed();
        Controller controller(store, dataDir + "/settings.ini", test);
        if (test) {
            bool ok = controller.smokeTest(dataDir);
            QFile report(dataDir + "/result.txt");
            report.open(QIODevice::WriteOnly);
            report.write(
                ok ? "PASS: wheel scrolling and position retention, plan expand, 48-hour completion "
                     "retention and history, checkmark progress, date detail creation, context pin, "
                     "custom categories, frameless and opacity, manager entry, SQLite migration backup, "
                     "calendar drop, backup roundtrip and rollback, task/plan forms, daily/weekly/monthly "
                     "quotas, "
                     "selected dates, month end, fixed/relative interval, cumulative counts, rule backup, "
                     "window rendering\n"
                   : "FAIL\n");
            std::cout << (ok ? "PASS " : "FAIL ") << dataDir.toStdString() << std::endl;
            return ok ? 0 : 1;
        }
        controller.show();
        return app.exec();
    } catch (const std::exception &e) {
        if (test)
            std::cerr << e.what() << std::endl;
        else
            QMessageBox::critical(nullptr, "简序无法启动", QString::fromUtf8(e.what()));
        return 2;
    }
}
