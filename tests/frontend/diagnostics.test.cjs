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
const gm = source.match(/function groupInterfaces\(list\)\s*\{[^\n]*\}/);
assert.ok(gm, 'groupInterfaces must exist in app.js');
const groupInterfaces = vm.runInNewContext(`(${gm[0]})`);
const mk = (iface, c) => ({ vid: '373E', pid: '0047', product: 'R5', containerId: c, deviceType: 'Mouse', interfaceNumber: iface });
const grouped = groupInterfaces([mk(0, '{A}'), mk(1, '{A}'), mk(0, '{B}'), mk(0, ''), mk(0, '')]);
assert.equal(grouped.length, 4, 'same container merges; missing container never merges');
assert.equal(grouped[0].interfaces.length, 2);
assert.equal(groupInterfaces(null).length, 0);
console.log('Frontend diagnostics aggregation tests passed.');
console.log('Frontend interface grouping tests passed.');
