// SPDX-License-Identifier: MIT
//
// Checks a value against the JSON Schema keywords the tools' outputSchemas
// use. Throws on a keyword it does not know, so a schema can't outgrow it.

const KNOWN = new Set([
  'type', 'enum', 'properties', 'required', 'additionalProperties', 'items', 'minItems', 'maxItems',
  'description',
]);

function typeOf(value) {
  if (value === null) return 'null';
  if (Array.isArray(value)) return 'array';
  if (Number.isInteger(value)) return 'integer';
  return typeof value;
}

function typeMatches(value, type) {
  const types = [type].flat();
  const actual = typeOf(value);
  return types.includes(actual) || (actual === 'integer' && types.includes('number'));
}

function checkObject(value, schema, at, problems) {
  for (const name of schema.required ?? []) if (!(name in value)) problems.push(`${at}.${name}: missing`);
  for (const [name, item] of Object.entries(value)) {
    const property = schema.properties?.[name];
    if (property) check(item, property, `${at}.${name}`, problems);
    else if (schema.additionalProperties === false) problems.push(`${at}.${name}: not in the schema`);
  }
}

function checkArray(value, schema, at, problems) {
  if (schema.minItems !== undefined && value.length < schema.minItems) problems.push(`${at}: fewer than ${schema.minItems}`);
  if (schema.maxItems !== undefined && value.length > schema.maxItems) problems.push(`${at}: more than ${schema.maxItems}`);
  if (schema.items) value.forEach((item, index) => check(item, schema.items, `${at}[${index}]`, problems));
}

function check(value, schema, at, problems) {
  const unknown = Object.keys(schema).filter((keyword) => !KNOWN.has(keyword));
  if (unknown.length) throw new Error(`${at}: schema keyword ${unknown.join(', ')} is not checked`);
  if (schema.type && !typeMatches(value, schema.type)) {
    problems.push(`${at}: ${typeOf(value)}, want ${[schema.type].flat().join(' or ')}`);
    return;
  }
  if (schema.enum && !schema.enum.includes(value)) problems.push(`${at}: ${JSON.stringify(value)} not in enum`);
  if (typeOf(value) === 'object') checkObject(value, schema, at, problems);
  if (typeOf(value) === 'array') checkArray(value, schema, at, problems);
}

export function schemaProblems(value, schema) {
  const problems = [];
  check(value, schema, '$', problems);
  return problems;
}
