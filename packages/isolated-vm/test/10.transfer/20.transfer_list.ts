import type { Agent, Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent as AgentClass, expect } from "@isolated-vm/experimental";
import { expectComplete, unsafeEvalAsStringInRealm } from "@isolated-vm/experimental/test/fixtures";

/** Returns a reference to a function which describes the buffers in its parameter */
async function makeDescribe(agent: Agent) {
	type Describe = (message?: unknown) => readonly (number | boolean | null)[] | null;
	const realm = expect(await agent.createRealm());
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.fn = (message: { buffer?: ArrayBuffer; view?: Uint8Array }) =>
			message.buffer
				? [ message.view ? message.view.buffer === message.buffer : null, ...new Uint8Array(message.buffer) ]
				: null;
	});
	const global = await realm.acquireGlobalObject();
	return expect(await global.get("fn")) as Reference<Describe>;
}

await test("transferred buffer is moved", async () => {
	await using agent = await AgentClass.create();
	const fn = await makeDescribe(agent);
	const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
	const result = expectComplete(await fn.invoke([ { buffer } ], { transfer: [ buffer ] }));
	assert.equal(buffer.byteLength, 0);
	assert.deepStrictEqual(result, [ null, 1, 2, 3 ]);
});

await test("transferred buffer reached through a view", async () => {
	await using agent = await AgentClass.create();
	const fn = await makeDescribe(agent);
	const view = Uint8Array.from([ 1, 2, 3 ]);
	const result = expectComplete(await fn.invoke([ { buffer: view.buffer, view } ], { transfer: [ view.buffer ] }));
	assert.equal(view.buffer.byteLength, 0);
	assert.deepStrictEqual(result, [ true, 1, 2, 3 ]);
});

await test("transferred buffer keeps its identity", async () => {
	await using agent = await AgentClass.create();
	const fn = await makeDescribe(agent);
	const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
	const view = new Uint8Array(buffer);
	const result = expectComplete(await fn.invoke([ { buffer, view } ], { transfer: [ buffer ] }));
	assert.equal(buffer.byteLength, 0);
	assert.deepStrictEqual(result, [ true, 1, 2, 3 ]);
});

await test("unlisted buffer is copied", async () => {
	await using agent = await AgentClass.create();
	const fn = await makeDescribe(agent);
	const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
	const result = expectComplete(await fn.invoke([ { buffer } ]));
	assert.equal(buffer.byteLength, 3);
	assert.deepStrictEqual(result, [ null, 1, 2, 3 ]);
});

await test("unvisited buffer is detached", async () => {
	await using agent = await AgentClass.create();
	const fn = await makeDescribe(agent);
	const buffer = new ArrayBuffer(3);
	expectComplete(await fn.invoke([ "hello" ], { transfer: [ buffer ] }));
	assert.equal(buffer.byteLength, 0);
});

await test("duplicate buffer throws", async () => {
	await using agent = await AgentClass.create();
	const fn = await makeDescribe(agent);
	const buffer = new ArrayBuffer(3);
	assert.throws(() => void fn.invoke([], { transfer: [ buffer, buffer ] }));
	assert.equal(buffer.byteLength, 3);
});

await test("unknown transfer value throws", async () => {
	await using agent = await AgentClass.create();
	const fn = await makeDescribe(agent);
	assert.throws(() => void fn.invoke([], { transfer: [ {} ] }));
});
