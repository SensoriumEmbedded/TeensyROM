// SPDX-License-Identifier: MIT
// Retired in compiled-library builds. Fail before reading any source checkout,
// resolving a revision, or writing files, including when the package is absent.
// mpe/source-lock.json remains the historical source-based build record.
throw new Error(
  'Source import is disabled for the compiled MPE host integration. ' +
  'Update the licensed archive, public interface, manifest and matching relink ' +
  'SDK in mpe/library as a reviewed package. Do not import a private host ' +
  'checkout. mpe/source-lock.json documents the historical source-based host.'
);
