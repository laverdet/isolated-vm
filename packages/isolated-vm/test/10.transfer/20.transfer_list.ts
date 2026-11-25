import * as assert from "node:assert/strict";
import { test } from "node:test";
import { expectComplete, makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("transferred buffer is moved", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction((buffer: ArrayBuffer) => {
		assert.deepEqual([ ...new Uint8Array(buffer) ], [ 1, 2, 3 ]);
	});
	const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
	expectComplete(await fn.invoke([ buffer ], { transfer: [ buffer ] }));
	assert.equal(buffer.byteLength, 0);
});

await test("transferred buffer reached through a view", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction((view: Uint8Array) => {
		assert.deepEqual([ ...view ], [ 1, 2, 3 ]);
	});
	const view = Uint8Array.from([ 1, 2, 3 ]);
	expectComplete(await fn.invoke([ view ], { transfer: [ view.buffer ] }));
	assert.equal(view.buffer.byteLength, 0);
});

await test("transferred buffer keeps its identity", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction((view: Uint8Array, buffer: ArrayBuffer) => {
		assert.equal(view.buffer, buffer);
		assert.deepEqual([ ...view ], [ 1, 2, 3 ]);
	});
	const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
	const view = new Uint8Array(buffer);
	expectComplete(await fn.invoke([ view, buffer ], { transfer: [ buffer ] }));
	assert.equal(buffer.byteLength, 0);
});

await test("transferred buffer visited before its view", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction((buffer: ArrayBuffer, view: Uint8Array) => {
		assert.equal(view.buffer, buffer);
		assert.deepEqual([ ...view ], [ 1, 2, 3 ]);
	});
	const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
	const view = new Uint8Array(buffer);
	expectComplete(await fn.invoke([ buffer, view ], { transfer: [ buffer ] }));
	assert.equal(buffer.byteLength, 0);
});

await test("unlisted buffer is copied", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction((buffer: ArrayBuffer) => {
		assert.deepEqual([ ...new Uint8Array(buffer) ], [ 1, 2, 3 ]);
	});
	const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
	expectComplete(await fn.invoke([ buffer ]));
	assert.equal(buffer.byteLength, 3);
});

await test("unvisited buffer throws", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => {});
	const buffer = new ArrayBuffer(3);
	assert.throws(() => void fn.invoke([], { transfer: [ buffer ] }), /unvisited/i);
	assert.equal(buffer.byteLength, 0);
});

await test("duplicate buffer throws", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => {});
	const buffer = new ArrayBuffer(3);
	assert.throws(() => void fn.invoke([], { transfer: [ buffer, buffer ] }));
	assert.equal(buffer.byteLength, 3);
});

await test("unknown transfer value throws", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => {});
	assert.throws(() => void fn.invoke([], { transfer: [ {} ] }));
});

await test("sparse transfer list throws", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => {});
	const buffer = new ArrayBuffer(3);
	const list = new Array<ArrayBuffer>(2);
	list[1] = buffer;
	assert.throws(() => void fn.invoke([], { transfer: list }), /sparse/i);
	assert.equal(buffer.byteLength, 3);
});

await test("throwing transfer list getter propagates", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => {});
	const list: ArrayBuffer[] = [];
	Object.defineProperty(list, 0, {
		enumerable: true,
		get: () => {
			throw new Error("boom");
		},
	});
	assert.throws(() => void fn.invoke([], { transfer: list }), /boom/);
});
