const ivm = require('isolated-vm');
const isolate = new ivm.Isolate;
const context = isolate.createContextSync();
const timeout = setTimeout(() => {
	console.log('fail');
	process.exit(1);
}, 1000);
(async function() {
	try {
		await context.eval('/1+2/.test("1".repeat(1024 ** 2 * 127));', { timeout: 100 });
		console.log('did not fail');
	} catch (err) {}
	clearTimeout(timeout);
	console.log('pass');
})().catch(console.error);

