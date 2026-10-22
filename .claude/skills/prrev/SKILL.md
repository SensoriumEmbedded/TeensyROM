---
name: prrev
description: Full review of a TeensyROM pull request. Pulls the PR into its prNN-check branch and opens it in Beyond Compare (bcbc), renames the session after the PR, runs a high code review and trims the findings to a recommended set, measures the PR's impact on RAM1 (ITCM code and DTCM variables/stack), RAM2 and flash for TR and TR+, and lists the hardware testing still needed. On request it then drafts the response to post on the PR. Use whenever the user says "prrev" (normally with a PR number) or asks for a full review of a PR in this repo.
---

# prrev: PR review

The argument is a PR number (`61` or `#61`), optionally followed by an effort level for the
code review (`prrev 61 medium`). With no number, ask which PR.

A run has six parts, and a seventh on request:

| Section | Produces |
|---|---|
| 1. Context | earlier review notes, PR metadata, what the author says they tested |
| 2. bcbc and session name | `prN-check`, Beyond Compare open, session titled after the PR |
| 3. Code review | findings checked, rated and trimmed to a recommended set |
| 4. Memory | before/after RAM1, RAM2 and flash for TR and TR+, with the symbols behind it |
| 5. Hardware testing | what the author covered, what's still needed |
| 6. Report | the summary for the user, plus a review note for re-checks |
| 7. PR response | a draft for the user to post, only when asked |

## 0. Ground rules

- **Nothing outward.** No commits, pushes, PR comments, messages or flashing. The user
  posts and sends things themselves.
- **Don't touch the user's checkout.** Builds run in throwaway worktrees; bcbc works on refs
  only.
- **Builds are part of this skill.** Invoking prrev is the user's request for section 4's
  builds, and only those. Never flash.
- **Private context stays with the user.** Use whatever private context you have: your
  memory notes, the user's own todo list and its item numbers, unpublished investigations,
  the user's machines and test rigs, local paths. Cite it freely in what you tell the user
  (sections 1 to 6). It never goes into anything other people will read: the PR response,
  a message to the PR's author, issue comments, commit messages, PR bodies or docs.
  Section 7 says how to keep it out.
- **Keep the user posted.** Keep a task list (one step per section) and post a one-line
  progress note between sections. A full run takes a while.
- **GitHub.** Use `gh` if it's installed. Otherwise use the API with curl, unauthenticated
  (60 requests an hour): `https://api.github.com/repos/<owner>/<repo>/...`, with the owner
  and repo from `git remote get-url origin`.

## 1. Context

- **Earlier notes.** If you keep a persistent memory for this project, read its index and
  any note about this PR (such as `project_prN_review.md`), or about a stack of PRs it
  belongs to.
- **A re-review.** If the PR was reviewed before, note the old head and the earlier findings.
  Review the whole PR again, but report which earlier findings are fixed, still open or new.
  If the old head is still in the local object store, `git diff <old head> origin/pr/N` shows
  what changed since.
- **PR metadata**, from the API:
  - `pulls/N`: title, author, head sha, body, mergeable state.
  - `pulls/N/commits`, `issues/N/comments`, `pulls/N/comments` (inline), `pulls/N/reviews`.
  - `commits/<head sha>/check-runs` for CI.

  Keep the body and the comments: section 5 needs what the author tested.
- **Stacked PRs.** If the body says the PR is stacked on another one, or its commits include
  another open PR's head, its diff against main contains that PR too. Ask whether to review
  only this PR's own delta or the whole thing. For a stack, a shared review branch with one
  `--no-ff` merge per PR, in stack order, lets each one be reviewed as `merge^1..merge`.

## 2. bcbc and the session name

- **bcbc.** Invoke the `bcbc` skill with the PR number. It fetches `origin/pr/N`, creates or
  refreshes `prN-check` (main plus a merge of the PR) and opens BC in the background. From its
  report keep the review branch tip `B`, its base `B^1`, the file count, and three cases:
  - **Local main is behind origin/main:** say so. The review is against local main. Don't
    update main.
  - **The merge conflicts:** bcbc makes no branch. Review `main...origin/pr/N`, build the PR
    head against `git merge-base main origin/pr/N` in section 4, and make "doesn't merge
    cleanly with main" (with the files) a top finding.
  - **Already merged:** say so and ask whether to review the merge anyway.
