---
name: Push to git
description: Automates staging, committing with a conventional message, and pushing.
disable-model-invocation: true
---

# Push to git Workflow
1. Run `git add .` to stage all changes.
2. Analyze the `git diff --staged` to understand changes.
3. Generate a commit message following the [Conventional Commits](https://conventionalcommits.org) standard.
4. Run `git commit -m "[generated message]"`.
5. Run `git push`.
