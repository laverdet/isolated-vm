const ivm = require('isolated-vm');
const assert = require('assert');

(async function() {
	// Without a handler `import()` rejects with the error v8 raises on its own
	const bare = new ivm.Isolate;
	const bareContext = await bare.createContext();
	await assert.rejects(bareContext.eval(`import('./add.js')`, { promise: true }), /Not supported/);
	bare.dispose();

	const seen = [];
	const modules = new Map;
	const isolate = new ivm.Isolate({
		importModuleDynamically(specifier, referrer) {
			seen.push([ specifier, referrer ]);
			if (specifier === './boom.js') {
				throw new Error('boom');
			}
			return modules.get(specifier);
		},
	});
	const context = await isolate.createContext();
	const add = await isolate.compileModule(
		`export const name = 'add'; export default (left, right) => left + right;`,
		{ filename: 'file:///add.js' });
	await add.instantiate(context, () => {});
	await add.evaluate();
	modules.set('./add.js', add);

	// The handler answers with a module and the isolate receives that module's namespace
	assert.strictEqual(await context.eval(`import('./add.js').then(ns => ns.name)`, { promise: true }), 'add');
	assert.deepStrictEqual(seen.at(-1), [ './add.js', '<isolated-vm>' ]);

	// A module gets its own filename as the referrer, and the bindings it receives are usable
	const main = await isolate.compileModule(
		`export const load = () => import('./add.js');`,
		{ filename: 'file:///main.js' });
	await main.instantiate(context, () => {});
	await main.evaluate();
	const load = await main.namespace.get('load', { reference: true });
	const namespace = await load.apply(undefined, [], { result: { promise: true, reference: true } });
	assert.deepStrictEqual(seen.at(-1), [ './add.js', 'file:///main.js' ]);
	// The exports of a namespace are accessors, so reading one from here needs `accessors`
	const exported = await namespace.get('default', { accessors: true, reference: true });
	assert.strictEqual(await exported.apply(undefined, [ 2, 4 ]), 6);

	// The handler may answer with a promise, like the callback which resolves static dependencies
	modules.set('./add-later.js', Promise.resolve(add));
	assert.strictEqual(await context.eval(`import('./add-later.js').then(ns => ns.name)`, { promise: true }), 'add');

	// A specifier reaches the handler exactly as the isolate wrote it
	modules.set('./café-中.js', add);
	assert.strictEqual(await context.eval(`import('./café-中.js').then(ns => ns.name)`, { promise: true }), 'add');
	assert.deepStrictEqual(seen.at(-1), [ './café-中.js', '<isolated-vm>' ]);

	// The isolate sees what the handler throws
	await assert.rejects(context.eval(`import('./boom.js')`, { promise: true }), /boom/);

	// A specifier the handler has no module for rejects in the isolate
	await assert.rejects(context.eval(`import('./nope.js')`, { promise: true }), /Resolved import was not `Module`/);

	// A module which has not been evaluated has no bindings to hand over, and one which has not been
	// instantiated has no namespace at all
	const unevaluated = await isolate.compileModule(`export const name = 'unevaluated';`);
	await unevaluated.instantiate(context, () => {});
	modules.set('./unevaluated.js', unevaluated);
	await assert.rejects(context.eval(`import('./unevaluated.js')`, { promise: true }), /Module has not been evaluated/);
	modules.set('./uninstantiated.js', await isolate.compileModule(`export const name = 'uninstantiated';`));
	await assert.rejects(context.eval(`import('./uninstantiated.js')`, { promise: true }), /Module has not been evaluated/);

	// A module which failed to evaluate rejects with the error it threw
	const failed = await isolate.compileModule(`throw new Error('module failed');`);
	await failed.instantiate(context, () => {});
	await assert.rejects(failed.evaluate(), /module failed/);
	modules.set('./failed.js', failed);
	await assert.rejects(context.eval(`import('./failed.js')`, { promise: true }), /module failed/);

	// A namespace belongs to the isolate its module was compiled in and can go nowhere else
	const other = new ivm.Isolate;
	const otherContext = await other.createContext();
	const otherModule = await other.compileModule(`export const name = 'other';`);
	await otherModule.instantiate(otherContext, () => {});
	await otherModule.evaluate();
	modules.set('./other.js', otherModule);
	await assert.rejects(context.eval(`import('./other.js')`, { promise: true }), /Module is from a different isolate/);
	other.dispose();

	isolate.dispose();

	// A handler may belong to another isolate rather than to nodejs, and it is called there
	const outer = new ivm.Isolate;
	const outerContext = await outer.createContext();
	outerContext.global.setSync('ivm', ivm);
	assert.strictEqual(await outerContext.eval(`(async () => {
		const modules = new Map;
		const inner = new ivm.Isolate({ importModuleDynamically: specifier => modules.get(specifier) });
		const innerContext = await inner.createContext();
		const nested = await inner.compileModule('export const name = "nested";');
		await nested.instantiate(innerContext, () => {});
		await nested.evaluate();
		modules.set('./nested.js', nested);
		return innerContext.eval('import("./nested.js").then(ns => ns.name)', { promise: true });
	})()`, { promise: true }), 'nested');
	outer.dispose();

	// A handler whose own isolate dies while it runs reports why
	const starving = new ivm.Isolate({ memoryLimit: 16 });
	const starvingContext = await starving.createContext();
	starvingContext.global.setSync('ivm', ivm);
	const starved = await starvingContext.eval(`
		const waste = [];
		new ivm.Isolate({ importModuleDynamically: () => { for (;;) waste.push(new Array(1e5).fill(0)) } });
	`);
	const starvedContext = await starved.createContext();
	await assert.rejects(starvedContext.eval(`import('./starved.js')`, { promise: true }), /memory limit/);
	starved.dispose();

	console.log('pass');
})().catch(console.error);
