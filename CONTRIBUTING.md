# Contributing Guide

本文档定义本仓库的 Git 分支管理、代码开发、Pull Request、Code Review 和版本合并流程。

所有开发者在参与项目开发前，请先阅读并遵守本文档。

---

## 1. 分支结构

本项目采用以下分支模型：

```text
                         master
                           ↑
                           │ Pull Request
                           │ Merge Commit
                           │
                          dev
                    ↑       ↑       ↑
                    │       │       │
                  PR      PR      PR
                    │       │       │
             feature/*   fix/*   experiment/*
```

各分支用途如下：

| 分支             | 用途          | 是否长期保留 |
| -------------- | ----------- | ------ |
| `master`       | 稳定版本 / 发布版本 | 是      |
| `dev`          | 日常开发集成分支    | 是      |
| `feature/*`    | 新功能开发       | 否      |
| `fix/*`        | Bug 修复      | 否      |
| `experiment/*` | 实验性功能开发     | 否      |
| `sync/*`       | 同步上游官方仓库    | 否      |

基本原则：

* 禁止直接在 `master` 上开发。
* 禁止直接在 `dev` 上开发。
* 所有功能开发必须从最新的 `dev` 创建独立开发分支。
* 开发完成后通过 Pull Request 合并到 `dev`。
* `dev` 测试稳定后，再通过 Pull Request 合并到 `master`。

---

# 2. 第一次克隆项目

开发者应克隆本项目仓库，而不是直接克隆上游官方仓库。

SSH：

```bash
git clone git@github.com:<OWNER>/<REPOSITORY>.git
cd <REPOSITORY>
```

或者 HTTPS：

```bash
git clone https://github.com/<OWNER>/<REPOSITORY>.git
cd <REPOSITORY>
```

检查远程仓库：

```bash
git remote -v
```

正常情况下应看到：

```text
origin  git@github.com:<OWNER>/<REPOSITORY>.git (fetch)
origin  git@github.com:<OWNER>/<REPOSITORY>.git (push)
```

普通开发者通常只需要配置 `origin`。

官方上游仓库 `upstream` 原则上由项目维护者负责同步。

---

# 3. 初始化本地 `dev` 分支

第一次 clone 后，如果本地只有 `master`：

```bash
git fetch origin
git switch -c dev --track origin/dev
```

检查：

```bash
git branch
```

应看到类似：

```text
* dev
  master
```

以后直接执行：

```bash
git switch dev
```

即可。

---

# 4. 开始一个新任务

每次开始开发之前，必须先更新本地 `dev`：

```bash
git switch dev
git pull --ff-only origin dev
```

推荐使用：

```bash
git pull --ff-only
```

避免在本地共享分支上意外产生额外 Merge Commit。

然后从最新的 `dev` 创建开发分支。

---

## 4.1 新功能

```bash
git switch -c feature/<feature-name>
```

例如：

```bash
git switch -c feature/login
```

---

## 4.2 Bug 修复

```bash
git switch -c fix/<bug-name>
```

例如：

```bash
git switch -c fix/network-timeout
```

---

## 4.3 实验性功能

```bash
git switch -c experiment/<experiment-name>
```

例如：

```bash
git switch -c experiment/new-model
```

---

# 5. 分支命名规范

推荐使用：

```text
feature/<name>
fix/<name>
experiment/<name>
refactor/<name>
docs/<name>
test/<name>
```

例如：

```text
feature/login
feature/database
fix/network-timeout
fix/null-pointer
experiment/new-model
docs/update-readme
refactor/network-module
```

分支名称建议：

* 使用英文；
* 使用小写字母；
* 单词之间使用 `-`；
* 名称能够直接说明任务内容。

不推荐：

```text
test1
new
abc
mybranch
temp
```

---

# 6. 日常开发

查看当前状态：

```bash
git status
```

查看修改：

```bash
git diff
```

添加修改：

```bash
git add .
```

提交：

```bash
git commit -m "feat: implement login"
```

继续开发时可以正常创建多个 commit：

```bash
git add .
git commit -m "fix: handle login timeout"
```

```bash
git add .
git commit -m "test: add login tests"
```

