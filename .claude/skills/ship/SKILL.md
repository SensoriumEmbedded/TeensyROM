---
name: ship
description: >
  Take a change in this repository all the way to a pull request that is ready
  to merge: branch in a worktree, implement, commit, review each change once at
  an effort set by its size and risk, open the PR as a draft, watch CI until
  green, then mark it ready for review. Stops before merging; the merge is
  always the maintainer's. Use when asked to implement a non-trivial change, or
  to take a change through the branch-review-PR workflow ("ship", "land",
  "take this to a PR").
---

# Ship a change

The standard workflow for non-trivial work in this repository. Every stage is
mandatory unless the user says otherwise, and the last one is a hard stop.

```
worktree → (implement → commit → review that change)* → final pass (if needed)
         → draft PR → CI green → before-Ready checks → ready for review → STOP
```

**Never merge.** Do not run `gh pr merge`, do not enable auto-merge, and do not
ask to. Hand over a green PR and stop.

Skip this skill for trivial work: a typo, a one-line fix the user dictated, a
pin bump. Say you are skipping it and why.

## 1. Worktree and branch

Call `EnterWorktree` with a `name` unless the session is in a worktree already.
Then, from the worktree:

```bash
git fetch -q origin
git checkout -q -b <type>/<short-slug> origin/main   # unless already on a fitting branch
```

`<type>` is `firmware`, `c64`, `tools`, `docs`, `test`, `ci` or `fix`, matching
the commit prefixes in `CLAUDE.md`. If the user is on a feature branch with
related work, stay on it.

A fresh worktree has no `tools/.cache`. Before the first firmware build, point
the builder at the primary checkout's toolchain:

```bash
export ARDUINO_CLI="$(ls <primary checkout>/tools/.cache/*/arduino-cli* | head -1)"
```

On Windows, build with `--out C:/tr-build/<name>`: the toolchain refuses the
long paths a worktree under `.claude/worktrees/` produces.

## 2. Implement

Follow `CLAUDE.md`. The rules that bite hardest, because nothing fails until a
board runs the image:

- **Read before changing.** `docs/Architecture/Constraints.md` for anything near
  the PHI2 path, DMA or RAM placement, and the module's section of
  `Teensy-Firmware.md`. Update the architecture doc in the same change.
- **Use the skill that covers the job.** A new IO handler is
  `add-io-handler`; a new menu program is `add-menu-program`; building and
  flashing are `teensyrom-build`.
- **Both images, both boards.** Grep for every other site of what you changed:
  the MinimalBoot twin of a full-firmware function (`Min_*.ino`), and the TR and
  TR+ sides of `Fab04_Features`.
- **Mind memory as you write.** `CLAUDE.md` → "The rules that bite hardest"
  covers ITCM, strings, tables and idle cost. The diff cannot show ITCM
  growth, so step 7 compares the link reports before the PR is marked ready.
- **Every entry path and every failure path.** The same section lists them; a
  launch change is checked from each.
- **Findings are fixed, filed or dropped by `CLAUDE.md` → "Findings"**, and so
  is every subagent this run starts: its prompt says "follow CLAUDE.md →
  Findings" rather than restating the rules.
- **Prove a test can fail.** Before claiming a test pins a behavior, break the
  line it covers, watch it fail, and restore it.
- **Check a claim before writing it.** A doc sentence or comment that names
  code is checked against the code first.

## 3. Commit, then review the change

Commit related changes together, with a message that says why. Separate
commits for separable concerns. Before each commit run the gates in
`CLAUDE.md` → "Gates" that the change calls for.

Review each change once, at the effort `CLAUDE.md` → "Reviews" gives it. Count
changed code lines with `git diff --numstat <base> <tip> -- ':!*.md'`. Run the
project's `reviewer` agent, which works in the worktree you give it:

    Agent(subagent_type="reviewer", description="<short-sha>",
          prompt="Worktree: <absolute worktree path>\nTarget: <sha or BASE..TIP>\nEffort: <level>")

When the user has authorized hardware for this work, add a fourth line, for
example `Hardware: authorized; board TR+; machine C128 NTSC`. Never commit the
port or the machine's address.

Do not wrap `/code-review` in a general-purpose agent: the wrapper repeats the
setup and the gates, and the `/code-review` fork resolves the primary checkout
rather than your worktree, so it can review a different branch and report it
as yours.

- **A small branch gets one review.** A branch that is one change under about
  300 changed code lines is reviewed once, as `origin/main..HEAD`, when it is
  done; that review is the final pass too.
