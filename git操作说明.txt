可以。你现在的开发流程已经比较清晰了，完整链路就是：

```text
开发者 clone 你的仓库
        ↓
基于 dev 创建 feature/*
        ↓
开发 + commit
        ↓
push feature/*
        ↓
Pull Request → dev
        ↓
Review
        ↓
Squash Merge
        ↓
dev 集成测试
        ↓
Pull Request: dev → master
        ↓
Review
        ↓
Merge Commit
        ↓
master 稳定版本
```

下面按实际操作完整走一遍。

## 1. 开发者第一次 Clone

开发者不要 clone 官方 `upstream`，而是 clone **你的 GitHub 仓库**：

```bash
git clone git@github.com:YOUR_NAME/YOUR_REPO.git
cd YOUR_REPO
```

如果不用 SSH，也可以：

```bash
git clone https://github.com/YOUR_NAME/YOUR_REPO.git
cd YOUR_REPO
```

检查远程：

```bash
git remote -v
```

应该看到：

```text
origin  git@github.com:YOUR_NAME/YOUR_REPO.git (fetch)
origin  git@github.com:YOUR_NAME/YOUR_REPO.git (push)
```

对普通开发者来说，只有 `origin` 就够了。

官方 `upstream` 通常由你这个 maintainer 管理，不需要每个开发者都配置。

---

# 2. 获取远程分支

先：

```bash
git fetch origin
```

查看：

```bash
git branch -a
```

可能看到：

```text
* master
  remotes/origin/master
  remotes/origin/dev
```

第一次需要创建本地 `dev`：

```bash
git switch -c dev --track origin/dev
```

以后就直接：

```bash
git switch dev
```

---

# 3. 每次开发前先更新 dev

这是很重要的习惯。

```bash
git switch dev
git pull --ff-only origin dev
```

我建议用：

```bash
--ff-only
```

而不是简单：

```bash
git pull
```

因为 `dev` 是共享分支，开发者本地一般不应该在 `dev` 上制造自己的 merge commit。

所以：

```bash
git pull --ff-only origin dev
```

更干净。

---

# 4. 从最新 dev 创建自己的开发分支

例如开发登录：

```bash
git switch dev
git pull --ff-only origin dev

git switch -c feature/login
```

数据库开发：

```bash
git switch -c feature/database
```

修 bug：

```bash
git switch -c fix/network-timeout
```

实验功能：

```bash
git switch -c experiment/new-model
```

核心原则：

> **所有开发分支都从最新 `dev` 创建，而不是从 `master` 创建。**

即：

```text
master
  ↑
 dev
  ├── feature/login
  ├── feature/database
  └── fix/network
```

---

# 5. 正常开发

修改代码后：

```bash
git status
```

检查变化：

```bash
git diff
```

提交：

```bash
git add .
git commit -m "feat: implement login"
```

继续开发：

```bash
git add .
git commit -m "fix: handle login timeout"
```

你的 feature 分支可能变成：

```text
dev
 │
 └── feature/login
       ├── feat: implement login
       └── fix: handle login timeout
```

---

# 6. 第一次 Push feature 分支

执行：

```bash
git push -u origin feature/login
```

这里：

```text
-u
```

会建立：

```text
本地 feature/login
       ↕
origin/feature/login
```

之后继续开发时只需要：

```bash
git push
```

---

# 7. 开发期间如果 dev 更新了怎么办？

这个很常见。

假设你开发了两天，同时其他人的代码已经进入 `dev`。

先：

```bash
git fetch origin
```

你可以看一下：

```bash
git log --oneline --graph --decorate --all
```

此时建议把最新 `dev` 更新到自己的 feature。

对于个人 feature branch，我比较推荐：

```bash
git switch feature/login
git rebase origin/dev
```

如果没有冲突：

```text
成功
```

如果有冲突：

```bash
git status
```

修复文件，然后：

```bash
git add <文件>
git rebase --continue
```

全部完成后，因为 rebase 改了 commit history：

```bash
git push --force-with-lease
```

注意是：

```bash
--force-with-lease
```

不要习惯性使用：

```bash
git push --force
```

因为 `--force-with-lease` 更安全。

---

