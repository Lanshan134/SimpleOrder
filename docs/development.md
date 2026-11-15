# SimpleOrder 开发指南

在 VS Code 中打开仓库根目录，使用 Microsoft C/C++ 扩展。推荐在 Windows 上使用 Qt 5.15.2 MinGW 64 位套件和匹配的 MinGW/GDB 8.1。

## 构建与调试

仓库通过本机目录定位工具链：

| 路径 | 内容 |
| --- | --- |
| `.local/qt/5.15.2/mingw81_64/` | Qt 安装目录，含 `bin/qmake.exe` |
| `.local/mingw/` | MinGW 安装目录，含 `bin/g++.exe`、`bin/gdb.exe`、`bin/mingw32-make.exe` |

可使用已有安装的目录联接。命令行也可通过 `-QtRoot` 和 `-MinGWBin` 给构建脚本传入路径。

1. **Ctrl+Shift+B** 执行 **SimpleOrder: Debug**，生成 `build/debug/bin/SimpleOrder.exe`。
2. 在“运行和调试”选择 **调试简序**，按 **F5**。调试版的任务和设置保存在 `build/debug/data/`，Windows 启动项设置在独立数据模式下禁用。
3. 选择 **调试隔离自检** 可在测试代码中打断点。自检生成独立的 `test-results/run-*` 目录。
4. 从任务列表运行 **SimpleOrder: Release**，生成 `outputs/SimpleOrder/SimpleOrder.exe`。

调试版应用关闭优化并保留 GDB 符号，复用 Qt Release 运行库。程序启动、保存任务和解析输入可分别在 `src/app/main.cpp`、`src/storage/store.cpp` 和 `src/domain/entryparser.cpp` 设置断点。

也可从仓库根目录运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -Configuration Debug
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build.ps1 -Configuration Release
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\run.ps1
```

运行程序时可传入 `--data-dir <目录>` 隔离数据库与设置；正常启动使用原正式数据目录。自检使用 `--self-test`，两个参数分别使用。

## 源码导览

| 层 | 职责 |
| --- | --- |
| `src/app/` | 应用入口、任务操作协调、配置应用 |
| `src/domain/` | 任务与分类数据结构、计划规则、智能输入解析 |
| `src/storage/` | SQLite 访问、迁移、导入导出与备份 |
| `src/ui/windows/` | 管理器、悬浮窗、编辑面板与设置窗口 |
| `src/ui/widgets/` | 列表、日历、智能输入和表单控件 |
| `src/ui/theme/` | 样式表、主题与缩放 |
| `tests/` | 业务、交互与布局自检 |

更改任务规则时从业务层开始；更改数据格式时查看存储层；更改交互时从相应窗口或控件开始。新增 C++ 文件需同时更新 qmake 和 CMake 的源码清单。

本地工具链、构建输出、调试数据和测试截图由 Git 忽略。使用新的构建目录可避免复用迁移前的绝对路径缓存。
