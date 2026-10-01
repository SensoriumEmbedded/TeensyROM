---
name: bcbc
description: Open Beyond Compare on a git diff. With no argument it shows the current work against main. With a PR number it first creates or refreshes that PR's prNN-check review branch. It also takes a branch, a commit or a range. Use whenever the user asks to see a diff in Beyond Compare/"BC", asks to pull a PR into a review branch and show it, or says "bcbc".
---

# bcbc: Beyond Compare on a git diff

Opens Beyond Compare (BC) through `git difftool --dir-diff`. Git writes only the changed
files of each side into two temp folders, BC compares the folders, and git deletes them
when BC closes. There is no reference clone. The only machine-specific part is the user's
own git `diff.tool` setting.

## 0. Preconditions and ground rules

- `git config diff.tool` must name a tool that can compare folders, and that **waits until
  its window closes**. Git deletes the temp folders as soon as the tool returns. For BC on
  Windows that means `BComp.exe`. `BCompare.exe` returns at once and leaves BC showing
  deleted folders. If no tool is configured, tell the user how to set one up and stop:
  ```
  git config --global diff.tool bc
  git config --global difftool.bc.path "C:/Program Files/Beyond Compare 5/BComp.exe"
  ```
- **Beyond Compare gets extras** (section 6): an expanded tree, titles for each side, and
  read-only snapshots. The tool counts as BC when its executable ends in `BComp.exe` or
  `BCompare.exe`. Find the executable from `git config difftool.<tool>.path`, or as the
  first quoted path in `git config difftool.<tool>.cmd`. Either way, use the `BComp.exe` in
  that same folder, because it's the one that waits.
- Never switch the user's checkout, touch their working tree or index, or push. Everything
  below works on refs and objects only, so it works with uncommitted changes or with any
  branch checked out.

## 1. Pick the mode from the argument

| Argument | Mode |
|---|---|
| none | Current work (section 2) |
| digits, e.g. `48` or `#48` | PR (section 3) |
| contains `..` | Range (section 5) |
| an existing local branch | Branch (section 4) |
| anything else that resolves to a commit (`git rev-parse --verify <arg>^{commit}`) | Commit (section 5) |

## 2. Current work

- **On `main`:** use `git difftool --dir-diff HEAD`, which shows the uncommitted changes. If
  `git status --porcelain --untracked-files=no` is empty, say there's nothing to show and stop.
- **On another branch:** use `git difftool --dir-diff $(git merge-base main HEAD)`, which
  shows the branch's commits plus uncommitted edits, without main's newer commits.
- Tell the user three things:
  - The right side is their working tree.
  - Untracked files don't appear until added. `git add -N <file>` makes one show.
  - Edits saved on the right side are copied back into the working tree when BC closes.
    That is git's own `--dir-diff` behavior when it copies files, which is the Windows
    default.

## 3. PR

`N` is the PR number and `B` is the review branch `prN-check`.

1. **Fetch.** Run `git fetch origin main "+refs/pull/N/head:refs/remotes/origin/pr/N"`. The
   `+` is there because PRs get force-pushed. If local `main` is behind `origin/main`, say so,
   because the review branch is built on local `main`. Don't update `main` yourself.
2. **Already merged?** If `git merge-base --is-ancestor origin/pr/N main` succeeds, the PR is
   already in main.
   - Find the merge commit that brought it in:
     `M=$(git log --merges --ancestry-path --format=%H origin/pr/N..main | tail -1)`.
   - Show that merge's changes with `git difftool --dir-diff M^1 M`.
   - Don't create a review branch. Then go to section 6.
3. **Trial merge.** Run `git merge-tree --write-tree --name-only --no-messages main origin/pr/N`.
   - Exit 0 means it's clean, and the first output line is the merged tree `T`.
   - Exit 1 means there are conflicts. The lines after the first name the conflicting
     files; go to step 6.

   This merges across all merge bases, the same as `git merge`. GitHub's merge button
   sometimes reports conflicts that this doesn't, for example PR #48's criss-cross history.
   Those aren't real conflicts.
4. **Decide what to do with `B`.**
   - **`B` doesn't exist:** create it (step 5).
   - **`B` is a bcbc review branch.** That means its tip has exactly two parents, its message
     is `Merge remote-tracking branch 'origin/pr/N' into prN-check`, and
     `git merge-base --is-ancestor B^1 main` succeeds.
     - If `B^1` is `main` and `B^2` is `origin/pr/N`, it's **up to date**. Leave it.
     - Otherwise the PR or main has moved. **Rebuild** it (step 5). The old tip stays in the
       reflog; report it.
   - **`B` is anything else**, such as local commits on it, a squash preview, or a plain copy
     of the PR head: **don't touch it.**
     - Say what it is.
     - Show the would-be merge anyway: run step 5's `commit-tree` without `git branch -f`,
       which leaves a dangling commit.
     - Ask whether to rebuild it.
   - **`B` is checked out** (`git branch --show-current` is `B`) and would need creating or
     rebuilding: don't move it under the working tree. Say so and ask.
