/**
 * Checks if object returned by the isolate.compileModuleSync() returns an object that statisfy nodejs vm.Module class
 **/
const ivm = require('isolated-vm');
const { strictEqual } = require('assert');

(function () {
  const isolate = new ivm.Isolate();
  const code = `
    import first from './first';
    import second from './second';
    export default function add(a, b) { return a + b; }
  `;
  try {
    const module = isolate.compileModuleSync(code);
    strictEqual(typeof module.getModuleRequestsLength, 'function');
    const moduleRequestsLength = module.getModuleRequestsLength();
    strictEqual(moduleRequestsLength, 2);
    const moduleRequests = Array.from({ length: moduleRequestsLength }).map((val, index) => module.getModuleRequestSync(index));
    strictEqual(JSON.stringify(moduleRequests), JSON.stringify(['./first', './second']))
    strictEqual(typeof module.instantiate, 'function');
    strictEqual(typeof module.evaluate, 'function');
    console.log('pass');
  } catch(err) {
    console.log(err);
  }

})();