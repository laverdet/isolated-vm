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
		throws(() => module.evaluateSync());
		module.instantiateSync(context);
		throws(() => module.instantiateSync());
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
	function setupModuleNamespaceAndRunChecks() {
		const data = (moduleMap.namespace = {});
		const code = `export default function add(a, b) { return a + b; };`;
		const module = data.module = isolate.compileModuleSync(code);
		throws(() => module.namespace);
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

		const dependencySpecifiers = module.dependencySpecifiers;
		strictEqual(JSON.stringify(dependencySpecifiers), JSON.stringify(['./add']));

		strictEqual(typeof module.setDependency, 'function');
		module.setDependency('./add', moduleMap.add.module);
		module.instantiateSync(context);
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

	function setupModuleBugRunChecks() {
		const data = (moduleMap.bug = {});
		const code = `
			export let value = 0;

			export function countUp() {
				return ++value;
			};
		`;
		const module = data.module = isolate.compileModuleSync(code);
		module.instantiateSync(context);
		module.evaluateSync();
		// lets try to use add through our "math" library
		const reference = module.namespace;
		const value = reference.getSync('value');
		const countUp = reference.getSync('countUp');
		strictEqual(reference.getSync('value').copySync(), 0);
		countUp.applySync(null, [ ]);
		strictEqual(reference.getSync('value').copySync(), 1);
	}
	if (/^v8\.[0-6]/.test(process.version)) {
		console.log('pass');
		return;
	}
	try {
		setupModuleAddAndRunChecks();
		setupModuleNamespaceAndRunChecks();
		setupModuleInstantiateErrorAndRunChecks();
		setupModuleTimeoutAndRunChecks();
		setupModuleEvaluateErrorAndRunChecks();
		setupModuleMathAndRunChecks();
		setupModuleBugRunChecks();
		console.log('pass');
	} catch(err) {
		console.error(err);
	}

})();
