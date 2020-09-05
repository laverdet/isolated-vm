// node-args: --expose-gc

//
// Checks if it is possible to create es6 modules.
const ivm = require('isolated-vm');
const { strictEqual, throws } = require('assert');
const assert = require('assert');

function doesNotReject(fn) {
	return fn().catch(console.error);
}
function rejects(fn) {
	return fn().then(() => { throw new Error('Promise did not reject') }).catch(() => 0);
}

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
		module.instantiateSync(context, function(){});
		throws(() => module.instantiateSync());
		strictEqual(typeof module.evaluateSync, 'function');
		const evaluateResult = module.evaluateSync();
		// strictEqual('This is awesome!', evaluateResult);
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
		throws(() => module.namespace, /not been instantiated/);
	}

	function setupModuleInstantiateErrorAndRunChecks() {
		const data = (moduleMap.instantiateError = {});
		const code = `
			import something from './something';
			export default "hello world";
		`;
		const module = data.module = isolate.compileModuleSync(code);
		throws(() => module.instantiateSync(context, function(){}), /dependency was not/);
	}

	function setupModuleTimeoutAndRunChecks() {
		const data = (moduleMap.timeout = {});
		const code = `let i = 0; while(++i); i;`; // should generate a timeout
		const module = data.module = isolate.compileModuleSync(code);
		module.instantiateSync(context, function(){});
		throws(() => module.evaluateSync({ timeout: 50 }), /execution timed out/);
	}

	function setupModuleEvaluateErrorAndRunChecks() {
		const data = (moduleMap.evaluate = {});
		const code = `throw new Error('Some error');`;
		const module = data.module = isolate.compileModuleSync(code);
		module.instantiateSync(context, function(){});
		throws(() => module.evaluateSync(context), /Some error/);
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

		module.instantiateSync(context, function(spec) {
			if (spec == './add') {
				return moduleMap.add.module;
			}
		});
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
		module.instantiateSync(context, function(){});
		module.evaluateSync();
		// lets try to use add through our "math" library
		const reference = module.namespace;
		const value = reference.getSync('value');
		const countUp = reference.getSync('countUp');
		strictEqual(reference.getSync('value'), 0);
		countUp.applySync(null, [ ]);
		strictEqual(reference.getSync('value'), 1);
	}

	function moduleCollectionChecks() {
		isolate.compileModuleSync('');
		global.gc();
	}

	function linkChecks() {
		doesNotReject(async function() {
			let context = isolate.createContextSync();
			let modules = {
				'a': isolate.compileModuleSync('import * as foo from "b";'),
				'b': isolate.compileModuleSync('import * as foo from "a"; import * as bar from "c";'),
				'c': isolate.compileModuleSync('1'),
			};

			// Test sync module resets
			throws(function() {
				modules.a.instantiateSync(context, function(spec) {
					if (spec == 'c') throw new Error('hello');
					return modules[spec];
				});
			}, /hello/);

			// Test async module resets
			await rejects(async function() {
				await modules.a.instantiate(context, function(spec) {
					if (spec == 'c') throw new Error('hello');
					return Promise.resolve(modules[spec]);
				});
			}, /hello/);

			// Instantiate module successfully
			await modules.a.instantiate(context, function(spec) {
				return Promise.resolve(modules[spec]);
			});

			// Make sure links are remembered
			modules.a.instantiateSync(context, () => { throw new Error });
			await modules.a.instantiate(context, () => { throw new Error });
		});
	}

	function cachedDataChecks() {
		// Generate cached data
		let context = isolate.createContextSync();
		let code = 'export function foo() { return 123; }';
		let module = isolate.compileModuleSync(code, { produceCachedData: true });
		let cachedData = module.cachedData;
		console.log(cachedData.copy());
		assert.ok(cachedData, 'did not produce cached data');
		// Test cached data
		let module2 = isolate.compileModuleSync(code, { cachedData: cachedData });
		assert.strictEqual(module2.cachedDataRejected, false, 'cache data was rejected');
		module2.instantiateSync(context, function(){});
		module2.evaluateSync();
		assert.strictEqual(module2.namespace.getSync('foo').applySync(undefined, []), 123, 'invalid result from cached module');
		// Test invalid cached data
		assert.ok(isolate.compileModuleSync('void 0', { cachedData: cachedData }).cachedDataRejected, 'cache data wasn\'t rejected');
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
		moduleCollectionChecks();
		linkChecks();
		// cachedDataChecks(); TODO: enable after v8 6.9.37
		console.log('pass');
	} catch(err) {
		console.error(err);
	}

})();
