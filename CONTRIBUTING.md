# 协作约定

日常开发基于 beta，稳定版本进入 main。

## 事项与分类

Issue 是工作记录，功能请求是其中一种；PR 是改动的审阅入口。它们共享仓库编号，保留原编号，通过标签、里程碑和看板查找。

每项工作使用一个 type、一个 priority、一个 status 标签，可使用多个 area 标签。type 包括 bug、feature、improvement、refactor、docs；P1 优先处理，P2 普通，P3 后续优化。优先级不等于发布日期承诺。

| 看板阶段 | 标签 | 条件 |
| --- | --- | --- |
| 待讨论 | status:discussion | 需要明确方案、范围或复现步骤 |
| 待开发 | status:ready | 范围和可验证的验收条件已明确 |
| 开发中 | status:in-progress | 已认领并开始开发 |
| 待验收 | status:review | 实现可审阅和试用 |
| 完成 | status:done | 验收通过，关闭 Issue |

认领后才设置负责人。开发者同步标签和看板状态，具体范围及验收结果记录在 Issue / PR。里程碑表示目标版本，Release 表示实际发布。代码合并、使用验收、版本发布分别记录。

## 开发与审阅

1. 从最新 origin/beta 创建独立分支，例如 codex/issue-9-window-layout、feature/... 或 fix/...，避免共同推送同一功能分支。
2. 先明确 Issue 范围、验收条件及负责人，再实现；需求变化同步记录。
3. PR 指向 beta，关联 Issue，说明最终行为和验证证据，由另一位协作者审阅。新提交后重新审阅。
4. 试用通过后关闭 Issue；验收前使用 Refs #编号，避免 Closes 自动关闭。
5. 稳定版本由 beta 向 main 提交 PR，使用 merge 保留长期分支关系；功能分支可 squash。发布前核对版本、提交、标签与产物。

beta、main 要求 PR、至少一位其他审阅者批准、解决审阅讨论，禁止强制推送和删除。首次协作配置完成后启用保护，后续按此流程提交。暂不要求尚未建立的 CI 检查。

## 验证与数据

按 README 使用本机兼容的 Qt / MinGW 构建，不提交个人绝对路径、SDK、构建输出。代码改动执行适用的构建与 --self-test；界面改动还需实际操作。试用时确认运行本次构建的程序，避免旧 EXE 导致误判。

默认只改源码；替换试用程序、更新 Release 按当次授权执行。文档和模板检查格式、链接及 Git 差异即可。

不要提交个人数据库、备份、凭据或含私人任务的日志。遵守 .gitignore，使用虚构任务复现问题。

[事项](https://github.com/Lanshan134/SimpleOrder/issues) · [PR](https://github.com/Lanshan134/SimpleOrder/pulls) · [里程碑](https://github.com/Lanshan134/SimpleOrder/milestones)
