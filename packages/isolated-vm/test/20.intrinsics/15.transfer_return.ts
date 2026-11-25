import type { Reference } from "@isolated-vm/experimental";
import type { TestAgent } from "@isolated-vm/experimental/test/fixtures";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { expectComplete, expectThrow, makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

/** The transferred value, along with a probe for the byte length left behind in the realm */
type Probed<Type> = { result: Type; byteLength: Reference<() => number> };

/** Defines a realm function which returns `rt.transfer(...)`, typed as the `Type` which arrives on the host */
async function defineTransfer<Type>(vm: TestAgent, code: () => unknown): Promise<Reference<() => Type>> {
	return await vm.defineFunction(code) as Reference<() => Type>;
}

await test("returned buffer is moved", async () => {
	await using vm = await makeTestAgent();
	const fn = await defineTransfer<Probed<ArrayBuffer>>(vm, () => {
		const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
		return rt.transfer({ result: buffer, byteLength: new rt.Reference(() => buffer.byteLength) }, [ buffer ]);
	});
	const { result, byteLength } = expectComplete(await fn.invoke([]));
	assert.deepStrictEqual(Array.from(new Uint8Array(result)), [ 1, 2, 3 ]);
	assert.equal(expectComplete(await byteLength.invoke([])), 0);
});

await test("returned buffer reached through a view", async () => {
	await using vm = await makeTestAgent();
	const fn = await defineTransfer<Probed<Uint8Array>>(vm, () => {
		const view = Uint8Array.from([ 1, 2, 3 ]);
		const byteLength = new rt.Reference(() => view.buffer.byteLength);
		return rt.transfer({ result: view, byteLength }, [ view.buffer ]);
	});
	const { result, byteLength } = expectComplete(await fn.invoke([]));
	assert.deepStrictEqual(Array.from(result), [ 1, 2, 3 ]);
	assert.equal(expectComplete(await byteLength.invoke([])), 0);
});

await test("returned buffer keeps its identity", async () => {
	await using vm = await makeTestAgent();
	const fn = await defineTransfer<{ buffer: ArrayBuffer; view: Uint8Array }>(vm, () => {
		const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
		const view = new Uint8Array(buffer);
		return rt.transfer({ buffer, view }, [ buffer ]);
	});
	const result = expectComplete(await fn.invoke([]));
	assert.equal(result.view.buffer, result.buffer);
	assert.deepStrictEqual(Array.from(new Uint8Array(result.buffer)), [ 1, 2, 3 ]);
});

await test("unlisted buffer is copied", async () => {
	await using vm = await makeTestAgent();
	const fn = await defineTransfer<Probed<ArrayBuffer>>(vm, () => {
		const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
		return rt.transfer({ result: buffer, byteLength: new rt.Reference(() => buffer.byteLength) });
	});
	const { result, byteLength } = expectComplete(await fn.invoke([]));
	assert.deepStrictEqual(Array.from(new Uint8Array(result)), [ 1, 2, 3 ]);
	assert.equal(expectComplete(await byteLength.invoke([])), 3);
});

await test("unvisited buffer throws", async () => {
	await using vm = await makeTestAgent();
	const fn = await defineTransfer<Probed<Reference<() => string>>>(vm, () => {
		const buffer = new ArrayBuffer(3);
		return {
			result: new rt.Reference(() => rt.transfer("hello", [ buffer ])),
			byteLength: new rt.Reference(() => buffer.byteLength),
		};
	});
	const { result, byteLength } = expectComplete(await fn.invoke([]));
	expectThrow(await result.invoke([]));
	assert.equal(expectComplete(await byteLength.invoke([])), 0);
});

await test("getters throw", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => {
		const buffer = new ArrayBuffer(3);
		const transferList = [ buffer ];
		Object.defineProperty(transferList, 0, {
			get() { return buffer; },
		});
		return rt.transfer("hello", transferList);
	});
	expectThrow(await fn.invoke([]));
});

await test("invalid transfer list throws", async () => {
	await using vm = await makeTestAgent();
	// @ts-expect-error
	const fn = await vm.defineFunction(() => rt.transfer(1, "nope"));
	expectThrow(await fn.invoke([]));
});

await test("unknown transfer value throws", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => rt.transfer(1, [ {} ]));
	expectThrow(await fn.invoke([]));
});