- **Session name.** As soon as the PR title is known, rename the session to
  `PR #N: <PR title>`, for example "PR #39: Bound C64- and card-driven buffers and indices".
  Trim a long title to its gist.
  - In the Claude desktop app, use `mcp__ccd_session_mgmt__set_session_title` with
    `session_id: "self"`. If the app asks the user to approve the new title, that's expected.
  - Where no rename tool exists, skip this step.

## 3. Code review

### Run it

1. **Start section 4's builds first.** They run in the background during the review.
2. **Invoke the `code-review` skill with an explicit level:** `high prN-check`, or the level
   the user gave. For a stacked delta or a conflicting PR, pass the range instead
   (`<merge>^1..<merge>`, `main...origin/pr/N`).
   - Never call it without a level. It reuses the last level typed, and a `max` review of a
     large PR has taken over an hour and millions of tokens.
   - Use `max` only when the user asks, after saying it means dozens of agents and likely
     over an hour.
   - Don't pass `--comment` or `--fix`.

### Check each finding

- **Verify before keeping.** Read the code at the cited lines. Check claims about ROMs, C64
  code or bus timing against the real thing: actual ROM bytes (your memory may say where
  local copies of the kernal images are), the `Source/C64` assembly, the disassembly.
  Findings that look solid in the firmware source alone have been withdrawn this way:
  - A ROM signature "false positive" that the real ROM's bytes ruled out.
  - An input length the C64 side can't actually send.
- **Timing claims.** Before calling a change in the PHI2 path a timing risk, count the
  instructions before the data-bus write in the disassembly of both sides. Compare that with
  the margin in Constraints.md ("An IO1 read has to put its data on the bus early"), about
  50 ns. A few cycles at 816 MHz is a few nanoseconds. Don't overstate it.
- **Where it came from.** Classify each finding as one of:
  - New in this PR.
  - A regression of something that worked on main.
  - Pre-existing, and the PR touches it or only half-fixes it.
  - Pre-existing and untouched.

  Code that was already in RAM on main and only grew isn't the PR's issue. Mention it as an
  FYI at most.

### Trim to a recommended set

Rate each finding on three axes:

| Axis | High | Medium | Low |
|---|---|---|---|
| Severity | crash, hang, data loss, PHI2 timing | wrong behaviour users will hit | edge cases, messages, docs, tests |
| Fix complexity | redesign, protocol or C64-side change | several functions | a few lines in one place |
| Memory cost of the fix | DTCM (new globals, strings, tables) or ITCM (ISR-reachable or non-FLASHMEM code) | RAM2 | none, or flash only (FLASHMEM thread code) |

A fix that costs RAM1 needs a matching severity. Then sort every finding into one group:

- **Recommended to fix in this PR:** the severity justifies the complexity and the memory.
- **Hardware check only:** a risk that a test settles better than a code change.
- **Optional or follow-up:** real but not worth it now, or it belongs in another PR.
- **No change needed:** withdrawn, or fine on inspection.

Keep the recommended group short. It's what the contributor will be asked to do.

## 4. Memory

Build both sides yourself: `B^1` (main) and `B` (`prN-check`), from their shas. Never
compare against numbers in docs or notes. Those are dated measurements of other trees.

### What to build

| The diff touches | Build |
|---|---|
| Nothing the firmware compiles: docs, `tools/`, tests, CI, C64 source without regenerated headers | Nothing. Say why memory is unchanged. |
| `Source/Teensy/` | The main image, for both TR+ and TR. |
| `Source/Teensy/MinimalBoot/` | Also MinimalBoot, for both. (Its `Common/` is compiled into the main image too.) |
| `Source/Teensy/MinimalBoot/` or `Source/Teensy/VMBoot/` | Also the extension image (TR+ only). It is the MinimalBoot sketch with VMBoot overlaid. |

