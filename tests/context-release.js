const ivm = require('isolated-vm');

(async () => {
	let result = '';
	const isolate = new ivm.Isolate;
	const context = await isolate.createContext();
	await context.evalClosure(`
		save = value => $0.applySync(undefined, [ value ]);
		run = () => $1.applySync(undefined, [], { result: { promise: true } });
		`, [
			val => result = val,
			() => new Promise(resolve => setTimeout(resolve, 100)),
		],
		{ arguments: { reference: true } },
	);

	const job = context.evalSync(`
		(async () => {
			save('fail');
			await run();
			save('pass');
		})();
		`, { promise: true },
	);
	context.release();
	await job.result;
	console.log(result);
})().catch(console.error);
