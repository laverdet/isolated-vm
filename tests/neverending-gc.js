const ivm = require('isolated-vm');
function getWall(isolate) {
	const { wallTime } = isolate;
	return (wallTime[0] + wallTime[1] / 1e9) * 1000;
}

(async function() {
	const isolate = new ivm.Isolate;
	const context = await isolate.createContext();
	await context.eval('let heap = [];');
	const run = (await context.eval(`function run() {
		let str = "";
		for (let ii = 0; ii < 1000; ++ii) {
			str += Math.random();
		}
		return str;
	}; run`, { reference: true })).result;
	const wallTime = getWall(isolate);
	for (let ii = 0; ii < 1000; ++ii) {
		await run.apply();
	}
	const time1 = getWall(isolate) - wallTime;
	while (true) {
		await context.eval('heap.push(run())');
		const heap = await isolate.getHeapStatistics();
		if (heap.heap_size_limit < heap.used_heap_size * 1.2) {
			break;
		}
	}
	const wallTime2 = getWall(isolate);
	for (let ii = 0; ii < 1000; ++ii) {
		await run.apply();
	}
	const time2 = getWall(isolate) - wallTime2;
	if (time1 * 4 > time2) {
		console.log('pass');
	} else {
		console.log('fail');
	}
	const timer = setTimeout(() => {
		console.log('hung');
		process.exit(1);
	}, 1000);
	timer.unref();
})().catch(console.error);

