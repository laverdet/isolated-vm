const ivm = require('isolated-vm');
const assert = require('assert');

function rejects(fn, re) {
	return fn().then(
		() => { throw new Error('Promise did not reject'); },
		err => { assert(re.test(err.message), `unexpected rejection: ${err.message}`); });
}

(async function() {
	const isolate = new ivm.Isolate();
	const context = await isolate.createContext();
	// Root has two direct imports; the callback resolves the first, then throws synchronously on the
	// second. The first import leaves a pending linker continuation outstanding when the throw escapes.
	const root = await isolate.compileModule(`import 'a'; import 'b';`);
	const a = await isolate.compileModule(`export default 1;`);
	const b = await isolate.compileModule(`export default 2;`);

	await rejects(() => root.instantiate(context, specifier => {
		if (specifier === 'a') return a;
		throw new Error('boom');
	}), /boom/);

	// Reset() should have left link state clean, so the module can be instantiated again successfully.
	await root.instantiate(context, specifier => specifier === 'a' ? a : b);

	console.log('pass');
})().catch(err => { console.error(err); });
