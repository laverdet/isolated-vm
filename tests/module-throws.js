const ivm = require('isolated-vm');
function rejects(fn) {
  return fn().then(() => { throw new Error('Promise did not reject') }).catch(() => 0);
}

(async function() {
	const isolate = new ivm.Isolate();
	const context = await isolate.createContext();
	const module = await isolate.compileModule(`import foo from 'foo'`);
	await rejects(() => module.instantiate(context, () => Promise.reject(new Error)));
	await rejects(() => module.evaluate());
	console.log('pass');
})().catch(console.error);