由于 `feature/* → dev` 最终使用 Squash Merge，因此开发分支内部允许存在多个开发过程 commit。

---

# 7. Commit Message 规范

推荐采用 Conventional Commits 风格。

常见类型：

```text
feat:     新功能
fix:      Bug 修复
docs:     文档修改
refactor: 代码重构
test:     测试相关
chore:    构建、配置等维护工作
perf:     性能优化
```

例如：

```text
feat: add database support
fix: handle network timeout
docs: update Git workflow
refactor: simplify socket initialization
test: add database unit tests
```

避免使用：

```text
update
change
test
fix
123
修改代码
```

等无法明确描述变更内容的提交信息。

---

# 8. 第一次 Push 开发分支

例如当前分支：

```text
feature/login
```

执行：

```bash
git push -u origin feature/login
```

`-u` 会建立本地分支与远程分支的跟踪关系。

以后继续开发时：

```bash
git push
```

即可。

---

# 9. 开发期间同步最新 `dev`

如果开发过程中其他人的代码已经合并进入 `dev`，建议在提交 PR 前同步最新代码。

首先：

```bash
git fetch origin
```

然后：

```bash
git switch feature/login
git rebase origin/dev
```

如果没有冲突，rebase 会直接完成。

如果发生冲突：

```bash
git status
```

手动解决冲突后：

```bash
git add <已解决的文件>
git rebase --continue
```

如果希望取消此次 rebase：

```bash
git rebase --abort
```

由于 rebase 会改变 commit history，如果该开发分支之前已经 push，需要：

```bash
git push --force-with-lease
```

禁止习惯性使用：

```bash
git push --force
```

推荐始终使用：

```bash
git push --force-with-lease
```

以避免覆盖其他开发者的新提交。

---

# 10. 开发完成后的检查

提交 Pull Request 前，应至少确认：

```bash
git status
```

工作区没有遗漏修改。

同步最新 `dev`：

```bash
git fetch origin
git rebase origin/dev
```

然后执行项目对应的：

```text
Build
Unit Test
Integration Test
Lint
```

等检查。

确认代码能够正常编译、运行和测试后，再提交 Pull Request。

---

# 11. 创建 Pull Request：开发分支 → `dev`

开发完成并 Push 后，在 GitHub 创建 Pull Request。

必须确认：

```text
base:    dev
compare: feature/<name>
```

例如：

```text
base:    dev
compare: feature/login
```

即：

```text
feature/login
      │
      │ Pull Request
      ▼
     dev
```

不要直接创建：

```text
feature/login → master
```

---

# 12. Pull Request 内容

PR 标题应简洁说明本次改动，例如：

```text
feat: implement login
fix: handle network timeout
docs: update Git development guide
```

PR 描述建议至少说明：

```text
1. 修改了什么
2. 为什么修改
3. 如何测试
4. 是否存在已知问题
```

如果关联 Issue，可以在 PR 中填写：

```text
Closes #123
```

---

# 13. Code Review

`dev` 分支受到 Ruleset 保护。

正常情况下 Pull Request 需要至少：

```text
1 approving review
```

Reviewer 应：

1. 打开 Pull Request；
2. 查看 `Files changed`；
3. 检查代码实现；
4. 提出必要的修改意见；
5. 最终选择 `Approve`。

GitHub 操作：

```text
Files changed
    ↓
Review changes
    ↓
Approve
    ↓
Submit review
```

如果选择：

```text
Comment
```

只代表评论，不算 Approval。

如果选择：

```text
Request changes
```

则表示代码需要修改。

---

# 14. Review 后继续修改

如果 Reviewer 提出修改意见，开发者继续在原开发分支修改即可。

例如：

```bash
git switch feature/login
```

修改代码后：

```bash
git add .
git commit -m "fix: address review comments"
git push
```

如果 Ruleset 开启了：

```text
Dismiss stale pull request approvals when new commits are pushed
```

新 commit Push 后，之前的 Approval 会失效，需要 Reviewer 重新审核。

所有 Review Conversation 也应在问题解决后执行：

```text
Resolve conversation
```

---

# 15. 开发分支合并到 `dev`

Pull Request：

