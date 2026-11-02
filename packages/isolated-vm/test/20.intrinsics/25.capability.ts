import type { AbstractModule, Realm } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, Capability, expect } from "@isolated-vm/experimental";
import { expectComplete, expectThrow } from "@isolated-vm/experimental/test/fixtures";

/** Evaluates `source` in `realm`, with `fn` imported from the capability module */
async function evaluate(agent: Agent, realm: Realm, fn: AbstractModule, source: string) {
	const module = expectComplete(await agent.compileModule(`import fn from "fn"; ${source}`));
	await module.link(realm, () => fn);
	return module.evaluate(realm);
}

const createFn = async (realm: Realm, capability: Capability) => expect(await realm.createCapability({ default: capability }, { origin: "fn" }));
const copy = async (realm: Realm, key: string) => expect(await (await realm.acquireGlobalObject()).get(key)).copy();

/** Evaluates `source` with `fn` bound to `capability`, and returns the completion & `globalThis.result` */
async function run(capability: Capability, source: string) {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const completion = await evaluate(agent, realm, await createFn(realm, capability), source);
	return { completion, result: await copy(realm, "result") };
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
	await createFn(realm, once);
	assert.throws(() => realm.createCapability({ default: once }, { origin: "fn" }), /already used/);
});

await test("suspended tasks run concurrently", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const gates = [ 0, 1, 2 ].map(() => Promise.withResolvers<number>());
	const suspensions = [ 0, 1, 2 ].map(() => Promise.withResolvers<null>());
	const wait = Capability.sync((index: number) => {
		suspensions[index]?.resolve(null);
		return gates[index]?.promise;
	});
	const fn = await createFn(realm, wait);
	const script = expectComplete(await agent.compileScript("globalThis.marker = (globalThis.marker ?? 0) + 1"));
	// Two evaluations suspend at once, then a script runs on a third thread while both are waiting
	const first = evaluate(agent, realm, fn, "globalThis.first = fn(0) + fn(2); globalThis.result = globalThis.marker;");
	const second = evaluate(agent, realm, fn, "globalThis.second = fn(1);");
	await Promise.all([ suspensions[0]?.promise, suspensions[1]?.promise ]);
	expectComplete(await script.run(realm));
	gates[1]?.resolve(20);
	expectComplete(await second);
	gates[0]?.resolve(10);
	await suspensions[2]?.promise;
	expectComplete(await script.run(realm));
	gates[2]?.resolve(30);
	expectComplete(await first);
	assert.deepEqual([ await copy(realm, "first"), await copy(realm, "second"), await copy(realm, "result") ], [ 40, 20, 2 ]);
});

await test("dispose while suspended", async () => {
	const agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const forever = Capability.sync(() => new Promise(() => void agent.disposeAsync()));
	await evaluate(agent, realm, await createFn(realm, forever), "fn();");
});
