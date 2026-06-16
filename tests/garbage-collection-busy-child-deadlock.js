// node-args: --expose-gc
//
// Regression test for the parent-GC-vs-busy-child deadlock.
//
// `MarkSweepCompactPrologue` pushes memory pressure onto child isolates from the parent isolate's
// GC prologue. If it delivers that pressure by taking `Executor::Lock` on the child, it constructs
// a blocking `v8::Locker`; a child stuck in an unbounded script never releases it, freezing the
// parent mid-GC. The pressure must instead be delivered off-thread so the parent never blocks.
//
// The child here runs an unbounded loop with a bounded timeout, so the test always terminates even
// if the bug is present. We then assert on elapsed time: the fix completes the parent's GC loop in
// a few hundred ms, while the blocking version stalls until the child's timeout fires (~CHILD_TIMEOUT).
const ivm = require('isolated-vm');

const CHILD_TIMEOUT = 3000;
// Generous margin over the fix's real cost (~300ms) but well under a stall on the child's lock.
const MAX_PARENT_LOOP_MS = 1500;

async function run() {
	const isolate = new ivm.Isolate({ memoryLimit: 128 });
	const context = isolate.createContextSync();

	// Kick off an unbounded script on the child's own thread; don't await it. This keeps the child
	// isolate in the `Running` state while the parent below forces full GCs.
	const busy = context.eval('while (true) {}', { timeout: CHILD_TIMEOUT }).catch(() => {});

	// Hammer the parent isolate with full GCs while the child is busy. Each GC runs
	// `MarkSweepCompactPrologue` against the `Running` child — the exact deadlock path.
	const start = Date.now();
	for (let i = 0; i < 50; i++) {
		const junk = [];
		for (let j = 0; j < 50000; j++) junk.push({ j });
		void junk;
		global.gc();
		await new Promise(resolve => setTimeout(resolve, 5));
	}
	const elapsed = Date.now() - start;

	await busy;
	isolate.dispose();

	if (elapsed > MAX_PARENT_LOOP_MS) {
		// The parent blocked on the busy child's lock instead of delivering pressure off-thread.
		console.error(`parent GC loop stalled for ${elapsed}ms (limit ${MAX_PARENT_LOOP_MS}ms)`);
		process.exit(1);
	}
	console.log('pass');
}

run().catch(error => {
	console.error(error);
	process.exit(1);
});
