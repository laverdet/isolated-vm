import * as assert from "node:assert/strict";
import { describe, test } from "node:test";
import * as ivm from "@isolated-vm/experimental";
import { injectAssert, unsafeEvalAsString, unsafeEvalAsStringInRealm } from "./fixtures.js";

await describe("array buffer", async () => {
	await using agent = await ivm.Agent.create();
	await describe("transfer out", async () => {
		const ab = await unsafeEvalAsString(agent, () => {
			const uint8 = new Uint8Array([ 1, 2, 3 ]);
			return uint8.buffer;
		});
		const uint8 = new Uint8Array(ab);
		assert.deepStrictEqual(uint8, new Uint8Array([ 1, 2, 3 ]));
	});

	await test("transfer out same reference", async () => {
		const ab = await unsafeEvalAsString(agent, () => {
			// nb: also tests zero-length buffer
			const ab = new ArrayBuffer(0);
			return [ ab, ab ];
		});
		assert.strictEqual(ab[0], ab[1]);
	});

	await test("transfer in", async () => {
		const realm = await agent.createRealm();
		await injectAssert(agent, realm);
		const global = await realm.acquireGlobalObject();
		await global.set("ab", new Uint8Array([ 1, 2, 3 ]).buffer);
		await unsafeEvalAsStringInRealm(agent, realm, () => {
			// @ts-expect-error
			const uint8 = new Uint8Array(ab);
			assert.deepStrictEqual([ ...uint8 ], [ 1, 2, 3 ]);
		});
	});

	await test("transfer in same reference", async () => {
		const realm = await agent.createRealm();
		await injectAssert(agent, realm);
		const global = await realm.acquireGlobalObject();
		const ab = new ArrayBuffer(0);
		await global.set("abs", [ ab, ab ]);
		await unsafeEvalAsStringInRealm(agent, realm, () => {
			// @ts-expect-error
			// eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
			assert.strictEqual(abs[0], abs[1]);
		});
	});
});

await describe("shared array buffer", async () => {
	await using agent = await ivm.Agent.create();
	await describe("transfer out", async () => {
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

	await test("transfer out same reference", async () => {
		const ab = await unsafeEvalAsString(agent, () => {
			// nb: also tests zero-length buffer
			const sab = new SharedArrayBuffer(0);
			return [ sab, sab ];
		});
		assert.strictEqual(ab[0], ab[1]);
	});

	await test("transfer in", async () => {
		const realm = await agent.createRealm();
		await injectAssert(agent, realm);
		const global = await realm.acquireGlobalObject();
		const sab = new SharedArrayBuffer(3);
		const uint8 = new Uint8Array(sab);
		uint8.set([ 1, 2, 3 ]);
		await global.set("sab", sab);
		await unsafeEvalAsStringInRealm(agent, realm, () => {
			// @ts-expect-error
			const uint8 = new Uint8Array(globalThis.sab);
			assert.deepStrictEqual([ ...uint8 ], [ 1, 2, 3 ]);
		});
	});

	await test("transfer in same reference", async () => {
		const realm = await agent.createRealm();
		await injectAssert(agent, realm);
		const global = await realm.acquireGlobalObject();
		const sab = new SharedArrayBuffer(0);
		await global.set("sabs", [ sab, sab ]);
		await unsafeEvalAsStringInRealm(agent, realm, () => {
			// @ts-expect-error
			// eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
			assert.strictEqual(sabs[0], sabs[1]);
		});
	});
});

await describe("typed array", async () => {
	await using agent = await ivm.Agent.create();
	await test("transfer out", async () => {
		const [ uint8, uint8_copy ] = await unsafeEvalAsString(agent, () => {
			const uint8 = new Uint8Array([ 1, 2, 3 ]);
			return [ uint8, new Uint8Array(uint8.buffer) ];
		});
		assert.deepStrictEqual(uint8, new Uint8Array([ 1, 2, 3 ]));
		assert.strictEqual(uint8.buffer, uint8_copy.buffer);
	});

	await test("transfer in", async () => {
		const realm = await agent.createRealm();
		await injectAssert(agent, realm);
		const global = await realm.acquireGlobalObject();
		const uint8 = new Uint8Array([ 1, 2, 3 ]);
		uint8.set([ 1, 2, 3 ]);
		await global.set("uint8", uint8);
		await unsafeEvalAsStringInRealm(agent, realm, () => {
			// @ts-expect-error
			// eslint-disable-next-line @typescript-eslint/no-unsafe-assignment
			assert.deepStrictEqual([ ...globalThis.uint8 ], [ 1, 2, 3 ]);
		});
	});

	await test("shared transfer out", async () => {
		const [ shared_uint8, shared_uint8_copy ] = await unsafeEvalAsString(agent, () => {
			const sab = new SharedArrayBuffer(3);
			const uint8 = new Uint8Array(sab);
			uint8.set([ 1, 2, 3 ]);
			return [ uint8, new Uint8Array(uint8.buffer) ];
		});
		assert.ok(shared_uint8.buffer instanceof SharedArrayBuffer);
		assert.deepStrictEqual(shared_uint8, new Uint8Array([ 1, 2, 3 ]));
		assert.strictEqual(shared_uint8.buffer, shared_uint8_copy.buffer);
	});
});
