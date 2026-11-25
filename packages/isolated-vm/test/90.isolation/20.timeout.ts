import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Capability } from "@isolated-vm/experimental";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("timeout", async () => {
	await using vm = await makeTestAgent();
	assert.strictEqual(await vm.unsafeScript({ timeout: 20 }, () => { for (;;); }), null);
});

await test("reference.invoke timeout", async () => {
	await using vm = await makeTestAgent();
	const loop = await vm.defineFunction(() => { for (;;); });
	assert.strictEqual(await loop.invoke([], { timeout: 20 }), null);
});

await test("module.evaluate timeout", async () => {
	await using vm = await makeTestAgent();
	assert.strictEqual(await vm.unsafeModule({ timeout: 20 }, () => { for (;;); }), null);
});

await test("timeout edge cases", async () => {
	await using vm = await makeTestAgent();
	assert.strictEqual(await vm.unsafeScript({ timeout: 0 }, () => "ok"), null);
	assert.strictEqual(await vm.unsafeScript({ timeout: -1 }, () => "ok"), null);
	assert.strictEqual(await vm.unsafeScript({ timeout: Infinity }, () => "ok"), "ok");
	assert.strictEqual(await vm.unsafeScript({ timeout: Number.MAX_VALUE }, () => "ok"), "ok");
	assert.strictEqual(await vm.unsafeScript(() => "ok"), "ok");
});

await test("timeout paused during sync capability", async () => {
	const wait = () => new Promise<void>(resolve => { setTimeout(resolve, 100); });
	await using vm = await makeTestAgent({ capabilities: { wait: { default: Capability.sync(wait) } } });
	await vm.unsafeModule('import wait from "test:wait";', () => { Object.assign(globalThis, { wait }); });
	// Waiting on the host takes longer than the timeout, but that time doesn't count
	assert.strictEqual(await vm.unsafeScript({ timeout: 50 }, () => { void wait(); return "ok"; }), "ok");
	// The timeout still applies to the script's own time afterwards
	assert.strictEqual(await vm.unsafeScript({ timeout: 50 }, () => { void wait(); for (;;); }), null);
});
