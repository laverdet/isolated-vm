const ivm = require('isolated-vm');
const assert = require('assert');

const isolate = new ivm.Isolate;
const context = isolate.createContextSync();
const global = context.global;

const value = { foo: 'bar' };

global.setSync('test', value, { reference: true });
assert.equal(global.getSync('test').deref(), value);

global.setSync('test', value, { copy: true });
assert.deepEqual(global.getSync('test', { copy: true }), value);
assert.deepEqual(global.getSync('test', { externalCopy: true }).copy(), value);

assert.deepEqual(isolate.compileScriptSync('test').runSync(context, { copy: true }), value);
const fn = isolate.compileScriptSync('(function foo(arg1) { return arg1; })').runSync(context, { reference: true });
assert.deepEqual(fn.applySync(undefined, [ value ], { arguments: { copy: true }, return: { copy: true } }), value);

console.log('pass');
