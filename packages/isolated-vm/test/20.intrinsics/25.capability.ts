import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, Capability, expect } from "@isolated-vm/experimental";
import { expectComplete, expectThrow } from "@isolated-vm/experimental/test/fixtures";

/** Evaluates `source` with `fn` bound to `capability`, and returns the completion & `globalThis.result` */
async function run(capability: Capability, source: string) {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const module = expectComplete(await agent.compileModule(`import fn from "fn"; ${source}`));
	await module.link(realm, async () => expect(await realm.createCapability({ default: capability }, { origin: "fn" })));
	const completion = await module.evaluate(realm);
	const result = await expect(await (await realm.acquireGlobalObject()).get("result")).copy();
	return { completion, result };
}

await test("sync capability resolves", async () => {
	const add = Capability.sync((left: unknown, right: unknown) => Promise.resolve(Number(left) + Number(right)));
	assert.equal((await run(add, "globalThis.result = fn(1, 2);")).result, 3);
});

await test("sync capability rejects", async () => {
	const fail = Capability.sync(() => Promise.reject(new Error("nope")));
	assert.equal((await run(fail, "try { fn(); } catch (error) { globalThis.result = error.message; }")).result, "nope");
	// eslint-disable-next-line @typescript-eslint/prefer-promise-reject-errors
	const uninspectable = Capability.sync(() => Promise.reject(Symbol("reason")));
	assert.equal((await run(uninspectable, "try { fn(); } catch (error) { globalThis.result = error instanceof Error; }")).result, true);
});

await test("sync capability throws", async () => {
	const fail = Capability.sync(() => { throw new Error("nope"); });
	assert.match(String(expectThrow((await run(fail, "fn();")).completion)), /nope/);
});

await test("abandoned sync capability resolves null", async () => {
	const abandoned = Capability.sync(() => new Promise(() => void setTimeout(() => global.gc?.(), 10)));
	assert.equal((await run(abandoned, "globalThis.result = fn();")).result, null);
});

await test("capability is one-shot", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const once = Capability.sync(() => {});
	expect(await realm.createCapability({ default: once }, { origin: "fn" }));
	assert.throws(() => realm.createCapability({ default: once }, { origin: "fn" }), /already used/);
});

await test("dispose while suspended", async () => {
	const agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const module = expectComplete(await agent.compileModule('import fn from "fn"; fn();'));
	const forever = Capability.sync(() => new Promise(() => void agent.disposeAsync()));
	await module.link(realm, async () => expect(await realm.createCapability({ default: forever }, { origin: "fn" })));
	await module.evaluate(realm);
});
