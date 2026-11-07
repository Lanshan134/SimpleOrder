# 简序 · SimpleOrder v2.0.0

本版采用重新设计的界面，保留智能快速录入、任务和计划规则、数据备份以及日期编辑保护。Windows 便携试用包的历史版本见 GitHub Releases。

- 管理器分为列表和日历两栏。打开事项时才显示编辑面板，关闭面板后可以继续浏览日历并保留草稿。
- 列表显示相对日期。今天使用强调色，逾期使用警示色，明天使用普通文字样式。悬停可查看完整日期。
- 悬浮列表随窗口增高显示更多整行，快速添加栏固定在底部。小窗口日历可以滚动，隐藏周序号后仍可点选首列日期。
- 设置页提供更明确的按钮边界、焦点和危险操作样式，保留清空确认及备份流程。
- VS Code 的「调试简序」使用独立数据库。构建、断点和自检说明见 [开发指南](development.md)。

## Issue 核对

| Issue | v2.0.0 内容 |
| --- | --- |
| [#1](https://github.com/Lanshan134/SimpleOrder/issues/1) | 既有智能录入已在 beta 完成，本版沿用 |
| [#4](https://github.com/Lanshan134/SimpleOrder/issues/4) | 精简常驻说明，保留错误、草稿和确认提示 |
| [#7](https://github.com/Lanshan134/SimpleOrder/issues/7) | 显示今天、明天、昨天。明天沿用普通样式 |
| [#8](https://github.com/Lanshan134/SimpleOrder/issues/8) | 已改善设置控件辨识度及危险操作样式，实际使用验收单独跟踪 |
| [#9](https://github.com/Lanshan134/SimpleOrder/issues/9) | 修复悬浮列表伸缩布局，验证窗口拉高、小窗口日历和固定底部栏 |

悬浮窗图标上栏（#2）和日历专用展开／收起入口（#3）仍在需求清单。
