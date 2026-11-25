import type { Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Capability } from "@isolated-vm/experimental";
import { expectComplete, makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("promise reaction inherits the timeout of the task which enqueued it", async () => {
	await using vm = await makeTestAgent();
	// The script which attaches the reaction runs under a budget, and hands back the trigger
	const resolve = await vm.unsafeScript({ timeout: 50 }, () => {
		const { promise, resolve } = Promise.withResolvers<undefined>();
		void promise.then(() => { for (;;); });
		return new rt.Reference(resolve);
	}) as unknown as Reference<() => void>;
	// The host resumes the isolate without a timeout, but the reaction still runs under the budget
	// of the script which enqueued it. The termination happens inside the microtask checkpoint, after
	// `resolve()` itself returned, so the host task sees a normal completion.
	const start = performance.now();
	expectComplete(await resolve.invoke([]));
	assert.ok(performance.now() - start < 1000);
	// The realm is still usable afterwards
	assert.strictEqual(await vm.unsafeScript(() => "ok"), "ok");
});

await test("zone budget terminates and is shared by clones", async () => {
	await using vm = await makeTestAgent();
	// A zone created by the runtime applies its own budget
	assert.strictEqual(await vm.unsafeScript(() => new rt.Zone({ timeout: 20 }).run(() => { for (;;); })), null);
	// A clone spends the same budget, which is paused while neither zone is current
	await vm.unsafeScript(() => {
		const busy = (ms: number) => { const end = Date.now() + ms; while (Date.now() < end); };
		const zone = new rt.Zone({ timeout: 1000 });
		const clone = zone.clone();
		clone.run(() => { busy(50); });
		assert.deepEqual(
			[ Number(zone.timeout) < 960, Number(clone.timeout) < 960, new rt.Zone().timeout ],
			[ true, true, undefined ]);
	});
});

await test("enqueued microtask spends the budget of the zone which enqueued it", async () => {
	await using vm = await makeTestAgent();
	const start = performance.now();
	await vm.unsafeScript(() => {
		new rt.Zone({ timeout: 20 }).run(() => { rt.enqueueMicrotask(() => { for (;;); }); });
	});
	assert.ok(performance.now() - start < 1000);
});

await test("budget can be replaced or adjusted while live", async () => {
	await using vm = await makeTestAgent();
	// Extending the budget from inside the zone outlives the original deadline
	await vm.unsafeScript(() => {
		const busy = (ms: number) => { const end = Date.now() + ms; while (Date.now() < end); };
		const zone = new rt.Zone({ timeout: 20 });
		zone.run(() => { rt.Zone.current?.applyTimeout(1000); busy(50); });
		const adjusted = new rt.Zone({ timeout: 20 });
		adjusted.run(() => { rt.Zone.current?.adjustTimeout(1000); busy(50); });
		assert.deepEqual(
			[ Number(zone.timeout) > 900, Number(adjusted.timeout) > 900, new rt.Zone().applyTimeout(1) ],
			[ true, true, false ]);
	});
	// Adjusting a paused budget below zero expires it, which terminates the next run. A failed
	// assertion would be a throw completion rather than the termination's `null`.
	assert.strictEqual(await vm.unsafeScript(() => {
		const zone = new rt.Zone({ timeout: 1000 });
		assert.deepEqual([ zone.adjustTimeout(-2000), zone.timeout, zone.applyTimeout(1000) ], [ true, 0, false ]);
		return zone.run(() => { for (;;); });
	}), null);
});

await test("timeout paused during sync capability inside a reaction", async () => {
	const wait = () => new Promise<void>(resolve => { setTimeout(resolve, 100); });
	await using vm = await makeTestAgent({ capabilities: { wait: { default: Capability.sync(wait) } } });
	await vm.unsafeModule('import wait from "test:wait";', () => { Object.assign(globalThis, { wait }); });
	const enqueue = await vm.defineFunction(() => {
		const { wait } = globalThis as unknown as { wait: () => void };
		const { promise, resolve } = Promise.withResolvers<undefined>();
		let done = false;
		void promise.then(() => { wait(); done = true; });
		return { resolve: new rt.Reference(resolve), done: new rt.Reference(() => done) };
	});
	type Resolvers = { resolve: Reference<() => void>; done: Reference<() => boolean> };
	const { resolve, done } = expectComplete(await enqueue.invoke([], { timeout: 50 })) as unknown as Resolvers;
	expectComplete(await resolve.invoke([]));
	assert.strictEqual(expectComplete(await done.invoke([])), true);
});
