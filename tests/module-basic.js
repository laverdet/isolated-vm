/**
 * Checks if it is possible to create es6 modules.
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
    const dependencySpecifiers = module.dependencySpecifiers;
    strictEqual(Array.isArray(dependencySpecifiers), true);
    strictEqual(dependencySpecifiers.length, 0);
    strictEqual(typeof module.instantiateSync, 'function');
    module.instantiateSync(context);
    strictEqual(typeof module.evaluateSync, 'function');
    const evaluateResult = module.evaluateSync();
    strictEqual('This is awesome!', evaluateResult);
    const reference = module.namespace;
    strictEqual(reference.typeof, 'object');
    const defaultExport = reference.getSync('default');
    strictEqual(typeof defaultExport, 'object');
    strictEqual(defaultExport.typeof, 'function');
    const result = defaultExport.applySync(null, [ 2, 4 ]);
    strictEqual(result, 6);
  }

  function setupModuleInstantiateErrorAndRunChecks() {
    const data = (moduleMap.instantiateError = {});
    const code = `
      import something from './something';
      export default "hello world";
    `;
    const module = data.module = isolate.compileModuleSync(code);
    throws(() => module.instantiateSync(context));
  }

  function setupModuleTimeoutAndRunChecks() {
    const data = (moduleMap.timeout = {});
    const code = `let i = 0; while(++i); i;`; // should generate a timeout
    const module = data.module = isolate.compileModuleSync(code);
    module.instantiateSync(context);
    throws(() => module.evaluateSync({ timeout: 50 }));
  }

  function setupModuleEvaluateErrorAndRunChecks() {
    const data = (moduleMap.evaluate = {});
    const code = `throw new Error('Some error');`;
    const module = data.module = isolate.compileModuleSync(code);
    module.instantiateSync(context);
    throws(() => module.evaluateSync(context));
  }

  function setupModuleMathAndRunChecks() {
    const data = (moduleMap.math = {});
    const code = `
      import add from './add';
      export { add };
      
      export function sub(a, b) {
        return add(a, -b);
      };
    `;
    const module = data.module = isolate.compileModuleSync(code);
    const dependencySpecifiers = module.dependencySpecifiers

    strictEqual(JSON.stringify(dependencySpecifiers), JSON.stringify(['./add']));
    strictEqual(typeof module.setDependency, 'function');
    strictEqual(module.setDependency('./add', moduleMap.add.module), true);
    const instantiateResult = module.instantiateSync(context);
    strictEqual(instantiateResult, true);
    module.evaluateSync();
    // lets try to use add through our "math" library
    const reference = module.namespace;
    strictEqual(reference.typeof, 'object');
    const add = reference.getSync('add');
    strictEqual(typeof add, 'object');
    strictEqual(add.typeof, 'function');
    strictEqual(add.applySync(null, [ 2, 4 ]), 6);

    const sub = reference.getSync('sub');
    strictEqual(typeof sub, 'object');
    strictEqual(sub.typeof, 'function');
    strictEqual(sub.applySync(null, [ 2, 4 ]), -2);
  }

  try {
    setupModuleAddAndRunChecks();
    setupModuleInstantiateErrorAndRunChecks();
    setupModuleTimeoutAndRunChecks();
    setupModuleEvaluateErrorAndRunChecks();
    setupModuleMathAndRunChecks();
    console.log('pass');
  } catch(err) {
    console.log(err);
  }

})();