'use strict';
const ivm = require('isolated-vm');
(async function() {
		const isolate = new ivm.Isolate;
		const context = await isolate.createContext();

		await context.evalClosure(
			'run = () => $0.applySyncPromise(null, [])',
			[
				async () => {
					isolate.dispose();
					await new Promise(resolve => setTimeout(resolve, 0));
				}
			],
			{ arguments: { reference: true } },
		);

		const run = await context.global.get('run', { reference: true });
		try {
			await run.apply(null, []);
		} catch (err) {
			if (err.message !== 'Isolate was disposed during execution') {
				throw err;
			}
		}
		console.log('pass');
}()).catch(console.log);