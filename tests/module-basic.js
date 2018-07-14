/**
 * Checks if object returned by the isolate.compileModuleSync() returns an object that statisfy nodejs vm.Module class
 **/
const ivm = require('isolated-vm');
const { strictEqual } = require('assert');

(function () {
  const isolate = new ivm.Isolate();
  const code = `export default function add(a, b) { return a + b; }`;
  try {
    const module = isolate.compileModuleSync(code);
    strictEqual(typeof module.link, 'function');
    strictEqual(typeof module.instantiate, 'function');
    strictEqual(typeof module.evaluate, 'function');
    console.log('pass');
  } catch(err) {
    console.log(err);
  }

})();