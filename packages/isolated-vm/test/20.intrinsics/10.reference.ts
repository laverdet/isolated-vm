import type { Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { inspect } from "node:util";
import { Capability } from "@isolated-vm/experimental";
import { expect, expectComplete, makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

const illegalAccess = new Error("illegal access");

await test("function reference invoke", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction(() => "wow");
	assert.equal(expectComplete(await fn.invoke([])), "wow");
});

await test("invoke function cross-param reference", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction((left: unknown, right: unknown) => {
		assert.equal(left, right);
		return [ left, right ];
	});
	const object = {};
	const [ left, right ] = expectComplete(await fn.invoke([ object, object ]));
	assert.equal(left, right);
});

await test("invoke function reference with circular object", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction((value: Record<string, unknown>) => {
		assert.equal(value.date1, value.date2);
		assert.equal(value.object, value);
		return value;
	});
	const date = new Date();
	const object: Record<string, unknown> = {
		record: {},
		date1: date,
		date2: date,
	};
	object.object = object;
	const result = expectComplete(await fn.invoke([ object ]));
	assert.equal(result.date1, result.date2);
	assert.equal(result.object, result);
});

await test("get & set should not invoke proxies", async () => {
	await using vm = await makeTestAgent();
	const make = await vm.defineFunction(() => new rt.Reference(new Proxy({}, {
		get() { throw new Error("interceptor invoked"); },
		set() { throw new Error("interceptor invoked"); },
	})));
	const proxy = expectComplete(await make.invoke([])) as unknown as Reference<object>;
	expect(await proxy.get("name"));
	expect(await proxy.set("name", undefined));
});

await test("copy should not invoke interceptors", async () => {
	await using vm = await makeTestAgent();
	const make = await vm.defineFunction(() => new rt.Reference(Object.defineProperty({}, "name", {
		enumerable: true,
		get() { throw new Error("interceptor invoked"); },
	})));
	const object = expectComplete(await make.invoke([])) as unknown as Reference<object>;
	await assert.rejects(object.copy(), illegalAccess);
});

await test("invoke should not invoke interceptors", async () => {
	await using vm = await makeTestAgent();
	const fn = await vm.defineFunction((_value?: unknown) => {});
	await vm.unsafeScript(() => {
		// eslint-disable-next-line no-extend-native
		Object.defineProperty(Object.prototype, "name", {
			set() { throw new Error("interceptor invoked"); },
		});
	});
	await assert.rejects(
		async () => expectComplete(await fn.invoke([ { name: "value" } ])),
		illegalAccess,
	);
});

await test("copy settles immediately when the isolate is idle, and waits when it is busy", async () => {
	// `started` resolves once the busy loop is running, and therefore holding the isolate lock
	const { promise: started, resolve: onStarted } = Promise.withResolvers<null>();
	const signal = () => onStarted(null);
	await using vm = await makeTestAgent({
		capabilities: { signal: { default: Capability.void(signal) } },
	});
	await vm.unsafeModule('import signal from "test:signal";', () => {
		Object.assign(globalThis, {
			value: { nested: [ 1, "two", true ] },
			busy: () => { signal(); const end = Date.now() + 200; while (Date.now() < end); },
		});
	});
	const value = await vm.get("value");

	// Idle: only works sometimes. If there's a crash the test will fail intermittently.
	// eslint-disable-next-line @typescript-eslint/await-thenable
	await undefined;
	const idle = value.copy();
	assert.deepEqual(await idle, { nested: [ 1, "two", true ] });

	// Busy: the copy falls back to the scheduler and completes once the isolate is free
	const busy = await vm.get("busy") as Reference<() => void>;
	const running = busy.invoke([]);
	await started;
	const deferred = value.copy();
	assert.match(inspect(deferred), /pending/);
	assert.deepEqual(await deferred, { nested: [ 1, "two", true ] });
	expectComplete(await running);
});
