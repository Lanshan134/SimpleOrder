# 简序 · SimpleOrder

桌面一角，井然有序。

Windows 原生个人任务管理器，基于 C++17、Qt Widgets 和 SQLite。当前为 v0.2 源码开发版；本地 `outputs/` 中旧安装包不代表最新源码。

## 功能

- 悬浮任务列表与月历两种视图，无边框、置顶、尺寸缩放，背景与文字透明度独立设置。
- 任务与计划使用独立表单；计划支持每天/周/月次数、指定星期或月日期、间隔重复、累计次数目标。
- 已完成事项淡化并加删除线，默认 48 小时后隐藏，数据库记录保留。
- 自定义类型名称、颜色和排序；任务实心点、计划空心点。
- 日历拖放改期。编辑任务时点击日期修改日期草稿；计划有效期默认不限。
- 编辑窗口支持与其他窗口同时操作；切换窗口保留本次运行内草稿，切换其他事项时按实际改动询问保存。
- 分类设置、JSON 导入导出、自动备份。详情需点击保存；快速添加、完成、置顶和拖放即时保存。

时间字段仅作记录，不发送通知。尚无跨设备同步；多屏和不同 DPI 等桌面行为仍需进一步体验验证。

## 环境与构建

已验证：Windows、Qt 5.15.2、MinGW 8.1.0 x64、C++17。Qt 与编译器必须使用兼容套件。仓库不含 SDK、编译器、运行库或构建产物。

克隆后安装 Qt 和 MinGW，在项目根目录运行（路径按本机安装位置填写）：

```powershell
.\build.cmd C:\Qt\5.15.2\mingw81_64 C:\Qt\Tools\mingw810_64\bin
.\run.cmd
```

`build.cmd` 是原生 CMD 入口，无需修改 PowerShell 执行策略。中文路径使用临时英文目录联接构建；中间文件位于 `build/`，可运行目录位于 `outputs/SimpleOrder-v0.2/`。

无参数构建使用开发机默认路径：Qt 为项目下 `work/qt/5.15.2/mingw81_64`，MinGW 为 `D:/CodeApp/Environment/mingw64/bin`。其他机器须传入上述两个参数。VS Code 的 `Ctrl+Shift+B` 使用无参数构建并运行；请按本机环境在 `.vscode/tasks.json` 的构建任务参数中补上 Qt、MinGW 路径。重新编译前先退出运行中的程序。

也可使用 CMake：

```powershell
cmake -S . -B build/cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/mingw81_64
cmake --build build/cmake
```

使用 CMake 时 MinGW 须在 PATH 中；运行前还需要部署 Qt DLL、插件与 `style.qss`。现有 `build.cmd` 已包含这些部署步骤。

## 验证

构建完成后，在项目根目录运行自检：

```powershell
$testRun = Start-Process -FilePath .\outputs\SimpleOrder-v0.2\SimpleOrder.exe -ArgumentList '--self-test' -Wait -PassThru
if ($testRun.ExitCode -ne 0) { throw "Self-test failed: $($testRun.ExitCode)" }
```

自检写入独立 `test-results/run-*`，生成结果与窗口截图，不使用正式数据库。覆盖数据迁移、备份往返、四种计划规则、草稿与日期交互、滚轮、设置和透明度等。自动检查不替代真实桌面操作体验。

## 目录

| 路径 | 用途 |
|---|---|
| `src/` | Qt 界面、计划规则、SQLite 存储与自检逻辑 |
| `style.qss` | 应用样式 |
| `taskmanager.pro` / `CMakeLists.txt` | 构建配置 |
| `.vscode/tasks.json` | 共享 VS Code 构建任务 |
| `tests/` | 自检启动脚本 |
| `docs/` | 编辑窗口设计与交互说明 |
| `licenses/` / `THIRD_PARTY_NOTICES.md` | 已有第三方许可文本和说明 |
| `work/`、`build/`、`outputs/`、`test-results/` | 本地目录，不提交 Git |

## 数据

为兼容改名前的版本，正式数据继续位于 `%LOCALAPPDATA%/LightTaskLocal/LightTask/`，也可在设置中查看。数据库升级与导入前会自动备份。个人数据库、备份与个人导出 JSON 不应加入源码仓库。

## 文档

- [编辑窗口设计与交互](docs/editor-design.md)
- [历史更新记录](CHANGELOG.md)
- [第三方组件说明](THIRD_PARTY_NOTICES.md)

## 许可证

项目自身代码和文档采用 [MIT License](LICENSE)，版权署名为 Lanshan134。第三方组件继续适用各自许可证，详见 [第三方组件说明](THIRD_PARTY_NOTICES.md) 和 `licenses/`。
