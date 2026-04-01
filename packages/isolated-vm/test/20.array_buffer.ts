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
