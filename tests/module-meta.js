const ivm = require('isolated-vm');
const assert = require('assert');

(async function() {
	const isolate = new ivm.Isolate();
	const context = isolate.createContextSync();

	assert.throws(() =>
		isolate.compileModuleSync('export default import.meta.url', { meta: meta => { meta.url = 'fail' } }));

	console.log(context.evalClosureSync(`
		const module = $0.compileModuleSync('export default import.meta.url', { meta: meta => { meta.url = 'pass' } });
		module.instantiateSync($1, () => {});
		module.evaluateSync();
		return module.namespace.deref().default;
	`, [ isolate, context ]));
})().catch(console.error);
