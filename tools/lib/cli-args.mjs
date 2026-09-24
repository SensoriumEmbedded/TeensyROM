// One pass over argv, so an option's value is never mistaken for an option and a repeat
// cannot be quietly dropped.
//
// This was written in build-firmware.mjs, for a failure the other build scripts share.
// Reading each option with its own indexOf() takes the *first* occurrence and ignores the
// rest, which is the wrong end for the one shape that produces a repeat in practice:
// `npm run <script> -- --opt <value>` appends the caller's argument after the script's
// own, so the caller's override loses and the baked-in value wins without a word.
// package.json bakes four options into build:hello, so
// `npm run build:hello -- --id OTHER` builds HELLO and exits 0.
//
// An unrecognised argument is refused for the same reason: a misspelling is silent
// otherwise, and the thing it failed to change is the thing the caller asked for.
// Refusing costs a caller who meant to override nothing but naming the script directly.
export function scanArgs(args, { options = [], repeatable = [], flags = [] } = {}) {
  const single = new Set(options);
  const many = new Set(repeatable);
  const bare = new Set(flags);
  const values = new Map();
  const lists = new Map(repeatable.map((name) => [name, []]));

  const known = () => [...single, ...many, ...bare].sort().join(' ');

  for (let i = 0; i < args.length; i++) {
    const name = args[i];

    if (single.has(name) || many.has(name)) {
      const value = args[i + 1];
      if (value === undefined || value.startsWith('--')) throw new Error(`Missing value for ${name}`);
      if (many.has(name)) {
        lists.get(name).push(value);
      } else if (values.has(name)) {
        throw new Error(`${name} given more than once (${values.get(name)}, then ${value}). ` +
          'Only one can take effect and the other would be ignored silently, so neither is. ' +
          `If this came from \`npm run <script> -- ${name} ${value}\`, the script already passes ` +
          `${name}; run the tool directly instead.`);
      } else {
        values.set(name, value);
      }
      i++;
      continue;
    }

    if (!bare.has(name)) throw new Error(`Unknown argument ${name}. Known arguments: ${known()}`);
  }

  return {
    // Every accessor runs after the scan above, so these read a finished map.
    option: (name, fallback = null) => (values.has(name) ? values.get(name) : fallback),
    all: (name) => lists.get(name) ?? [],
    flag: (name) => bare.has(name) && args.includes(name),
    given: (name) => values.has(name) || (lists.get(name)?.length ?? 0) > 0 || args.includes(name),
  };
}
