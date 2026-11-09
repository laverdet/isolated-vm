import type { Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, Capability, expect, expectComplete } from "@isolated-vm/experimental";
import "@isolated-vm/experimental/test/fixtures";

await test("timeout", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const loop = expectComplete(await agent.compileScript("for(;;);"));
	assert.strictEqual(await loop.run(realm, { timeout: 20 }), null);
});

await test("reference.invoke timeout", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const script = expectComplete(await agent.compileScript("globalThis.loop = () => { for(;;); }; undefined;"));
	expectComplete(await script.run(realm));
	const global = await realm.acquireGlobalObject();
	const loop = expect(await global.get("loop")) as Reference<() => void>;
	assert.strictEqual(await loop.invoke([], { timeout: 20 }), null);
});

await test("module.evaluate timeout", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const loop = expectComplete(await agent.compileModule("for(;;);"));
	await loop.link(realm, () => undefined);
	assert.strictEqual(await loop.evaluate(realm, { timeout: 20 }), null);
});

await test("timeout edge cases", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const script = expectComplete(await agent.compileScript("'ok'"));
	assert.strictEqual(await script.run(realm, { timeout: 0 }), null);
	assert.strictEqual(await script.run(realm, { timeout: -1 }), null);
	assert.strictEqual(expectComplete(await script.run(realm, { timeout: Infinity })), "ok");
	assert.strictEqual(expectComplete(await script.run(realm, { timeout: Number.MAX_VALUE })), "ok");
	assert.strictEqual(expectComplete(await script.run(realm)), "ok");
});

await test("timeout paused during sync capability", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const wait = Capability.sync(() => new Promise<void>(resolve => { setTimeout(resolve, 100); }));
	const capability = expect(await realm.createCapability({ default: wait }, { origin: "wait" }));
	const module = expectComplete(await agent.compileModule('import wait from "wait"; globalThis.wait = wait;'));
	await module.link(realm, () => capability);
	await module.evaluate(realm);
	// Waiting on the host takes longer than the timeout, but that time doesn't count
	const script = expectComplete(await agent.compileScript("wait(); 'ok'"));
	assert.strictEqual(expectComplete(await script.run(realm, { timeout: 50 })), "ok");
	// The timeout still applies to the script's own time afterwards
	const loop = expectComplete(await agent.compileScript("wait(); for(;;);"));
	assert.strictEqual(await loop.run(realm, { timeout: 50 }), null);
});
