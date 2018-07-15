/**
 * Checks if object returned by the isolate.compileModuleSync() returns an object that statisfy nodejs vm.Module class
 **/
const ivm = require('isolated-vm');
const { strictEqual } = require('assert');

(function () {
  const isolate = new ivm.Isolate();
  const context = isolate.createContextSync(); // All modules must share the same context

  const moduleMap = {};

  function setupModuleAddAndRunChecks() {
    const data = (moduleMap.add = {});
    const code = `export default function add(a, b) { return a + b; }`;
    const module = data.module = isolate.compileModuleSync(code);
    strictEqual(typeof module.getModuleRequestsLength, 'function');
    const moduleRequestsLength = module.getModuleRequestsLength();
    strictEqual(moduleRequestsLength, 0);
    strictEqual(typeof module.instantiateSync, 'function');
    const success = module.instantiateSync(context);
    strictEqual(success, true);
  }

  try {
    setupModuleAddAndRunChecks();
    console.log('pass');
  } catch(err) {
    console.log(err);
  }

})();