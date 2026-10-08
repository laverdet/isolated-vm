import type { Realm, Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete, expectThrow } from "@isolated-vm/experimental/test/fixtures";
import { makeDirectResolver, makeLinker, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

type AnyFunction = (...args: unknown[]) => unknown;

async function makeRuntimeFunction(agent: Agent, realm: Realm, body: string) {
	const runtime = expect(await realm.instantiateRuntime());
	const module = expectComplete(await agent.compileModule(`
		import { Reference } from "isolated-vm:runtime";
		globalThis.fn = () => { ${body} };
	`));
	await module.link(realm, makeLinker(makeDirectResolver(), makeStaticLoader({ "isolated-vm:runtime": runtime })));
	expectComplete(await module.evaluate(realm));
	const global = await realm.acquireGlobalObject();
	return expect(await global.get("fn")) as Reference<AnyFunction>;
}

await test("reference is constructible", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const references = [ new Reference(null), new Reference({}), new Reference(() => {}) ];
		return references.every(reference => reference instanceof Reference);
	`);
	assert.equal(expectComplete(await fn.invoke([])), true);
});

await test("reference subclass", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		class Subclass extends Reference { constructor() { super("subclass"); } }
		return new Subclass() instanceof Reference;`);
	assert.equal(expectComplete(await fn.invoke([])), true);
});

await test("reference prototype is a plain object", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, "return [ Reference.prototype, Reference.name, Reference.length ];");
	assert.deepStrictEqual(expectComplete(await fn.invoke([])), [ {}, "Reference", 1 ]);
});

await test("reference requires `new`", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, "return Reference(1);");
	assert.match(String(expectThrow(await fn.invoke([]))), /new/);
});