# 8. Feature 开发完成

开发者先确保自己分支基于最新 dev：

```bash
git fetch origin
git rebase origin/dev
```

然后跑测试。

例如：

```bash
npm test
```

或者：

```bash
pytest
```

或者你的项目自己的：

```bash
make test
```

确认没问题。

最后：

```bash
git push --force-with-lease
```

如果之前没 rebase，只是普通新增 commit：

```bash
git push
```

---

# 9. 创建 PR：feature → dev

进入 GitHub。

创建：

```text
Pull Request
```

一定确认：

```text
base:    dev
compare: feature/login
```

不是：

```text
base: master
```

所以：

```text
feature/login
      ↓
     PR
      ↓
     dev
```

---

# 10. PR Review

因为你现在给 `dev` 设置了 Ruleset：

```text
Require Pull Request
Required approvals: 1
Require conversation resolution
```

所以流程类似：

```text
Developer
   ↓
feature/login
   ↓
PR → dev
   ↓
Reviewer A
   ↓
Approve
```

如果 Reviewer 提出意见：

```text
这里需要修改
```

开发者回本地：

```bash
git switch feature/login
```

修改：

```bash
git add .
git commit -m "fix: address review comments"
git push
```

由于你开启了：

```text
Dismiss stale approvals when new commits are pushed
```

之前的 Approval 会失效。

需要重新：

```text
Review → Approve
```

并把 discussion：

```text
Resolve conversation
```

---

# 11. Merge feature → dev

按照我们前面给 `dev` 定的规则：

```text
Allowed merge method:
Squash
```

因此 GitHub PR 上选择：

```text
Squash and merge
```

例如 feature 里原本有：

```text
feat: initial login
fix: password validation
fix: typo
fix: tests
cleanup login code
```

Squash 进 `dev` 后成为一个：

```text
feat: implement login
```

于是 `dev` 历史比较干净。

---

# 12. 删除 feature branch

PR Merge 完以后，GitHub 上：

```text
Delete branch
```

如果开启了自动删除 head branch，它会自动做。

开发者本地：

```bash
git switch dev
```

更新：

```bash
git pull --ff-only origin dev
```

删除本地：

```bash
git branch -d feature/login
```

现在这个任务结束。

---

# 13. 开发者开始下一个任务

又从最新 dev：

```bash
git switch dev
git pull --ff-only origin dev

git switch -c feature/next-feature
```

重复即可。

所以普通开发者每天核心工作实际上就是：

```bash
git switch dev
git pull --ff-only origin dev

git switch -c feature/xxx

# 开发

git add .
git commit -m "feat: xxx"

git push -u origin feature/xxx
```

然后：

```text
GitHub PR
feature/xxx → dev
```

---

# 14. dev 到 master 谁来操作？

这一步我建议由：

```text
Maintainer / 项目负责人
```

来做。

而不是普通开发者每完成一个 feature 就：

```text
dev → master
```

因为 `dev` 应该是：

> 一批已经完成 Review、正在集成测试的功能。

例如：

```text
dev

feat: login
feat: database
fix: network timeout
feat: model configuration
```

经过：

```text
CI
集成测试
手工测试
staging
```

确认这一批功能可以发布，才：

```text
dev → master
```

---

# 15. 发布前更新 dev

Maintainer：

```bash
git switch dev
git pull --ff-only origin dev
```

测试：

```bash
# 根据你的项目
pytest

# 或
npm test

# 或
make test
```

确认：

```text
Tests ✅
Build ✅
功能验证 ✅
```

---

# 16. 创建 PR：dev → master

GitHub：

```text
New Pull Request
```

设置：

```text
base:    master
compare: dev
```

也就是：

```text
dev
 ↓
PR
 ↓
master
```

PR 标题可以类似：

```text
Release v1.2.0
```

或者：

```text
Merge dev into master - 2026-08 release
```

PR 中能看到这次准备发布的所有变化。

---

# 17. Review dev → master

由于你的 `master` Ruleset 比较严格：

```text
Required approvals: 1
Dismiss stale approvals
Require conversation resolution
Block force pushes
```

需要其他 Collaborator：

```text
Review
 ↓
Approve
```

如果有问题：