```text
feature/* → dev
fix/* → dev
experiment/* → dev
```

统一使用：

```text
Squash and merge
```

例如开发分支原本：

```text
feat: initial database
fix: database connection
fix: typo
test: add database test
```

Squash 后进入 `dev`：

```text
feat: add database support
```

这样可以避免大量开发过程 commit 污染 `dev` 历史。

---

# 16. 开发分支合并后的清理

GitHub 已启用：

```text
Automatically delete head branches
```

因此 Pull Request 合并后，远程开发分支通常会自动删除。

例如：

```text
feature/login → dev
```

Merge 后：

```text
origin/feature/login
```

会自动删除。

开发者本地仍需自行清理。

首先切回 `dev`：

```bash
git switch dev
```

更新：

```bash
git pull --ff-only origin dev
```

删除本地开发分支：

```bash
git branch -d feature/login
```

如果 Git 提示该分支未合并，但已经确认 PR 完成 Squash Merge，可以使用：

```bash
git branch -D feature/login
```

最后清理已经不存在的远程引用：

```bash
git fetch --prune
```

---

# 17. 为什么 Squash Merge 后可能需要 `-D`

Squash Merge 会把开发分支上的多个 commit 创建为一个新的 commit。

例如：

```text
feature/login:

A
B
C
```

进入 `dev` 后可能成为：

```text
dev:

S
```

其中：

```text
S = Squash(A + B + C)
```

因此 Git 从 commit ancestry 的角度可能认为原来的：

```text
A
B
C
```

并没有直接进入 `dev`。

所以：

```bash
git branch -d feature/login
```

可能提示分支没有完全 merge。

只要确认 GitHub PR 已经成功 Squash Merge，即可：

```bash
git branch -D feature/login
```

---

# 18. 开始下一个任务

完成一个任务后：

```bash
git switch dev
git pull --ff-only origin dev
```

然后重新从最新 `dev` 创建开发分支：

```bash
git switch -c feature/<next-feature>
```

不要长期重复使用已经完成的 feature branch。

推荐：

```text
一个任务
=
一个开发分支
=
一个 Pull Request
```

---

# 19. `dev` → `master` 发布流程

`dev` 是开发集成分支。

`master` 是稳定版本分支。

因此不是每合并一个 feature 都立即：

```text
dev → master
```

而是在一批功能完成并经过测试后，再进行版本合并。

例如当前 `dev` 包含：

```text
feat: login
feat: database
fix: network timeout
```

完成：

```text
Build ✅
Unit Test ✅
Integration Test ✅
Manual Test ✅
```

后，由 Maintainer 创建：

```text
dev
 │
 │ Pull Request
 ▼
master
```

GitHub 上：

```text
base:    master
compare: dev
```

---

# 20. `dev` → `master` 使用 Merge Commit

与：

```text
feature/* → dev
```

不同，

```text
dev → master
```

应使用：

```text
Create a merge commit
```

而不是 Squash Merge。

即：

```text
feature/*
    │
    │ Squash
    ▼
   dev
    │
    │ Merge Commit
    ▼
 master
```

这样可以保留长期分支之间清晰的 Git ancestry。

---

# 21. `dev` 和 `master` 不删除

以下是长期分支：

```text
master
dev
```

永远不要在正常开发流程中删除。

以下是临时开发分支：

```text
feature/*
fix/*
experiment/*
sync/*
```

任务完成后应删除。

---

# 22. 禁止直接 Push `dev` / `master`

正常开发者不要执行：

```bash
git push origin dev
```

或者：

```bash
git push origin master
```

Repository Rulesets 会阻止普通开发者直接修改这些受保护分支。

正确流程：

```text
开发分支
   ↓
Pull Request
   ↓
dev
   ↓
Pull Request
   ↓
master
```

部分 Maintainer 可能拥有 Ruleset Bypass 权限，但 Bypass 只用于必要的仓库维护或紧急情况，不作为正常开发流程。

---

# 23. 禁止 Force Push `dev` / `master`

禁止：

```bash
git push --force origin dev
```

禁止：

```bash
git push --force origin master
```

`dev` 和 `master` 均受到 Ruleset 保护。

个人开发分支发生 rebase 后，可以：