5. **Create or rebuild.** This gives the same tree, parents and message that
   `git merge --no-ff origin/pr/N` on a fresh `B` would.
   ```
   C=$(git commit-tree <T> -p main -p origin/pr/N -m "Merge remote-tracking branch 'origin/pr/N' into prN-check")
   git branch -f prN-check $C
   ```
   Then show it with `git difftool --dir-diff B^1 B`. That is exactly what the PR changes
   on main.
6. **Conflicts.**
   - Don't create or touch `B`.
   - Show the PR's own changes since it left main with
     `git difftool --dir-diff main...origin/pr/N`.
   - List the conflicting files. Suggest that the PR's author update the PR, or offer to
     resolve the conflicts on a branch.
   - If `git merge-base --all main origin/pr/N` returns more than one commit, warn that this
     three-dot view may include unrelated changes.

When the GitHub API is reachable, add the PR's title, author and state to the report. Use
`gh pr view N`, or
`curl -s https://api.github.com/repos/<owner>/<repo>/pulls/N` with the owner and repo taken
from `git remote get-url origin`. If neither works, skip it without comment.

## 4. Branch

This shows what merging the branch into main would change, without creating anything.

- Trial-merge as in section 3, step 3.
- **Clean:** run `C=$(git commit-tree <T> -p main -p <branch> -m "bcbc trial merge")`, which
  leaves a dangling commit with no ref. Then `git difftool --dir-diff main $C`.
- **Conflicts:** run `git difftool --dir-diff main...<branch>` and list the conflicting files.

## 5. Commit or range

- **Commit:** `git difftool --dir-diff <c>^1 <c>`. For a merge commit, that is what the merge
  brought in on top of its first parent.
- **Range:** pass it straight through, e.g. `git difftool --dir-diff a..b` or `a...b`.

## 6. Launch and report

- **Run the difftool command in the background** (Bash with `run_in_background: true`). It
  blocks until BC closes, and git deletes the temp folders when it returns. Don't wait on it.
  Its completion notice only means the user closed BC.
- **Any tool except Beyond Compare:** run `git difftool --dir-diff <left> [<right>]`. Leave
  `<right>` out when the right side is the working tree.
- **Beyond Compare:** pass a one-off tool definition with `git -c`, which leaves the user's
  config untouched:
  ```
  MSYS_NO_PATHCONV=1 BCBC_LEFT="<left title>" BCBC_RIGHT="<right title>" \
  git -c difftool.bcbc.cmd='"<path to BComp.exe>" /expandall <ro> "/title1=$BCBC_LEFT" "/title2=$BCBC_RIGHT" "$LOCAL" "$REMOTE"' \
      difftool --dir-diff --tool=bcbc <left> [<right>]
  ```
  - `/expandall` opens every folder. `/title1=` and `/title2=` replace the temp-folder paths
    in BC's path bars.
  - Set `<ro>` from what's on the right:
    - `/lro` (left side read-only) when the right side is the working tree, so edits
      there can still be copied back.
    - `/ro` (both sides read-only) when both sides are commits, because edits there would
      be thrown away.
  - Titles are each side's ref and short hash, for example `main 306daef`,
    `pr48-check 82dc4d5 (PR #48)`, `working tree`, or `trial merge of <branch>`.
  - Why it's built this way:
    - Git runs a tool command through its bundled shell, which rewrites any argument
      starting with `/` into a Windows path, so `/expandall` becomes
      `C:/Program Files/Git/expandall`. `MSYS_NO_PATHCONV=1` turns that off. Without it, the
      switches need a doubled slash (`//expandall`).
    - `git difftool -x "<cmd> /switches"` doesn't work in folder mode. Git spawns the whole
      string as one program name and fails with "cannot spawn ...: Permission denied". A
      named tool's `cmd` does go through the shell, which is why the switches go there.
    - The shell expands `$LOCAL`, `$REMOTE` and the `BCBC_*` titles at run time, which is
      why they stay inside single quotes above.
- **Report in a few lines:**
  - What's on the left and right, as refs with short hashes.
  - The file count, from `git diff --stat` on the same two sides.
  - In PR mode, whether `B` was created, already up to date, or rebuilt (give the old tip),
    and whether local `main` is behind `origin/main`.
  - Anything that will look odd in BC:
    - A renamed or moved file shows as two one-sided files, because BC doesn't follow
      renames. Compare the pair directly to see the edits.
    - Binary files show only as binary.
