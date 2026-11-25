import * as assert from "node:assert/strict";
import { test } from "node:test";
import { expectComplete, expectThrow, makeTestAgent, unsafeIIFEAsString } from "@isolated-vm/experimental/test/fixtures";

const illegalAccess = new Error("illegal access");

await test("script source origin", async () => {
	await using vm = await makeTestAgent();
	const script = expectComplete(await vm.agent.compileScript(
		unsafeIIFEAsString(() => {
			try {
				throw new Error();
			} catch (error: any) {
				// eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
				return error.stack;
			}
		}),
		{
			origin: {
				name: "file:///test-script",
				location: { line: 100, column: 200 },
			},
		},
	));
	const result = expectComplete(await script.run(vm.realm));
	assert.ok(typeof result === "string");
	assert.ok(result.includes("file:///test-script:103"));
});

await test("script which throws", async () => {
	await using vm = await makeTestAgent();
	const origin = { name: "test:script" };
	const script = expectComplete(await vm.agent.compileScript("throw new Error('wow');", { origin }));
	const error = expectThrow(await script.run(vm.realm)) as Error;
	assert.equal(error.message, "wow");
	assert.ok(error.stack?.includes("test:script"));
});

await test("script with syntax error", async () => {
	await using vm = await makeTestAgent();
	const error = expectThrow(await vm.agent.compileScript("}"));
	// @ts-expect-error
	const message: unknown = error.message;
	assert.ok(typeof message === "string");
	assert.ok(message.includes("Unexpected token '}'"));
});

await test("script which returns a circular object", async () => {
	await using vm = await makeTestAgent();
	const result = await vm.unsafeScript(() => {
		const date = new Date();
		const object: Record<string, any> = {
			record: {},
			date1: date,
			date2: date,
		};
		object.object = object;
		return object;
	});
	assert.strictEqual(result.date1, result.date2);
	assert.strictEqual(result.object, result);
});

await test("script result should not invoke interceptors", async () => {
	await using vm = await makeTestAgent();
	await assert.rejects(vm.unsafeScript(() => {
		const object = Object.defineProperty({ name: "value" }, "name", {
			enumerable: true,
			get() { throw new Error("interceptor invoked"); },
		});
		return object;
	}), illegalAccess);
});
