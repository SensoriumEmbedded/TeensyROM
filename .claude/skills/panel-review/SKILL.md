---
name: panel-review
description: "Review one commit with four specialist lane reviewers in parallel — Auditor (correctness and missing bounds), Adversary (abuse and availability), Steward (contract and test drift), Pragmatist (structure). Each lane fixes what it finds in its own lane. An optional choice, in place of the reviewer at high, for a change of 50+ changed code lines that crosses a trust boundary, changes a wire format/public API/persisted data, changes concurrency, or changes behavior in a pinned path; or for an unsplittable change over 400 lines. The panel replaces the single reviewer for that change, never runs on top of it. For an ordinary change use the reviewer agent instead."
---

# Panel review

Four reviewers, one commit, disjoint lanes. The point is not more eyes — it is
that each lane has a scope narrow enough to be answerable, and evidence rules
that make a finding checkable by someone who does not own the lane.

This is the expensive path. The `reviewer` agent is the default for every
change; a panel at default models costs roughly a whole session, and the
escalation is a claim about the commit rather than about the code's quality.
It **replaces** the reviewer for this commit: do not also run the reviewer on
the same SHA, before or after. Never run two panels at once, or a panel beside
another heavy agent.

## When a panel is an option

A panel is an optional choice of review for one logical change, in place of
the `reviewer` agent at `high`. It is never a stage every change goes
through. These thresholds are the only place the choice is defined; other
docs point here.

*Changed code lines* is the added + deleted count from
`git diff --numstat <base> <tip> -- ':!*.md'`.

A change is **eligible** when it has at least **50 changed code lines** and
at least one of these is true:

1. **Trust boundary.** It changes authn or authz, a token or secret, or how
   input from the network or another user is parsed or checked.
2. **Contract others depend on.** It changes a wire format, a public API, or
   persisted data (a migration, a deletion, a format change).
3. **Concurrency.** It adds or changes a lock, a thread, or state shared
   between threads.
4. **Pinned path.** It changes behavior (not only names or layout) in a path
   the project pins as high-risk.

A change of more than **400 changed code lines** that cannot be split is also
eligible. If it can be split, split it instead.

**Choosing.** For an eligible change, pick the reviewer at `high` or a panel.
Prefer the panel when two or more of the criteria above apply. Say which one
you picked, and which threshold made the change eligible, in the report to the
user.

**Never a panel:** a change below 50 changed code lines (the reviewer at `high`
covers a risky small one), a prose-only or trivial change, a merge resolution,
a reviewer's fix commit, or a lane's fix commit.

**In TeensyROM, the criteria name these places:**

- **Trust boundary:** the remote protocol (`SerUSBIO.ino`, `Min_SerUSBIO.ino`,
  `ServiceTCP.ino`, `Min_ServiceTCP.ino`, `RemoteControl.ino`); parsers of
  files from SD, USB or the Ethernet browser (`FileParsers.ino`, `D64.ino`,
  `DriveDirLoad.ino`, `Swift_Browser.c`); NFC (`nfcScan.ino`); and the firmware self-update
  (`FlashUpdate.ino`, `Flash/`).
- **Contract:** the remote protocol in `docs/ControlComms.md`; the register map
  in `Menu_Regs.h`; the EEPROM layout and IO handler numbering, which persist
  across a flash; the image layout the boot linker files and
  `tools/lib/hex.mjs` define.
- **Concurrency:** the PHI2 interrupt against thread mode, which is this
  codebase's form of it: state an ISR-path handler shares with `PollingHndlr`
  or a command handler, `fBusSnoop` ownership, interrupt masking, and the DMA
  state machine.
- **Pinned path:** the list in `CLAUDE.md` under "Reviews".

## Target

One changeset, never a branch. `$ARGUMENTS` is a SHA, a ref, or empty for the
working tree. Resolve it once and hand every lane the identical target — lanes
reviewing different diffs cannot corroborate each other.

```
git show --stat <target>          # what changed
git diff <target>^ <target>       # the diff every lane reviews
git log -1 --format=%B <target>   # the author's own claims about it
```

If `$ARGUMENTS` names a branch or a range, stop and say so: pick a SHA, or run
the branch-wide pass separately as the second net before the PR.

Lane fix commits are not reviewed by another panel. Each gets one `low` pass
from the `reviewer` agent, and that ends the chain.

## Lanes

Run all four by default — the caller already decided this commit is worth it.

| Lane | Owns | Model |
|---|---|---|
| `auditor` | Correctness, logic, algorithmic soundness, operations with no bound | opus |
| `adversary` | Abuse, trust boundaries, availability under load | opus when the diff crosses a trust boundary or authn/authz; sonnet otherwise |
| `steward` | Contract drift (docs, schemas, generated files), tests as claims | sonnet |
| `pragmatist` | Structure, coupling, design fit — advisory only | sonnet |

Skip a lane only when the diff gives it literally no surface: no docs, schema,
generated artifact or test touched (Steward), or no new abstraction, widened
surface or grown unit (Pragmatist). **Never skip the Auditor** — it is the lane
that runs unconditionally, and the other lanes' scope rules are written assuming
it ran.