Skip what isn't needed with `--skip-minimal-build` and `--skip-extension-build`. Always pass
`--skip-combine`, which keeps each build's `run-XXXX` folder and the ELFs in it.

### Run the builds

Use a short folder outside the repo, because the Windows toolchain refuses long paths, such
as `C:/tr-build/rN` (or wherever the user keeps builds). Make a fresh subfolder each time
and never overwrite older builds there.

```
REPO=$(git rev-parse --show-toplevel)        # the user's checkout, before any cd
R=C:/tr-build/rN
git worktree add --detach C:/tr-build/wNm <B^1 sha>
git worktree add --detach C:/tr-build/wNp <B sha>
ls "$REPO"/tools/.cache/*/arduino-cli*       # the builder's cached copy, if there is one
```

Write the loop to `$R/run.sh` and run it in the background:

```
#!/bin/bash
set -u
export ARDUINO_CLI="<the cached arduino-cli found above>"
for t in tr-plus tr; do
  for s in m p; do
    ( cd C:/tr-build/wN$s && node tools/build-firmware.mjs --target $t --force --skip-combine <skips> --out $R/$s-$t ) > $R/$s-$t.log 2>&1
    echo "$s-$t exit=$?"
  done
done
```

- **`ARDUINO_CLI`.** A fresh worktree has no `tools/.cache`, and on Windows the builder's own
  download fails under Git Bash's tar ("Cannot connect to C:"). So point `ARDUINO_CLI` at the
  copy in the user's checkout. With `arduino-cli` on `PATH` instead, leave it unset.
- **One build at a time.** Each takes one to three minutes, much longer with Arduino IDE open.
  `--target tr` has no extension image and ignores `--skip-extension-build`.
- **Plain TR.** The committed `Fab04FeatureCtl.h` has the define commented out, so
  `--target tr` needs nothing. If the PR changes that, pass `--yes`. It edits only the
  worktree's copy.
- **A build fails.** Show the error, and check whether the same side of main fails too. If
  only the PR fails, that's a top finding. So is a PR that trips a build guard: the
  heavy-libc check, the MinimalBoot free-for-locals floor or the flash headroom check.
- **Afterwards.**
  - Copy each `*.ino.elf` to `$R/elf/<side>-<target>/`.
  - Delete the `run-XXXX` folders, about 400 MB each.
  - `git worktree remove` both worktrees.
  - Keep the logs and the ELFs.

### Report the numbers

Give one table per target and image (main → PR, change), with every number from the report:

| Line | What it is |
|---|---|
| RAM1 variables | DTCM: globals, `.data` (const strings live here too), `.bss` |
| RAM1 code | ITCM: everything not marked FLASHMEM |
| RAM1 padding | ITCM rounded up to the next 32 KB bank: room for more code before the next bank |
| RAM1 free for local variables | DTCM stack: the crash-risk number |
| RAM2 variables, free for malloc/new | OCRAM: DMAMEM and the heap |
| FLASH code, data | the image in flash |

This table stands in for the verbatim report blocks that the teensyrom-build skill asks for.
Give the log paths.

How to read it:

- **RAM1 banks.** RAM1 is one 512 KB pool of 16 × 32 KB banks, split between ITCM and DTCM at
  link time. Code growth eats padding first. Crossing a bank boundary takes 32 KB from
  free-for-locals in one step. Say how much padding is left and how close the PR brings
  each image to the next bank. Plain TR main has tended to have the least.
- **DTCM steps.** Alignment (`_VectorsRam` on 1 KiB, `periodictable` on 4 KiB) turns small
  growth into a 1 KB or 4 KB jump. Under 1 KB of new strings can cost 4,096 B of
  free-for-locals. Explain such a jump rather than charging the PR the whole step.
- **Floors.**
  - MinimalBoot: free-for-locals of at least 24,000. The build enforces it.
  - Main: a 24 KB floor, with a working target of about 56 KB (the floor plus one bank;
    see Constraints.md). Read it off the shipping TR+ build, which carries the extension
    loader.
  - Extension: ITCM code has a hard 64 KiB limit, so its padding is all the headroom it has.
  - Flash: the build's headroom check passes or fails. Report the code and data change.

### Find what grew

