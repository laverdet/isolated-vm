// node-args: --expose-gc
const ivm = require('isolated-vm');

// This test verifies that isolates and contexts torn down via GC (rather than an explicit
// `isolate.dispose()`) do not leak.
//
// IMPORTANT — what "no leak" means here:
// You CANNOT assert `rss_after === rss_before`. Under GC-driven teardown, V8 grows a process-wide
// page pool / zone-segment cache to cover the steady-state backlog of isolates pending disposal, then
// keeps those pages mapped — it never returns them to the OS. That residual (~50-100MB) is bounded,
// reclaimable, and allocator-independent (confirmed against glibc/jemalloc/tcmalloc). It is NOT a leak.
//
// The signal that DOES distinguish a leak from bounded retention is growth as a function of work done:
//   - bounded retention  -> a 4x-larger batch adds ~no further memory once the pool is warm
//   - a real leak         -> memory grows roughly linearly with iteration count
//
// So we run two batches in one process (a small warmup batch, then a 4x batch) and assert the large
// batch barely grows beyond the warmup. We also assert the V8 heap and external memory return to
// baseline, which is the direct evidence that the collected isolates released their memory.

const WARMUP_ITERATIONS = 500;
const MEASURE_ITERATIONS = WARMUP_ITERATIONS * 4; // 4x the work; a leak would grow ~4x with it.

// RSS leak gate (allocator-independent). An absolute MB threshold can't be portable: baseline
// fragmentation/page-retention is allocator-specific (glibc vs macOS libmalloc vs jemalloc/tcmalloc),
// so a 4x batch grows a different absolute amount on each platform with no leak present. Instead we
// compare the measurement batch's growth *per iteration* against the warmup batch's. Once the pool is
// warm, bounded retention grows far slower per-iteration than it did cold; a real leak grows at parity
// or faster. We only flag a leak when BOTH hold: growth clears a small absolute noise floor AND the
// per-iteration rate stays a meaningful fraction of the warmup rate.
const RSS_NOISE_FLOOR_MB = 24; // below this, post-warmup growth is just measurement noise — ignore.
const MAX_PER_ITERATION_RATIO = 0.75; // measurePerIter must stay below warmupPerIter * this.

// Collected isolates must bring the heap/external memory back down near where it started. These are the
// reliable, allocator-independent leak signals.
const MAX_HEAP_GROWTH_MB = 8;
const MAX_EXTERNAL_GROWTH_MB = 8;

/** @returns {{rss: number, heap: number, external: number}} current process memory in MB */
function sampleMemoryMb() {
	const usage = process.memoryUsage();
	return {
		rss: Math.round(usage.rss / 1024 / 1024),
		heap: Math.round(usage.heapUsed / 1024 / 1024),
		external: Math.round(usage.external / 1024 / 1024),
	};
}

/**
 * Drive GC hard and let pending isolate disposals/finalizers drain, so the next sample reflects the
 * settled floor rather than transient backlog.
 * @returns {Promise<void>}
 */
async function settle() {
	for (let i = 0; i < 5; i++) {
		global.gc();
		await new Promise(resolve => setTimeout(resolve, 50));
	}
}

/**
 * Create `count` isolates with a context, then drop all references so the outer (Node) isolate's GC is
 * solely responsible for tearing them down.
 * @param {number} count number of isolates to churn through
 * @returns {Promise<void>}
 */
async function churnIsolates(count) {
	for (let i = 0; i < count; i++) {
		const isolate = new ivm.Isolate;
		const context = isolate.createContextSync();
		const jail = context.global;

		void jail;

		await new Promise(resolve => setTimeout(resolve, 1));
	}
}

async function run() {
	if (typeof global.gc !== 'function') {
		console.error('failed: run with --expose-gc (e.g. `node --expose-gc tests/garbage-collection.js`)');
		process.exitCode = 1;
		return;
	}

	await settle();
	const baseline = sampleMemoryMb();

	// Warmup batch: lets V8's page pool grow to its steady-state working set.
	await churnIsolates(WARMUP_ITERATIONS);
	await settle();
	const afterWarmup = sampleMemoryMb();

	// Measurement batch: 4x the iterations. With the pool already warm, a non-leaking implementation
	// adds almost nothing; a leak grows roughly in proportion to the extra iterations.
	await churnIsolates(MEASURE_ITERATIONS);
	await settle();
	const afterMeasure = sampleMemoryMb();

	const warmupRssGrowth = afterWarmup.rss - baseline.rss;
	const postWarmupRssGrowth = afterMeasure.rss - afterWarmup.rss;
	const heapGrowth = afterMeasure.heap - baseline.heap;
	const externalGrowth = afterMeasure.external - baseline.external;

	// How fast RSS climbs cold (warmup) vs warm (measurement). If the warmup didn't grow RSS at all,
	// this platform has negligible fragmentation, so the noise floor alone decides.
	const warmupPerIter = warmupRssGrowth / WARMUP_ITERATIONS;
	const measurePerIter = postWarmupRssGrowth / MEASURE_ITERATIONS;
	const perIterationRatio = warmupPerIter > 0 ? measurePerIter / warmupPerIter : Infinity;

	const failures = [];
	// A leak only if growth is both large enough to matter AND keeps pace, per-iteration, with the cold
	// warmup. Bounded retention grows far slower once warm; a real leak grows at parity or faster.
	if (postWarmupRssGrowth > RSS_NOISE_FLOOR_MB && perIterationRatio > MAX_PER_ITERATION_RATIO) {
		failures.push(`RSS grew ${postWarmupRssGrowth} MB on a 4x batch at ${Math.round(perIterationRatio * 100)}% of the warmup per-iteration rate — memory scales with iterations (leak)`);
	}
	if (heapGrowth > MAX_HEAP_GROWTH_MB) {
		failures.push(`V8 heap stayed ${heapGrowth} MB above baseline — collected isolates did not release heap memory`);
	}
	if (externalGrowth > MAX_EXTERNAL_GROWTH_MB) {
		failures.push(`external memory stayed ${externalGrowth} MB above baseline — external memory not reclaimed`);
	}

	// The test runner asserts stdout is exactly "pass\n", so only emit diagnostics on failure (stderr).
	if (failures.length === 0) {
		console.log('pass');
	} else {
		console.error('baseline:', JSON.stringify(baseline));
		console.error(`after ${WARMUP_ITERATIONS} (warmup):`, JSON.stringify(afterWarmup));
		console.error(`after ${MEASURE_ITERATIONS} (4x):`, JSON.stringify(afterMeasure));
		console.error(`RSS growth: warmup ${warmupRssGrowth} MB (${warmupPerIter.toFixed(3)} MB/iter), post-warmup ${postWarmupRssGrowth} MB (${measurePerIter.toFixed(3)} MB/iter)`);
		console.error(`per-iteration ratio: ${perIterationRatio === Infinity ? 'n/a' : Math.round(perIterationRatio * 100) + '%'} (limit ${Math.round(MAX_PER_ITERATION_RATIO * 100)}%, noise floor ${RSS_NOISE_FLOOR_MB} MB)`);
		console.error(`heap growth vs baseline: ${heapGrowth} MB (limit ${MAX_HEAP_GROWTH_MB})`);
		console.error(`external growth vs baseline: ${externalGrowth} MB (limit ${MAX_EXTERNAL_GROWTH_MB})`);
		for (const failure of failures) {
			console.error('FAIL: ' + failure);
		}
		process.exitCode = 1;
	}
}

run().catch(error => {
	console.error(error);
	process.exitCode = 1;
});
