const ivm = require('isolated-vm');
const { strictEqual } = require('assert');

(async function() {
	const isolate = new ivm.Isolate();
	const context = await isolate.createContext();
	const specifier = './café-中-😀.js';
	const root = await isolate.compileModule(`import value from '${specifier}'; globalThis.value = value;`);
	const dependency = await isolate.compileModule('export default 42;');

	strictEqual(root.dependencySpecifiers[0], specifier);
	await root.instantiate(context, received => {
		strictEqual(received, specifier);
		return dependency;
	});
	await root.evaluate();
	strictEqual(await context.global.get('value'), 42);

	console.log('pass');
})().catch(err => { console.error(err); });
