import type { Realm, Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete, expectThrow, unsafeEvalAsStringInRealm } from "@isolated-vm/experimental/test/fixtures";
import { makeDirectResolver, makeLinker, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

async function makeTransferFunction(agent: Agent, realm: Realm, body: string) {
	const runtime = expect(await realm.instantiateRuntime());
	const module = expectComplete(await agent.compileModule(`
		import { transfer } from "isolated-vm://runtime";
		globalThis.fn = () => { ${body} };
	`));
	await module.link(realm, makeLinker(makeDirectResolver(), makeStaticLoader({ "isolated-vm://runtime": runtime })));
	expectComplete(await module.evaluate(realm));
	const global = await realm.acquireGlobalObject();
	return expect(await global.get("fn")) as Reference<(...args: unknown[]) => unknown>;
}

function byteLengthInRealm(agent: Agent, realm: Realm) {
	return unsafeEvalAsStringInRealm(agent, realm, () =>
		(globalThis as unknown as { buffer: ArrayBuffer }).buffer.byteLength);
}

await test("returned buffer is moved", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeTransferFunction(agent, realm,
		`const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
		globalThis.buffer = buffer;
		return transfer(buffer, [ buffer ]);`);
	const result = expectComplete(await fn.invoke([])) as ArrayBuffer;
	assert.deepStrictEqual(Array.from(new Uint8Array(result)), [ 1, 2, 3 ]);
	assert.equal(await byteLengthInRealm(agent, realm), 0);
});

await test("returned buffer reached through a view", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeTransferFunction(agent, realm,
		`const view = Uint8Array.from([ 1, 2, 3 ]);
		globalThis.buffer = view.buffer;
		return transfer(view, [ view.buffer ]);`);
	const result = expectComplete(await fn.invoke([])) as Uint8Array;
	assert.deepStrictEqual(Array.from(result), [ 1, 2, 3 ]);
	assert.equal(await byteLengthInRealm(agent, realm), 0);
});

await test("returned buffer keeps its identity", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeTransferFunction(agent, realm,
		`const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
		const view = new Uint8Array(buffer);
		return transfer({ buffer, view }, [ buffer ]);`);
	const result = expectComplete(await fn.invoke([])) as { buffer: ArrayBuffer; view: Uint8Array };
	assert.equal(result.view.buffer, result.buffer);
	assert.deepStrictEqual(Array.from(new Uint8Array(result.buffer)), [ 1, 2, 3 ]);
});

await test("unlisted buffer is copied", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeTransferFunction(agent, realm,
		`const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
		globalThis.buffer = buffer;
		return transfer(buffer);`);
	const result = expectComplete(await fn.invoke([])) as ArrayBuffer;
	assert.deepStrictEqual(Array.from(new Uint8Array(result)), [ 1, 2, 3 ]);
	assert.equal(await byteLengthInRealm(agent, realm), 3);
});

await test("unvisited buffer is detached", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeTransferFunction(agent, realm,
		`globalThis.buffer = new ArrayBuffer(3);
		return transfer("hello", [ globalThis.buffer ]);`);
	const result = expectComplete(await fn.invoke([]));
	assert.equal(result, "hello");
	assert.equal(await byteLengthInRealm(agent, realm), 0);
});

await test("getters throw", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeTransferFunction(agent, realm,
		`const buffer = new ArrayBuffer(3);
		const transferList = [ buffer ];
		Object.defineProperty(transferList, 0, {
			get() { return buffer }
		});
		return transfer("hello", transferList);`);
	expectThrow(await fn.invoke([]));
});

await test("invalid transfer list throws", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeTransferFunction(agent, realm, "return transfer(1, 'nope')");
	expectThrow(await fn.invoke([]));
});

await test("unknown transfer value throws", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeTransferFunction(agent, realm, "return transfer(1, [ {} ])");
	expectThrow(await fn.invoke([]));
});
