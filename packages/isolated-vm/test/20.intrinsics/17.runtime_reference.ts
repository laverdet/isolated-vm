import type { Realm, Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete, expectThrow } from "@isolated-vm/experimental/test/fixtures";
import { makeDirectResolver, makeLinker, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

type AnyFunction = (...args: unknown[]) => unknown;

/** Evaluates `body` as a module which has the runtime `Reference` class in scope */
async function evaluateWithRuntime(agent: Agent, realm: Realm, body: string, loader: Record<string, Awaited<ReturnType<Realm["instantiateRuntime"]>>> = {}) {
	const runtime = expect(await realm.instantiateRuntime());
	const module = expectComplete(await agent.compileModule(`
		import { Reference, transfer } from "isolated-vm:runtime";
		${body}
	`));
	await module.link(realm, makeLinker(makeDirectResolver(), makeStaticLoader({ "isolated-vm:runtime": runtime, ...loader })));
	expectComplete(await module.evaluate(realm));
}

/** Returns a reference to a function defined with the runtime `Reference` class in scope */
async function makeRuntimeFunction(agent: Agent, realm: Realm, body: string) {
	await evaluateWithRuntime(agent, realm, `globalThis.fn = () => { ${body} };`);
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

await test("reference transfers as host reference", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, "return new Reference(() => 'wow');");
	const reference = expectComplete(await fn.invoke([])) as Reference<() => string>;
	assert.equal(typeof reference.invoke, "function");
	assert.equal(expectComplete(await reference.invoke([])), "wow");
});

await test("reference is transferable within a nested value", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const object = { name: "object" };
		return { list: [ new Reference(object), new Reference(123) ], date: new Date(0), object };`);
	const result = expectComplete(await fn.invoke([])) as {
		list: [ Reference<{ name: string }>, Reference<number> ];
		date: Date;
		object: { name: string };
	};
	assert.deepStrictEqual(result.object, { name: "object" });
	assert.deepStrictEqual(result.date, new Date(0));
	assert.equal(await expect(await result.list[0].get("name")).copy(), "object");
	assert.equal(await result.list[1].copy(), 123);
});

await test("reference is transferable with a transfer list", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
		globalThis.buffer = buffer;
		return transfer({ buffer, reference: new Reference({ buffer }) }, [ buffer ]);`);
	const result = expectComplete(await fn.invoke([])) as { buffer: ArrayBuffer; reference: Reference<{ buffer: ArrayBuffer }> };
	assert.deepStrictEqual(Array.from(new Uint8Array(result.buffer)), [ 1, 2, 3 ]);
	// The referenced buffer was detached by the transfer
	const detached = await expect(await result.reference.get("buffer")).copy();
	assert.equal(detached.byteLength, 0);
});

await test("reference subclass", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		class Subclass extends Reference { constructor() { super("subclass"); } }
		return new Subclass();`);
	const reference = expectComplete(await fn.invoke([])) as Reference<string>;
	assert.equal(await reference.copy(), "subclass");
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

await test("host reference hydrates as runtime reference", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	await evaluateWithRuntime(agent, realm, `
		globalThis.make = value => new Reference(value);
		globalThis.check = reference => reference instanceof Reference;
		globalThis.identity = value => value;`);
	const global = await realm.acquireGlobalObject();
	const make = expect(await global.get("make")) as Reference<AnyFunction>;
	const check = expect(await global.get("check")) as Reference<AnyFunction>;
	const identity = expect(await global.get("identity")) as Reference<AnyFunction>;
	// Make a reference in the agent, and send it back
	const reference = expectComplete(await make.invoke([ "value" ])) as Reference<string>;
	assert.equal(expectComplete(await check.invoke([ reference ])), true);
	assert.equal(expectComplete(await check.invoke([ { reference } ])), false);
	// A round trip arrives as a host reference to the same value
	const again = expectComplete(await identity.invoke([ reference ])) as Reference<string>;
	assert.equal(await again.copy(), "value");
	// Plain host objects are still copied
	assert.deepStrictEqual(expectComplete(await identity.invoke([ { value: 1 } ])), { value: 1 });
});

await test("reference is transferable through a capability", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const { promise, resolve } = Promise.withResolvers<Reference<number>>();
	const capability = expect(await realm.createCapability(
		{ send(value: unknown) { resolve(value as Reference<number>); } },
		{ origin: "test:capability" },
	));
	await evaluateWithRuntime(agent, realm, `
		import { send } from "test:capability";
		send(new Reference(42));`, { "test:capability": capability });
	const reference = await promise;
	assert.equal(await reference.copy(), 42);
});

await test("reference identity is preserved", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const reference = new Reference(1);
		return [ reference, reference, new Reference(1) ];`);
	const [ left, right, other ] = expectComplete(await fn.invoke([])) as Reference<number>[];
	assert.strictEqual(left, right);
	assert.notStrictEqual(left, other);
});
