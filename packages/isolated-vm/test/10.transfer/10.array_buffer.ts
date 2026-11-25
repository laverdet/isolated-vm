import type { Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { describe, test } from "node:test";
import { expectComplete, isBun, makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

const sharedArrayBuffer = { skip: isBun && "bun can't transfer SharedArrayBuffer" };

await describe("array buffer", async () => {
	await test("transfer out", async () => {
		await using vm = await makeTestAgent();
		const ab = await vm.unsafeScript(() => Uint8Array.from([ 1, 2, 3 ]).buffer);
		assert.deepStrictEqual(new Uint8Array(ab), new Uint8Array([ 1, 2, 3 ]));
	});

	await test("transfer out same reference", async () => {
		await using vm = await makeTestAgent();
		const ab = await vm.unsafeScript(() => {
			// nb: also tests zero-length buffer
			const ab = new ArrayBuffer(0);
			return [ ab, ab ];
		});
		assert.strictEqual(ab[0], ab[1]);
	});

	await test("transfer in", async () => {
		await using vm = await makeTestAgent();
		const fn = await vm.defineFunction((ab: ArrayBuffer) => {
			assert.deepEqual([ ...new Uint8Array(ab) ], [ 1, 2, 3 ]);
		});
		expectComplete(await fn.invoke([ Uint8Array.from([ 1, 2, 3 ]).buffer ]));
	});

	await test("transfer in same reference", async () => {
		await using vm = await makeTestAgent();
		const fn = await vm.defineFunction((abs: ArrayBuffer[]) => {
			assert.equal(abs[0], abs[1]);
		});
		const ab = new ArrayBuffer(0);
		expectComplete(await fn.invoke([ [ ab, ab ] ]));
	});
});

await describe("shared array buffer", async () => {
	await test("transfer out", sharedArrayBuffer, async () => {
		await using vm = await makeTestAgent();
		const fn = await vm.defineFunction(() => {
			const uint8 = new Uint8Array(new SharedArrayBuffer(3));
			const read = new rt.Reference(() => [ ...uint8 ]) as unknown as Reference<() => number[]>;
			uint8.set([ 1, 2, 3 ]);
			return { sab: uint8.buffer, read };
		});
		const { sab, read } = expectComplete(await fn.invoke([]));
		assert.ok(sab instanceof SharedArrayBuffer);
		const uint8 = new Uint8Array(sab);
		assert.deepStrictEqual(uint8, new Uint8Array([ 1, 2, 3 ]));
		uint8.set([ 3, 2, 1 ]);
		assert.deepStrictEqual(expectComplete(await read.invoke([])), [ 3, 2, 1 ]);
	});

	await test("transfer out same reference", sharedArrayBuffer, async () => {
		await using vm = await makeTestAgent();
		const ab = await vm.unsafeScript(() => {
			// nb: also tests zero-length buffer
			const sab = new SharedArrayBuffer(0);
			return [ sab, sab ];
		});
		assert.strictEqual(ab[0], ab[1]);
	});

	await test("transfer in", sharedArrayBuffer, async () => {
		await using vm = await makeTestAgent();
		const fn = await vm.defineFunction((sab: SharedArrayBuffer) => {
			const uint8 = new Uint8Array(sab);
			assert.ok(uint8.buffer instanceof SharedArrayBuffer);
			assert.deepEqual([ ...uint8 ], [ 1, 2, 3 ]);
		});
		const sab = new SharedArrayBuffer(3);
		new Uint8Array(sab).set([ 1, 2, 3 ]);
		expectComplete(await fn.invoke([ sab ]));
	});

	await test("transfer in same reference", async () => {
		await using vm = await makeTestAgent();
		const fn = await vm.defineFunction((abs: ArrayBuffer[]) => {
			// eslint-disable-next-line @typescript-eslint/restrict-template-expressions, @typescript-eslint/no-base-to-string
			assert.equal(`${abs[0]}`, "[object ArrayBuffer]");
			assert.equal(abs[0], abs[1]);
		});
		const ab = new ArrayBuffer(0);
		expectComplete(await fn.invoke([ [ ab, ab ] ]));
	});
});

await describe("typed array", async () => {
	await test("transfer out", async () => {
		await using vm = await makeTestAgent();
		const [ uint8, uint8_copy ] = await vm.unsafeScript(() => {
			const uint8 = new Uint8Array([ 1, 2, 3 ]);
			return [ uint8, new Uint8Array(uint8.buffer) ];
		});
		assert.deepStrictEqual(uint8, new Uint8Array([ 1, 2, 3 ]));
		assert.strictEqual(uint8.buffer, uint8_copy.buffer);
	});

	await test("transfer in", async () => {
		await using vm = await makeTestAgent();
		const fn = await vm.defineFunction((uint8: Uint8Array) => {
			assert.deepEqual([ ...uint8 ], [ 1, 2, 3 ]);
		});
		expectComplete(await fn.invoke([ new Uint8Array([ 1, 2, 3 ]) ]));
	});

	await test("shared transfer out", sharedArrayBuffer, async () => {
		await using vm = await makeTestAgent();
		const [ shared_uint8, shared_uint8_copy ] = await vm.unsafeScript(() => {
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
