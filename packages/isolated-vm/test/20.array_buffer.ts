import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "@isolated-vm/experimental";
import { unsafeEvalAsString, unsafeEvalAsStringInRealm } from "./fixtures.js";

await test("array buffer transfer out", async () => {
	await using agent = await ivm.Agent.create();
	const ab = await unsafeEvalAsString(agent, () => {
		const uint8 = new Uint8Array([ 1, 2, 3 ]);
		return uint8.buffer;
	});
	const uint8 = new Uint8Array(ab);
	assert.deepStrictEqual(uint8, new Uint8Array([ 1, 2, 3 ]));
});

await test("array buffer same reference", async () => {
	await using agent = await ivm.Agent.create();
	const ab = await unsafeEvalAsString(agent, () => {
		// nb: also tests zero-length buffer
		const uint8 = new Uint8Array([]);
		return [ uint8.buffer, uint8.buffer ];
	});
	assert.strictEqual(ab[0], ab[1]);
});

await test("shared array buffer transfer out", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const sab = await unsafeEvalAsStringInRealm(agent, realm, () => {
		const sab = new SharedArrayBuffer(3);
		const uint8 = new Uint8Array(sab);
		uint8.set([ 1, 2, 3 ]);
		// @ts-expect-error
		globalThis.uint8 = uint8;
		return sab;
	});
	assert.ok(sab instanceof SharedArrayBuffer);
	const uint8 = new Uint8Array(sab);
	assert.deepStrictEqual(uint8, new Uint8Array([ 1, 2, 3 ]));
	uint8.set([ 3, 2, 1 ]);
	const next = await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		// eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
		const uint8: Uint8Array = globalThis.uint8;
		return [ ...uint8 ];
	});
	assert.deepStrictEqual(next, [ 3, 2, 1 ]);
});

await test("typed array transfer out", async () => {
	await using agent = await ivm.Agent.create();
	// eslint-disable-next-line @typescript-eslint/no-unused-vars
	const [ uint8, uint8_copy ] = await unsafeEvalAsString(agent, () => {
		const uint8 = new Uint8Array([ 1, 2, 3 ]);
		return [ uint8, new Uint8Array(uint8.buffer) ];
	});
	assert.deepStrictEqual(uint8, new Uint8Array([ 1, 2, 3 ]));
	// assert.strictEqual(uint8.buffer, uint8_copy.buffer);
});

await test("shared typed array transfer out", async () => {
	await using agent = await ivm.Agent.create();
	// eslint-disable-next-line @typescript-eslint/no-unused-vars
	const [ shared_uint8, shared_uint8_copy ] = await unsafeEvalAsString(agent, () => {
		const sab = new SharedArrayBuffer(3);
		const uint8 = new Uint8Array(sab);
		uint8.set([ 1, 2, 3 ]);
		return [ uint8, new Uint8Array(uint8.buffer) ];
	});
	assert.ok(shared_uint8.buffer instanceof SharedArrayBuffer);
	assert.deepStrictEqual(shared_uint8, new Uint8Array([ 1, 2, 3 ]));
	// assert.strictEqual(shared_uint8.buffer, shared_uint8_copy.buffer);
});
