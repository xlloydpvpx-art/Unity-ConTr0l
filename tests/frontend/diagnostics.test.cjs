const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(require('node:path').join(__dirname, '../../src/frontend/app.js'), 'utf8');
const match = source.match(/function summarizeFindings\(inventory\)\s*\{[^\n]*\}/);
assert.ok(match, 'summarizeFindings must exist in app.js');
const summarizeFindings = vm.runInNewContext(`(${match[0]})`);
assert.deepEqual(JSON.parse(JSON.stringify(summarizeFindings([
  { diagnostics: [{ severity: 'ERROR' }, { severity: 'WARNING' }] },
  { diagnostics: [{ severity: 'WARNING' }, { severity: 'INFO' }] },
  { diagnostics: null },
  {}
]))), { total: 4, errors: 1, warnings: 2 });
assert.deepEqual(JSON.parse(JSON.stringify(summarizeFindings([]))), { total: 0, errors: 0, warnings: 0 });
console.log('Frontend diagnostics aggregation tests passed.');