- **The reviewer's fix commit gets one `low` review, and that ends the chain.**
- **Do not commit or edit in the worktree while a reviewer runs.** It mutates
  the tree to prove findings.

When it reports, confirm its fix commit is on your branch
(`git branch --contains <fix-sha>`), re-run the gates, and route what it handed
back: an advisory that is clearly your call is fixed here as its own commit; a
design decision goes to the user; an "outside this change" item is filed as its
own issue only if it names the caller, the input and the wrong result.

## 4. Final pass

Only when the branch had **two or more** reviewed changes, once, at `medium`:

    Agent(subagent_type="reviewer", description="<branch-slug> final",
          prompt="Worktree: <absolute worktree path>\nTarget: origin/main..HEAD\nEffort: medium\nEvery change on this branch has had its own review. Look for interactions between them, not for defects inside one diff.")

**A firmware PR headed upstream gets a final pass at `high` instead,** whatever
the number of reviewed changes: the `reviewer` agent with
`Target: origin/main..HEAD` and `Effort: high`, in place of the `medium` pass
above. Its memory comparison is step 7's, and its hardware list is step 5's.

A defect still open afterwards is a stop: report it and ask before opening a PR.

## 5. Open the PR as a draft

```bash
git push -u origin HEAD
gh pr create --draft --title "<type>: <what changed>" --body-file <file>
```

The body says what the change does and why, and:

- **What ran on hardware**, across these axes: board (TR, TR+), machine (C64, C128), PAL or NTSC, Full or MinimalBoot, the source
  (SD, USB, TR menu, remote launch), the failure paths (no card, missing
  folder, bad image), a replug or card pull, a second launch, and a re-run
  after each fix, each with the commit it ran on. Run what is cheap before
  asking for review. List what ran, not what didn't: an axis left untested is
  simply absent, never a follow-up. With no hardware run at all, say "host
  build only, not hardware tested".
- **Memory impact** for a firmware change, from step 7.
- **Anything downstream tools would notice:** a remote-protocol, register-map
  or EEPROM-layout change, called out on its own line.

Nothing else: no declined findings, no review log, no follow-ups. A follow-up
is an issue by now, which the body may link.

## 6. Watch CI until green

```bash
gh pr checks --watch
```

CI runs the tool tests, the extension conformance suite, the bench tests and
both firmware builds. A failure that is a real defect gets its fix reviewed by
step 3's rules, not quietly patched.

## 7. Before Ready

Answer in the PR what a review will ask, before asking for one. CONTRIBUTING.md's
"Before you open a PR" checklist applies; these go further.

- **Memory, base against head.** CI's build log prints the memory report for
  each image under `[minimal]`, `[main]` and `[extension]`, so no local build
  is needed: compare this PR's head run with the run on `main` at the merge
  base (`gh run list -w build.yml --commit <sha>`, then `gh run view <id>
  --log`). Compare the main image on TR and TR+, and MinimalBoot and the
  extension image when the change touches them: `Source/Teensy/MinimalBoot/`
  builds into MinimalBoot, and it or `Source/Teensy/VMBoot/` into the TR+
  extension image. In the description, give one table per target and image,
  each naming its image, with base, head and change for RAM1 code, RAM1
  variables, RAM1 padding, free for local variables, RAM2 and flash.
- **ITCM growth gets `FLASHMEM` first.** Anything that grew RAM1 code and no
  ISR reaches is marked `FLASHMEM` before the PR is marked ready, not left for
  the maintainer to ask for; that commit is reviewed by step 3's rules and the
  figures re-taken from its CI run. When the cause is not obvious, build both
  sides locally with `--skip-combine` and diff `arm-none-eabi-nm -S
  --size-sort -C` of their ELFs: ITCM symbols sit at `0x0000_0000`, DTCM at
  `0x2000_0000`.
- **Re-read the words after every fix round,** whether the fix came from a
  review, CI or a comment: the title, the description and the code comments,
  against the final code. The title becomes the merge commit's subject, and a
  memory figure that no longer matches the head, or that names no image, is a
  false claim.

Once every check is green and nothing is left to do, mark it ready:

```bash
gh pr ready
```

If more work turns up after that, `gh pr ready --undo` puts it back to a draft.

## 8. Stop

Report to the user:

- The PR URL, its check status, and that it is marked ready.
- Each review that ran: target, effort, what it fixed and what it declined, with
  reasons.
- Advisories and where each went, issues filed, and the hardware testing still
  needed.

Then stop. The merge is the maintainer's.
