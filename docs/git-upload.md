# Git 上传前说明

## 已整理

- 准备提交源码、构建脚本、共享 VS Code 任务、文档与第三方许可文本。
- `work/`（含本机 Qt SDK）、`build/`、`outputs/`（含旧 EXE、DLL、ZIP）、`test-results/` 与日志均已忽略，本地文件没有删除。
- SQLite 数据库及旁文件、升级备份、常见凭据文件均已忽略。个人导出 JSON 不自动全局忽略，请导出到仓库外或已忽略目录，暂存前逐项检查。
- 原型和编辑设计已整理到 `docs/`，原型是历史参考。

## 首次提交

已添加 MIT 许可证，使用当前 Git 署名 Lanshan134；Git 提交姓名和邮箱已配置。首次上传时，应先准备好空的远程仓库。创建远程仓库时建议先不要自动生成 README，以免首次推送需要合并历史。

```powershell
git status --short
git add .
git diff --cached --stat
git diff --cached --check
git diff --cached
# 确认后再提交
git commit -m "Initial source version of SimpleOrder"
```

配置自己的远程地址后再推送；仓库地址、分支名和公开/私有状态由你确定。尚未整理好的个人项目可先使用私有仓库。

## 注意

- `.gitignore` 不会移除已跟踪文件；首次添加时规则会作用于尚未跟踪的文件。
- 不将 Qt SDK、编译器、EXE、DLL、安装包提交到源码仓库；稳定版安装包可单独作为发布附件管理。
- 本机默认构建路径不能跨机器直接使用；README 给出了显式传入 Qt/MinGW 路径的命令。
- 上传前检查新加入文件中是否含真实任务、账号、密码、令牌或个人路径。自动模式扫描只能辅助，不能保证发现全部秘密。
- 项目自身代码与文档采用根目录 MIT LICENSE；第三方 LGPL/GPL 等许可文件继续保留。
- 以后从源码重新分发可运行包时，需一起整理现有第三方说明及所需许可文本。当前旧安装包没有因本次整理而更新。

## 上传前还需要提供

1. 在 GitHub、Gitee 或其他平台创建空仓库，并确定仓库名称和公开/私有状态。
2. 复制远程仓库的 HTTPS 或 SSH 地址。创建时不要自动添加 README、许可证或 .gitignore，本地已有这些文件。
3. 确认本机 Git 的平台认证可用：HTTPS 使用凭据管理器登录，SSH 使用本机密钥。不要将令牌或私钥提交到仓库。
4. 审核暂存内容，创建首次提交，再配置远程并推送。

以下命令中的地址须替换为自己的真实仓库地址；首次上传可按以下顺序操作：

```powershell
git add .
git diff --cached --stat
git diff --cached --check
git commit -m "Initial source version of SimpleOrder"
git branch -M main
git remote add origin <你的仓库地址>
git push -u origin main
```

项目正式名称为“简序 · SimpleOrder”，建议仓库名称为 `SimpleOrder` 或 `simpleorder`。新构建输出为 `outputs/SimpleOrder-v0.2/SimpleOrder.exe`，本地旧版产物保留但不上传。
