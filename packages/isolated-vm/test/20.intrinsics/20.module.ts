import * as assert from "node:assert/strict";
import { mock, test } from "node:test";
import { Capability } from "@isolated-vm/experimental";
import { expectComplete, expectThrow, makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("module linker", async () => {
	await using vm = await makeTestAgent();
	const left = expectComplete(await vm.agent.compileModule(`
		import { right } from "right";
		globalThis.result = right();`));
	const right = expectComplete(await vm.agent.compileModule('export function right() { return "hello"; }'));
	await left.link(vm.realm, specifier => {
		assert.equal(specifier, "right");
		return right;
	});
	await left.evaluate(vm.realm);
	assert.equal(await vm.copy("result"), "hello");
});

await test("nested compilation in module linker", async () => {
	await using vm = await makeTestAgent();
	const left = expectComplete(await vm.agent.compileModule('import "right";'));
	await left.link(vm.realm, async () => expectComplete(await vm.agent.compileModule("export {};")));
});

await test("synthetic module capability", async () => {
	const capability = mock.fn<(...args: unknown[]) => void>();
	await using vm = await makeTestAgent({
		capabilities: { capability: { default: Capability.void(capability) } },
	});
	await vm.unsafeModule('import capability from "test:capability";', () => { capability("hello", "world"); });
	assert.deepEqual(capability.mock.calls.map(call => call.arguments), [ [ "hello", "world" ] ]);
});

await test("synthetic module with circular reference", async () => {
	const capability = (value1: unknown) => {
		// @ts-expect-error
		assert.strictEqual(value1.date1, value1.date2);
		// @ts-expect-error
		assert.strictEqual(value1.object, value1);
	};
	await using vm = await makeTestAgent({
		capabilities: { capability: { default: Capability.void(capability) } },
	});
	await vm.unsafeModule('import capability from "test:capability";', () => {
		const date = new Date();
		const object: Record<string, unknown> = {
			record: {},
			date1: date,
			date2: date,
		};
		object.object = object;
		capability(object);
	});
});

await test("synthetic module with data templates", async () => {
	const string = "string";
	await using vm = await makeTestAgent({
		capabilities: { capability: { string } },
	});
	await vm.unsafeModule('import { string } from "test:capability";', () => { assert.equal(string, "string"); });
});

await test("throw from linker", async () => {
	await using vm = await makeTestAgent();
	const left = expectComplete(await vm.agent.compileModule('import "right";'));
	await assert.rejects(left.link(vm.realm, () => {
		throw new Error("linker error");
	}));
});

await test("throw from evaluation", async () => {
	await using vm = await makeTestAgent();
	const origin = { name: "test:module" };
	const module = expectComplete(await vm.agent.compileModule('throw new Error("wow");', { origin }));
	await module.link(vm.realm, vm.linker);
	const error = expectThrow(await module.evaluate(vm.realm)) as Error;
	assert.equal(error.message, "wow");
	assert.ok(error.stack?.includes("test:module"));
});

await test("missing imports", async () => {
	await using vm = await makeTestAgent();
	const left = expectComplete(await vm.agent.compileModule('import { nothing } from "right";'));
	const right = expectComplete(await vm.agent.compileModule("export {};"));
	await assert.rejects(left.link(vm.realm, () => right));
});