Use `arm-none-eabi-nm` from the Teensy toolchain, under the Arduino data folder
(`%LOCALAPPDATA%/Arduino15` on Windows, `~/.arduino15` on Linux,
`~/Library/Arduino15` on macOS) at `packages/teensy/tools/teensy-compile/<version>/arm/bin/`:

```
"$NM" -S --size-sort -C <elf>
```

Diff the symbols of both sides. The address says where a symbol lives: ITCM `0x0000_0000`,
DTCM `0x2000_0000`, RAM2 `0x2020_0000`, flash `0x6000_0000`. Name the main contributors to
each RAM1 change. Then look for improvements to suggest:

- **New or grown non-FLASHMEM code** that no ISR reaches is a FLASHMEM candidate.
  - Check the FLASHMEM list in `docs/Architecture/Known-Issues.md` first.
  - Check that FLASHMEM held: inlining into a non-FLASHMEM caller and templates can defeat it.
  - FLASHMEM on anything the PHI2 ISR reaches is a timing bug, not a saving.
- **New const tables and strings in DTCM** can move to flash (`PROGMEM`, `PSTR`, `F()`).
  The exception is anything the ISR reads, such as menu item names and ROM images served on
  ROML.
- **New large globals** can move to RAM2 (`DMAMEM`) or be allocated when used.
- **New newlib routines.** The build already refuses `sscanf`, `pow`, `strtod` and similar.
  Note smaller additions such as `snprintf` or `strlcpy`.

Memory improvements are requested changes, like the recommended items. Ask for each one before
merging, whether or not a floor is at risk, and say plainly when one is.

## 5. Hardware testing

- **What the author tested.** From the PR body, comments and commit messages, note for each
  test:
  - The board (TR or TR+) and the machine (C64 or C128, PAL or NTSC).
  - The commit tested, and the main it was based on.
  - Exactly what was checked. "The listing stayed readable" isn't "picking an item works".
- **What counts as done.** A test counts only if it ran on the current head, or on a head
  whose differences don't matter for it, and it covers what's needed. A test on an older base
  doesn't cover layout or timing effects of main's newer commits.
- **What's needed.** Derive it from what changed:
  - Each user-facing path the PR touches.
  - The failure path of each recommended fix, once fixed.
  - PHI2-adjacent changes on NTSC, after the static timing check in section 3.
  - The other axes the change can differ on: TR and TR+, Full and MinimalBoot, C64 and C128,
    SD, USB and TR-menu sources, remote launch from the app.
  - Settings that persist across a power cycle, and the downgrade path when EEPROM layout or
    IO handler numbering changes.
- **The list.** Split it into "covered by the author" (one line each) and "still needed",
  grouped by timing, functional, after-fix checks and crafted inputs.
- **Machines.** Don't prescribe a specific machine unless the test depends on it. Any NTSC
  machine is fine for an NTSC test. Telling the user which of their own machines showed a
  problem first is fine.

## 6. Report

This is for the user, so private context belongs here wherever it helps: related todo-list
items, earlier investigations, the user's machines, local paths.

1. **Header:** PR #N, title, author, head sha, review branch and base, files changed
   (+/−), CI state.
2. **Recommended changes:** a table of finding (with `file:line`), severity, fix size,
   memory cost of the fix, and where it came from. Then the other three groups as short
   lists, each item with its reason.
3. **Memory:** the tables, the main contributors, and any concerns or improvements.
4. **Hardware testing:** covered by the author, still needed.
5. **Next step:** ask whether to draft the PR response.

Then, if you keep a persistent memory, write or update the PR's review note
(`project_prN_review.md`, in the shape of earlier review notes) and its line in the index:

- The head and review-branch shas, and what the PR does.
- The findings and how they were sorted.
- The memory numbers, with the log folder.
- What the author tested, and what's still needed.
- Whether a response was drafted, finalized or posted.

Don't add anything to the user's todo list unless they ask.

## 7. PR response (only when asked)

The same rules apply to anything else meant for the PR's author or other contributors, such
as a direct message to the author.

