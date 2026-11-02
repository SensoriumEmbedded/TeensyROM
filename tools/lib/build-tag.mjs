// SPDX-License-Identifier: MIT
//
// What marks a special build apart from a release, decided here with no git or file
// system in reach so it can be tested: the --tag name build-firmware.mjs appends to the
// version string and to the output names, the dirty-tree marker it adds on its own, and
// which build date each kind of tree gets. The firmware's half is TRVersionSuffix in
// Common_Defs.h, which the define below sets, and MakeBannerRow in IOH_TeensyROM.c, which
// lays the version string out on the C64's banner row.

// Letters, digits, '.' and '-'. The C64 prints the tag through ToPETSCII(), which has no
// '_' (it comes out as a graphic), and the tag is also part of a filename.
export const TAG_PATTERN = /^[A-Za-z0-9.-]+$/;

// The C64 banner row MakeBannerRow fills: the version string, and a page's word while it
// still fits beside it. The version alone has to fit, and that is the whole tag budget.
export const ROW_WIDTH = 40;

// The word MakeBannerRow adds to each built-in screen's row. A version long enough to push
// one past ROW_WIDTH makes that screen show the version alone, which the build points out.
export const PAGE_WORDS = [['Settings', ' Settings'], ['Help', ' Help'], ['port tests', ' Test']];

// What git status is asked about: the files compiled or linked into the image. Docs and
// the build tools themselves do not change the hex, and Fab04FeatureCtl.h is a toggle
// commonly left modified in a working tree (and edited by --yes), so none of those count.
export const DIRTY_PATHSPEC = ['--', 'Source/', 'tools/BootLinkerFiles/', 'tools/lib/extension-image.mjs',
  ':(exclude)Source/Teensy/MinimalBoot/Common/Fab04FeatureCtl.h'];

export function versionBase(target, trVersion) {
  return `${target === 'tr-plus' ? 'TeensyROM+' : 'TeensyROM'} v${trVersion}`;
}

// The longest tag that keeps the whole version string inside the row; the '-' before the
// tag and the '*' of a dirty tree take their own columns.
export function tagLimit({ target, trVersion, dirty }) {
  return ROW_WIDTH - versionBase(target, trVersion).length - 1 - (dirty ? 1 : 0);
}

export function validateTag(tag, { target, trVersion, dirty }) {
  if (tag === '') throw new Error("--tag is empty; give it a name (letters, digits, '.' and '-') or leave it out");
  if (!TAG_PATTERN.test(tag)) {
    throw new Error(`--tag ${JSON.stringify(tag)} may only use letters, digits, '.' and '-' ` +
      '(the C64 shows an underscore as a graphic character)');
  }
  const limit = tagLimit({ target, trVersion, dirty });
  if (tag.length > limit) {
    throw new Error(`--tag ${JSON.stringify(tag)} is ${tag.length} characters; ${limit} is the most that fits ` +
      `after "${versionBase(target, trVersion)}-"${dirty ? ' and the dirty-tree marker' : ''} ` +
      `in the ${ROW_WIDTH}-column C64 banner`);
  }
  return tag;
}

// On the C64 and in the serial replies: "-pr65*".
export function bannerSuffix({ tag, dirty }) {
  return (tag ? `-${tag}` : '') + (dirty ? '*' : '');
}

// In the hex and .TRH names: "-pr65-dirty" ('*' is not a filename character).
export function fileSuffix({ tag, dirty }) {
  return (tag ? `-${tag}` : '') + (dirty ? '-dirty' : '');
}

// The define as it goes into build.flags.defs. arduino-cli splits the expanded recipe on
// spaces with ' and " as grouping quotes that it removes, so the ' pair is what delivers
// the " pair to the compiler; the Arduino AVR core passes its USB strings the same way
// ('-DUSB_MANUFACTURER="Unknown"'). Nothing for a release build, so its command line is
// unchanged.
export function suffixDefine(suffix) {
  return suffix ? `'-DTRVersionSuffix="${suffix}"'` : '';
}

// The screens whose word no longer fits beside a version string of this length.
export function pagesWithoutWord(versionLength) {
  return PAGE_WORDS.filter(([, word]) => versionLength + word.length > ROW_WIDTH).map(([page]) => page);
}

// The build date, as SOURCE_DATE_EPOCH. One already in the environment wins (the
// reproducible-builds convention). A clean tree is stamped with its commit, so rebuilding
// a commit reproduces its hex and the date names the commit. A dirty tree is stamped with
// its own time: the commit's date would name an image this is not, and it is what lets two
// local iterations be told apart. No commit at all (not a checkout) is stamped with now.
export function chooseEpoch({ envEpoch, commitTime, dirty, now }) {
  if (envEpoch) return { epoch: envEpoch, source: 'environment' };
  if (!/^\d+$/.test(commitTime ?? '')) return { epoch: String(now), source: 'no git commit' };
  if (dirty) return { epoch: String(now), source: 'dirty tree' };
  return { epoch: commitTime, source: 'commit' };
}

// git status --porcelain's answer, from spawnSync. No git at all (the spawn failed) or no
// checkout around the tree ("not a git repository", GIT_DIR pointing nowhere included)
// counts as a clean tree: that is a build from an export, the same degrade the commit-time
// lookup has. Any other failure (a corrupt index, "dubious ownership" of the checkout, an
// unreadable .git) is a tree git could not read, and a clean stamp on it would be the
// misreading the marker exists to prevent, so the build is marked dirty and git's message
// is passed back for the build log.
export function dirtyFromStatus({ status, stdout, stderr, error }) {
  const said = (stderr ?? '').trim();
  if (error || /not a git repository/.test(said)) return { dirty: false, files: [], problem: '' };
  if (status !== 0) return { dirty: true, files: [], problem: said || `git status exited with ${status}` };
  const files = (stdout ?? '').split(/\r?\n/).filter(Boolean).map((line) => line.slice(3).trim());
  return { dirty: files.length > 0, files, problem: '' };
}
