/**
 * Checks if object returned by the isolate.compileModuleSync() returns an object that statisfy nodejs vm.Module class
 **/
const ivm = require('isolated-vm');
const { strictEqual, throws } = require('assert');

(function () {
  const isolate = new ivm.Isolate();
  const context = isolate.createContextSync(); // All modules must share the same context

  const moduleMap = {};

  function setupModuleAddAndRunChecks() {
    const data = (moduleMap.add = {});
    const code = `export default function add(a, b) { return a + b; };"This is awesome!";`;
    const module = data.module = isolate.compileModuleSync(code);
    strictEqual(typeof module.getModuleRequestsLength, 'function');
    const moduleRequestsLength = module.getModuleRequestsLength();
    strictEqual(moduleRequestsLength, 0);
    strictEqual(typeof module.instantiateSync, 'function');
    const instantiateResult = module.instantiateSync(context);
    strictEqual(instantiateResult, true);
    strictEqual(typeof module.evaluateSync, 'function');
    const evaluateResult = module.evaluateSync(context);
    strictEqual('This is awesome!', evaluateResult);

    const reference = module.getModuleNamespaceSync(context);
    strictEqual(reference.typeof, 'object');
    const defaultExport = reference.getSync('default');
    strictEqual(typeof defaultExport, 'object');
    strictEqual(defaultExport.typeof, 'function');
    const result = defaultExport.applySync(null, [ 2, 4 ]);
    strictEqual(result, 6);
  }

  function setupModuleTimeoutAndRunChecks() {
    const data = (moduleMap.timeout = {});
    const code = `let i = 0; while(++i); i;`; // should generate a timeout
    const module = data.module = isolate.compileModuleSync(code);
    const instantiateResult = module.instantiateSync(context);
    strictEqual(instantiateResult, true);
    throws(() => module.evaluateSync(context, { timeout: 50 }));
  }

  try {
    setupModuleAddAndRunChecks();
    setupModuleTimeoutAndRunChecks();
    console.log('pass');
  } catch(err) {
    console.log(err);
  }

})();