- **Draft only.** Never post or send it. Give it in a ` ```markdown ` fence so the user can
  copy it.
- **Contents.**
  - The recommended changes, renumbered 1..n. For each: a heading saying what's wrong (with
    `file:line`), why it matters (a repro if there is one) and the suggested fix.
  - Memory: one short paragraph with the measured impact, then each improvement as its own
    bullet: the lever, what it saves (bytes, and which RAM), and why it's safe. Don't fold a
    saving into a passing sentence. These count as changes before merging: include them in
    the "N things" lead-in (for example "Besides the memory item above, one thing…"), and
    never say "nothing to change" or "ready as it stands" when a response has any.
  - Hardware testing still to do, after acknowledging what the author covered.
  - Leave out hardware-check-only items unless they need the author to act, and the optional
    and no-change items. The hardware-testing heading's framing covers them (see Tone).
- **Tone**, from the maintainer's own edits of an earlier draft:
  - Open the first response with thanks for the work itself, meaning what the PR adds and
    not just its testing or write-up. For example: "Thank you for the Geos64
    implementation, this is exciting!  :)"
  - "There are N things I'd like you to look at before merging." Not "I'd like changed".
  - "As always, feel free to push back on any that are inappropriate or would take
    significant memory/code to resolve."
  - The hardware-testing framing goes in the section's heading, not in a sentence under it:
    "### Hardware testing still to do: for one of us to *possibly* get to before release,
    not gates, and anything I noted that isn't listed is covered by your follow-ups or
    fine as it is". Every hardware heading carries it. Later responses in a set use the
    short form, which ends at "before release".
  - No offer-of-a-hand line ("If any of these would be easier for me to take on…").
  - Close with "Thank you!"
  - The voice is the user's ("I reviewed…"), addressed to the author.
  - **A set of related PRs reviewed together** (a stack, or a batch from one author): the
    author reads every response at once, so say the shared lines only once, in the first
    PR's response. Those are the review method, the push-back invitation, the long form of
    the hardware-testing heading, and "Thank you!". Every other response opens with one
    plain thank-you specific to that PR, and words its "nothing to change" or closing
    line differently from the rest of the set.
- **No private references.** This text is public.
  - No todo-list item numbers. GitHub also turns "#69" into a link to the repo's own PR or
    issue #69.
  - Nothing from memory notes or other unpublished investigations, no local paths, and none
    of the user's own machines or rigs.
  - Cite commits on main, PRs and doc sections instead (for example a commit hash, or
    Constraints.md's "An IO1 read has to put its data on the bus early"). If there's no
    published source, state the fact plainly without one.
  - Before handing it over, reread the draft for each of these.
- **Afterwards.** When the user approves or edits it, record that in the review note, and
  later whether it was posted.
- **Learn from the user's edits.** When the user rewords a draft, compare their version with
  yours. Point out any change that is a pattern rather than a one-off (a phrase they always
  swap, something they always add or cut) and offer to add it to the tone rules above, so
  the next draft starts there.

The shape, with the details filled in from the review:

```markdown
Thank you for <the work itself>, <a short, genuine reaction>!

I reviewed this merged onto current main (<sha>) and built both targets. As always, feel
free to push back on anything that's inappropriate or would take significant memory/code
to resolve.

Memory: <one or two sentences of measured impact>. <N> improvement(s):

- **<The lever>: <bytes saved, and which RAM>.** <Why it's safe.>

There are <N> things I'd like you to look at before merging.

### 1. <What's wrong> (`<file>:<line>`)

<Why it matters, and a repro if there is one.>

Suggested fix: <the fix>.

### 2. ...

### Hardware testing still to do: for one of us to *possibly* get to before release, not gates, and anything I noted that isn't listed is covered by your follow-ups or fine as it is

Your <tests> cover <what they cover>.

- <test>
- <test>

Thank you!
```

Responses the user approved and posted are in this skill's `references/` folder:
`example-response-pr39.md` and `example-response-pr40.md`. Both predate the current tone
rules: they thank the author for testing rather than for the work, frame hardware testing
in a sentence instead of the heading, and end with an "everything else" line (PR #40 also
with an offer of a hand). Where an example and the tone rules above differ, follow the rules.