```text
不要直接修改 master
```

而是回到：

```text
dev / feature branch
```

进行修复。

---

# 18. dev → master 使用 Merge Commit

这和前面的 feature → dev 不同。

前面：

```text
feature → dev
        Squash
```

这里：

```text
dev → master
     Merge commit
```

GitHub 上选择：

```text
Create a merge commit
```

最终类似：

```text
master
   │
   │
   ├────────────── M  ← Merge dev into master
   │              /
   │             /
dev ─ A ─ B ─ C
```

这样 Git 能明确知道：

```text
dev 的 A/B/C
已经正式进入 master
```

对于长期存在的 `dev` 分支更加自然。

---

# 19. master Merge 完以后不要删除 dev

注意：

```text
feature/*
```

是临时分支，可以删除。

但：

```text
dev
master
```

都是长期分支。

所以：

```text
feature/login         → Merge 后删除 ✅
feature/database      → Merge 后删除 ✅
fix/network-timeout   → Merge 后删除 ✅

dev                   → 不删除
master                → 不删除
```

---

# 20. 发布之后开发者同步 dev

当：

```text
dev → master
```

完成以后，`dev` 本身仍然存在。

开发者下一次：

```bash
git switch dev
git pull --ff-only origin dev
```

然后继续：

```bash
git switch -c feature/new-feature
```

---

# 21. 一个完整实例

假设 A 要开发数据库模块。

### A 第一次：

```bash
git clone git@github.com:yourname/project.git
cd project

git fetch origin

git switch -c dev --track origin/dev
```

开始任务：

```bash
git switch dev
git pull --ff-only origin dev

git switch -c feature/database
```

开发：

```bash
git add .
git commit -m "feat: add database layer"

git add .
git commit -m "test: add database tests"
```

推送：

```bash
git push -u origin feature/database
```

GitHub：

```text
feature/database
      ↓
     PR
      ↓
     dev
```

Reviewer Approve。

然后：

```text
Squash and merge
```

得到：

```text
dev

A -- feat: login
B -- feat: add database layer
```

A 本地清理：

```bash
git switch dev
git pull --ff-only origin dev

git branch -d feature/database
```

---

# 22. 接着 B 修 network

B：

```bash
git switch dev
git pull --ff-only origin dev

git switch -c fix/network-timeout
```

开发：

```bash
git add .
git commit -m "fix: handle network timeout"

git push -u origin fix/network-timeout
```

然后：

```text
fix/network-timeout
       ↓
      PR
       ↓
      dev
```

Squash merge。

现在：

```text
dev

A -- feat: login
B -- feat: database
C -- fix: network timeout
```

---

# 23. 准备发布

Maintainer 检查：

```text
dev
 │
 ├── login ✅
 ├── database ✅
 └── network fix ✅
```

GitHub：

```text
dev
 ↓
PR
 ↓
master
```

Review：

```text
Approve ✅
```

使用：

```text
Create a merge commit
```

完成：

```text
                     master
                        │
                        M  release
                       /
dev ─── A ─── B ─── C
```

---

# 24. 你们团队可以直接遵守这几个规则

实际上只需要记住这 7 条：

1. **不在 `master` 开发**
2. **不在 `dev` 直接开发**
3. 开发前先更新 `dev`
4. 每个任务一个 `feature/*` / `fix/*`
5. `feature/* → dev` 必须 PR
6. `feature/* → dev` 用 **Squash Merge**
7. `dev → master` 用 **Merge Commit**

最终就是：

```text
                            master 🔒
                               ▲
                               │
                         Merge Commit
                               │
                             PR
                               │
                              dev 🔒
                         ▲      ▲      ▲
                         │      │      │
                       Squash Squash Squash
                         │      │      │
                    feature  feature   fix
```

再加一个非常实用的团队约定：**每个 feature 分支只由一个人主要维护；如果需要其他人一起改，先沟通后再 push 到同一个 feature branch。** 这样你无需给个人分支增加额外 Ruleset，团队 Git 历史也会很清楚。

下一步建议把这套流程写进仓库的 `CONTRIBUTING.md`，这样新同事 clone 项目之后直接照着操作，不需要每次重新解释。
