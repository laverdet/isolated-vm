const ivm = require('isolated-vm');

async function run() {
	for (let i = 0; i < 1000; i++) {
		const isolate = new ivm.Isolate;
		const context = isolate.createContextSync();
		const jail = context.global;

		void jail;

		await new Promise(resolve => setTimeout(resolve, 1));
	}

	const memoryUsage = process.memoryUsage();
	const rssMemory = Math.round(memoryUsage.rss / 1024 / 1024);
	if (rssMemory < 200) {
		console.log('pass');
	} else {
		console.log('failed with RSS memory:', rssMemory);
	}
}

run().catch(console.error);