const ivm = require('isolated-vm');

// Ideally the RSS should be below 200 MB after running this test, sometimes it goes over 200 MB.
// Anything over 512 MB is considered a failure, as it indicates a memory leak and the garbage collection propagation
// is likely not working correctly.
const RSS_MEMORY_LIMIT_MB = 512; // in MB

async function run() {
	for (let i = 0; i < 1000; i++) {
		const isolate = new ivm.Isolate;
		const context = isolate.createContextSync();
		const jail = context.global;
		jail.setSync('isolate', isolate);

		isolate.compileScriptSync(`
			isolate.compileScriptSync('1');
			delete globalThis.isolate;
		`).runSync(context);

		void jail;

		await new Promise(resolve => setTimeout(resolve, 1));
	}

	const memoryUsage = process.memoryUsage();
	const rssMemory = Math.round(memoryUsage.rss / 1024 / 1024);
	if (rssMemory < RSS_MEMORY_LIMIT_MB) {
		console.log('pass');
	} else {
		console.log('failed with RSS memory:', rssMemory);
	}
}

run().catch(console.error);