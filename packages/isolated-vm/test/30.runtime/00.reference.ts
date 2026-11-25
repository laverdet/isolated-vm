import type { Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Capability } from "@isolated-vm/experimental";
import { expect, expectComplete, makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("reference is constructible", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		for (const reference of [ new rt.Reference(null), new rt.Reference({}), new rt.Reference(() => {}) ]) {
			assert.ok(reference instanceof rt.Reference);
		}
	});
});

await test("reference transfers as host reference", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => new rt.Reference(() => "wow"));
	const reference = expectComplete(await fn.invoke([])) as unknown as Reference<() => string>;
	assert.equal(typeof reference.invoke, "function");
	assert.equal(expectComplete(await reference.invoke([])), "wow");
});

await test("reference is transferable within a nested value", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => {
		const object = { name: "object" };
		return { list: [ new rt.Reference(object), new rt.Reference(123) ], date: new Date(0), object };
	});
	const result = expectComplete(await fn.invoke([])) as unknown as {
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
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => {
		const buffer = Uint8Array.from([ 1, 2, 3 ]).buffer;
		return rt.transfer({ buffer, reference: new rt.Reference({ buffer }) }, [ buffer ]);
	});
	type Result = { buffer: ArrayBuffer; reference: Reference<{ buffer: ArrayBuffer }> };
	const result = expectComplete(await fn.invoke([])) as Result;
	assert.deepStrictEqual(Array.from(new Uint8Array(result.buffer)), [ 1, 2, 3 ]);
	// The referenced buffer was detached by the transfer
	const detached = await expect(await result.reference.get("buffer")).copy();
	assert.equal(detached.byteLength, 0);
});

await test("reference subclass", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => {
		class Subclass extends rt.Reference<string> {
			constructor() { super("subclass"); }
		}
		return new Subclass();
	});
	const reference = expectComplete(await fn.invoke([])) as unknown as Reference<string>;
	assert.equal(await reference.copy(), "subclass");
});

await test("reference prototype is a plain object", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		assert.deepEqual(rt.Reference.prototype, {});
		assert.equal(rt.Reference.name, "Reference");
		assert.equal(rt.Reference.length, 1);
	});
});

await test("reference requires `new`", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		// @ts-expect-error
		assert.throws(() => rt.Reference(1), { name: "TypeError", message: /new/ });
	});
});

await test("host reference hydrates as runtime reference", async () => {
	await using vm = await makeTestAgent();
	const make = await vm.defineFunction((value: unknown) => new rt.Reference(value));
	const check = await vm.defineFunction((reference: unknown) => reference instanceof rt.Reference);
	const identity = await vm.defineFunction((value: unknown) => value);
	// Make a reference in the agent, and send it back
	const reference = expectComplete(await make.invoke([ "value" ])) as unknown as Reference<string>;
	assert.equal(expectComplete(await check.invoke([ reference ])), true);
	assert.equal(expectComplete(await check.invoke([ { reference } ])), false);
	// A round trip arrives as a host reference to the same value
	const again = expectComplete(await identity.invoke([ reference ])) as Reference<string>;
	assert.equal(await again.copy(), "value");
	// Plain host objects are still copied
	assert.deepStrictEqual(expectComplete(await identity.invoke([ { value: 1 } ])), { value: 1 });
});

await test("reference is transferable through a capability", async () => {
	const { promise, resolve } = Promise.withResolvers<Reference<number>>();
	const send = (value: unknown) => { resolve(value as Reference<number>); };
	await using vm = await makeTestAgent({
		capabilities: { capability: { send: Capability.void(send) } },
	});
	await vm.unsafeModule('import { send } from "test:capability";', () => { send(new rt.Reference(42)); });
	const reference = await promise;
	assert.equal(await reference.copy(), 42);
});

await test("reference identity is preserved", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => {
		const reference = new rt.Reference(1);
		return [ reference, reference, new rt.Reference(1) ];
	});
	const [ left, right, other ] = expectComplete(await fn.invoke([])) as unknown as Reference<number>[];
	assert.strictEqual(left, right);
	assert.notStrictEqual(left, other);
});