```bash
git push --force-with-lease
```

---

# 24. 与官方上游仓库的关系

本项目基于官方上游项目开发。

Maintainer 本地通常存在两个 remote：

```text
origin
→ 本项目 GitHub Repository

upstream
→ 官方 Repository
```

可通过：

```bash
git remote -v
```

查看。

例如：

```text
origin    git@github.com:<OWNER>/<REPOSITORY>.git
upstream  https://github.com/<OFFICIAL>/<REPOSITORY>.git
```

普通开发者一般不需要维护 `upstream`。

---

# 25. Maintainer 同步官方代码

同步官方代码由 Maintainer 负责。

首先：

```bash
git fetch upstream
```

查看官方新增 commit：

```bash
git log master..upstream/master --oneline
```

查看代码差异：

```bash
git diff master..upstream/master
```

更新自己的 `master`：

```bash
git switch master
git pull --ff-only origin master
```

创建专门的同步分支：

```bash
git switch -c sync/upstream-YYYY-MM
```

例如：

```bash
git switch -c sync/upstream-2026-08
```

然后：

```bash
git merge upstream/master
```

解决冲突并完成测试后：

```bash
git push -u origin sync/upstream-2026-08
```

再通过 Pull Request 合并：

```text
sync/upstream-2026-08
          ↓
         PR
          ↓
       master
```

不推荐直接：

```bash
git pull upstream master
```

因为：

```bash
git fetch upstream
```

之后先检查差异，再决定如何 merge，更加安全和可控。

---

# 26. 推荐的日常命令

普通开发者绝大多数情况下只需要下面这些命令。

开始任务：

```bash
git switch dev
git pull --ff-only origin dev

git switch -c feature/<name>
```

开发：

```bash
git status
git diff

git add .
git commit -m "feat: <description>"
```

Push：

```bash
git push -u origin feature/<name>
```

然后在 GitHub：

```text
feature/<name>
      ↓
     PR
      ↓
     dev
```

---

# 27. 开发期间同步 `dev`

```bash
git fetch origin
git rebase origin/dev
```

如果已经 push：

```bash
git push --force-with-lease
```

---

# 28. PR Merge 后

```bash
git switch dev
git pull --ff-only origin dev

git branch -D feature/<name>

git fetch --prune
```

然后开始下一项任务：

```bash
git switch -c feature/<next-feature>
```

---

# 29. 完整开发流程速查

```text
1. Clone Repository
        ↓
2. Checkout dev
        ↓
3. git pull --ff-only origin dev
        ↓
4. 创建 feature/* 或 fix/*
        ↓
5. 开发
        ↓
6. Commit
        ↓
7. Push 开发分支
        ↓
8. 创建 PR：开发分支 → dev
        ↓
9. Code Review
        ↓
10. Approval
        ↓
11. Squash and merge
        ↓
12. 自动删除远程开发分支
        ↓
13. 本地切回 dev
        ↓
14. 更新 dev
        ↓
15. 删除本地开发分支
        ↓
16. 开始下一项任务
```

版本发布：

```text
feature/* ──┐
fix/* ──────┼── PR + Squash ──→ dev
experiment/*┘                     │
                                  │ 测试
                                  │
                                  ▼
                            PR + Review
                                  │
                            Merge Commit
                                  │
                                  ▼
                               master
```

---

# 30. 开发规则总结

所有开发者需要遵守以下原则：

1. 不直接在 `master` 开发。
2. 不直接在 `dev` 开发。
3. 每次开发前先更新 `dev`。
4. 所有开发分支从最新 `dev` 创建。
5. 一个任务对应一个开发分支。
6. 开发完成通过 PR 合并到 `dev`。
7. PR 必须经过 Code Review。
8. `feature/* → dev` 使用 Squash Merge。
9. `dev → master` 使用 Merge Commit。
10. PR Merge 后删除临时开发分支。
11. 禁止 Force Push `dev` 和 `master`。
12. 个人开发分支 rebase 后使用 `--force-with-lease`，不要使用普通 `--force`。
13. `master` 只保存经过验证的稳定版本。
14. 官方 `upstream` 同步由 Maintainer 统一处理。
