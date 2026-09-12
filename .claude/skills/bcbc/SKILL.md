---
name: bcbc
description: Open Beyond Compare showing the diff between the current branch's working tree and main. Use whenever the user asks to visually diff/compare the current branch (or "this branch", "what I have") against main, or says "bcbc".
---

# bcbc — Beyond Compare vs. main

Launches Beyond Compare (BC) with the current working tree on one side and a
standing `main` reference checkout on the other, so the user gets a full
folder-tree diff instead of a text-only `git diff`.

## 0. Preconditions

- Beyond Compare must be installed at `C:\Program Files\Beyond Compare 5\BCompare.exe`.
  If not found there, check `C:\Program Files\Beyond Compare 4\BCompare.exe` before
  giving up.
- The reference checkout lives at `C:\MyData\repos\TeensyROM-AltPull` — a persistent
  separate clone of this repo kept on `main`, reused across runs instead of creating a
  throwaway worktree each time (a worktree nested under a deep temp/scratchpad path hit
  Windows' MAX_PATH limit on this repo's longer filenames — a short, stable path avoids
  that entirely).

## 1. Check the current branch

`git branch --show-current` in the current project directory. If it's already `main`,
tell the user there's nothing to diff against itself and stop.

## 2. Refresh the reference checkout to the latest main

```
git -C "C:\MyData\repos\TeensyROM-AltPull" checkout main
git -C "C:\MyData\repos\TeensyROM-AltPull" pull --ff-only
```

If the pull fails (diverged history, local commits ahead, uncommitted changes, etc.),
**don't force it** — report exactly what's blocking the fast-forward and let the user
decide (e.g. it may have intentional local-only commits, same as a prior session found
it "ahead of origin/main by 1 commit"). Proceed with the diff using whatever `main` state
it's currently on rather than blocking the whole task on this.

## 3. Launch Beyond Compare

```
"C:\Program Files\Beyond Compare 5\BCompare.exe" "<current project root>" "C:\MyData\repos\TeensyROM-AltPull" &
disown
```

Use the actual current working directory (the project root) as the left side — don't
hardcode a path, since the repo may be cloned elsewhere on a different machine. Background
the launch (`&` + `disown`) since it's a GUI app and the call shouldn't block on the window
staying open.

Tell the user which branch is on the left and confirm `main` is on the right, and mention
that `.git` will show as different between the two (expected — separate metadata per
checkout, not a real diff).