Skipping the Adversary is the consequential one. It owns the rating of an
availability bound because it has the evidence the code cannot supply — the load
this deployment sees, and whether anyone can drive it. When it does not run, the
Auditor's bound findings arrive rated from the code alone and nothing supplies
the other half; say that in the report rather than letting the rating stand
unqualified.

## Phase 1 — analyse, in parallel, read-only

Launch every selected lane in one message so they run concurrently. Each gets
the resolved target and an explicit **report only, do not touch the tree**.
Set each launch's `description` to the short SHA alone (`a1b2c3d`): the lane
name already prints beside it.

The read-only instruction is the whole reason this phase parallelizes. Four
agents writing one worktree at once corrupt each other's edits, and an agent
that fixes a file another lane is mid-review of invalidates that review.

Each lane's prompt:

```
Review <target> in your lane only. Report only — do not edit, do not commit.
Diff: git diff <target>^ <target>
For each finding: file:line, severity, the mechanism, the evidence your lane
requires, and a proposed fix precise enough for you to apply later.
If the change is sound in your lane, say so and say what you verified.
```

When the user has authorized hardware for this work, add the same
`Hardware: authorized; board <TR|TR+>; machine <model, PAL|NTSC>` line the
reviewer gets, and name **one** lane that may use the board. Nothing serializes
access to a board, and two lanes flashing or poking it at once leave neither
result meaningful. The other lanes report a claim only a board can settle as
an open question.

## Phase 2 — merge the findings

Before anyone writes, reconcile. Two lanes reporting the same thing in their own
words is the signal this shape produces; two lanes reporting it because one wrote
the other's half is a counterfeit of that signal.

- **A missing bound reported twice is one finding, not two.** The Auditor owns
  the mechanism (no deadline, no ceiling, no limit); the Adversary owns the load
  story and the rating. Merge them, keep both halves, and take the Adversary's
  severity — it is the lane holding the evidence.
- **The same defect from two lanes independently** stays one finding and gets
  the worse severity. Note that both lanes reached it; that is the strongest
  result a panel produces.
- **A lane reporting outside its scope** gets dropped, not rerouted. If it looks
  real, hand it to the lane that owns it in phase 3 as an explicit question.
- **Advisory findings** — the Pragmatist's whole lane, the Steward's contract
  findings — are separated out here. They are for the author, not for the gate.

## Phase 3 — fix, sequentially, by the lane that found it

Serialize the writes. Continue each lane with `SendMessage` so it fixes with its
own analysis intact — the agent that found the defect writes the patch, and
nothing is transcribed through this session. Rewriting a lane's fix from its
description is new code, new code earns a review, and that is the loop that
spends hours on a minor change.

Order matters: **Auditor → Adversary → Steward → Pragmatist.** Correctness and
abuse fixes change behaviour, and changed behaviour changes what the docs and
tests should say, so the Steward runs after them and reviews the tree as
repaired rather than as submitted. The Pragmatist is last and advisory.

Each continuation carries the merged findings so a lane adds to an existing
finding instead of re-filing it:

```
Apply your findings now, in the working tree. One commit per lane, never an
amend of <target> — amending changes its SHA and the review it passed points at
nothing. Fix every site, not the cited one; the class not the instance, and no verification you did not run.
Re-run the reproduction after the fix. Report what you fixed, what you declined,
and why.
Findings merged across lanes, including yours: <merged list>
```

Two classes never go to a lane, because a single-commit reviewer cannot see what
they need:

- **Commit-message rewrites.** They change the SHA and invalidate every recorded
  review of that commit and of everything stacked on it. Report what the message
  should have said; the decision is the author's.
- **Editorial judgment across the change.** Whether to repair a sentence or cut
  its paragraph needs the whole-branch view. Five weak claims across four
  commits is a fact about claim density, invisible from inside one commit.

## Phase 4 — report

Write it where the next reader finds it: in the report that reaches the user, not
in a commit message. Prose a process depends on is prose that gets argued about.

```
## Panel review — <target>: <subject>
Lanes: auditor, adversary, steward, pragmatist  (skipped: <lane> — <why>)

### Fixed
- [critical] <file:line> — <mechanism> · <lane> · <commit>  (reproduction: <what ran, what changed>)

### Advisory — for you to accept or decline
- [warning] <file:line> — <future cost> · pragmatist

### Declined
- <finding> — <why>

### Verified clean
- auditor: <what it checked and what held>
```

State a lane's conclusion as that lane's, not as the panel's. Four lanes
approving is four narrow verdicts, not one broad one — and say plainly when a
clean result rests on a lane that did not run.

## Failure modes

- **A lane that reports nothing** has either found nothing or misread its scope.
  A lane must say what it verified; an empty report with no verification list is
  a lane that did not run properly, so rerun it.
- **Finding volume from an advisory lane** dilutes the rest. The Pragmatist owes
  a named future cost per finding; drop the ones without one.
- **A clean panel on a large diff** is the same suspicious result that motivated
  the escalation. Say so rather than reporting confidence you did not earn.
