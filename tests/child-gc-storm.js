// node-args: --expose-gc
const ivm = require('isolated-vm');

// Regression test for the "child GC storm".
//
// The parent (Node) isolate installs a GC prologue that, on every major GC, propagates memory
// pressure to each owned child isolate. That pressure notification triggers a full
// "collect-all-available-garbage" GC on the child — the single most expensive GC there is. It must
// therefore be delivered to a given child AT MOST ONCE, not on every parent GC.
//
// The bug (commit 9fb10cd): the prologue used the child-owned `last_memory_pressure` field as its
// dedup flag. A busy child resets that field to `kNone` in its own routine GC epilogues, re-arming
// the guard, so EVERY subsequent parent GC forced another full collect-all GC on the child. With the
// parent GCing frequently, a busy child gets slammed with full GCs over and over — pinning the CPU.
//
// We detect this directly and deterministically via `getHeapStatistics().major_gc_count`, which
// counts the mark-sweep-compact GCs a child has run. We drive a fixed, old-gen-churning workload on
// several busy children twice: once quietly, once while hammering the PARENT with GCs. Without the
// storm the parent's GCs have no effect on the children's major-GC count. With the storm the count
// multiplies several-fold. The comparison is self-calibrating (no machine-dependent absolute count).

const CHILDREN = 4;
const OUTER = 6; // times each child runs the workload closure

// The workload maintains a rolling window of medium-lived objects: they get promoted to old-gen and
// then dropped, which forces the child to run its OWN major GCs periodically. Those are exactly the
// collections the storm bug upgrades to full collect-all and multiplies.
const SETUP = `
	globalThis.buf = [];
	globalThis.work = function () {
		for (let i = 0; i < 800000; i++) {
			buf.push({ i, s: 'y' + i, n: { a: i, b: i } });
			if (buf.length > 150000) buf.splice(0, 75000);
		}
		return buf.length;
	};
`;

// Fixed build: parent-GC pressure leaves the child major-GC count essentially unchanged (ratio ~1).
// Buggy build: it multiplies it ~6x. 2.5 sits far from both — a real storm blows straight through it
// while normal GC jitter never approaches it.
const MAX_GC_RATIO = 2.5;

/**
 * Run the workload on `CHILDREN` busy child isolates and return the total number of major GCs they
 * performed while running.
 * @param {boolean} parentGcPressure hammer the parent isolate with major GCs while children work
 * @returns {Promise<number>} total child major GCs during the workload
 */
async function countChildMajorGcs(parentGcPressure) {
	const children = [];
	for (let i = 0; i < CHILDREN; i++) {
		const isolate = new ivm.Isolate({ memoryLimit: 256 });
		const context = isolate.createContextSync();
		context.evalSync(SETUP);
		children.push({ isolate, context });
	}

	global.gc();
	const before = children.map(({ isolate }) => isolate.getHeapStatisticsSync().major_gc_count);

	let timer = null;
	if (parentGcPressure) {
		timer = setInterval(() => global.gc(), 5);
	}

	await Promise.all(children.map(({ context }) =>
		context.eval(`for (let n = 0; n < ${OUTER}; n++) work(); "ok"`)));

	if (timer) clearInterval(timer);

	let total = 0;
	children.forEach(({ isolate }, i) => {
		total += isolate.getHeapStatisticsSync().major_gc_count - before[i];
	});
	for (const { isolate } of children) isolate.dispose();
	return total;
}

async function run() {
	if (typeof global.gc !== 'function') {
		console.error('failed: run with --expose-gc');
		process.exitCode = 1;
		return;
	}

	// Warm up so JIT/heap growth doesn't skew the first real measurement.
	await countChildMajorGcs(false);

	const baseline = await countChildMajorGcs(false);
	const underPressure = await countChildMajorGcs(true);

	// Guard against a degenerate baseline (workload did no major GCs at all) so the ratio is meaningful.
	if (baseline < 10) {
		console.error(`failed: baseline produced only ${baseline} major GCs — workload is not exercising major GC`);
		process.exitCode = 1;
		return;
	}

	const ratio = underPressure / baseline;
	if (ratio <= MAX_GC_RATIO) {
		console.log('pass');
	} else {
		console.error(`child major GCs without parent pressure: ${baseline}`);
		console.error(`child major GCs under parent GC pressure: ${underPressure}`);
		console.error(`ratio ${ratio.toFixed(2)} exceeds limit ${MAX_GC_RATIO} — parent GC is force-collecting busy children repeatedly (GC storm)`);
		process.exitCode = 1;
	}
}

run().catch(error => {
	console.error(error);
	process.exitCode = 1;
});
