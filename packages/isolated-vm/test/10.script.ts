import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "isolated-vm";
import { unsafeIIFEAsString, unwrapCompletion, unwrapThrowCompletion } from "./fixtures.js";

await test("script source origin", async () => {
	await using agent = await ivm.Agent.create();
	const script = unwrapCompletion(await agent.compileScript(
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
	const realm = await agent.createRealm();
	const result = unwrapCompletion(await script.run(realm));
	assert.ok(typeof result === "string");
	assert.ok(result.includes("file:///test-script:103"));
});

await test("script which throws", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const script = unwrapCompletion(await agent.compileScript("throw new Error('Hello');"));
	const error = unwrapThrowCompletion(await script.run(realm));
	// @ts-expect-error
	const message: unknown = error.message;
	assert.ok(typeof message === "string");
	assert.ok(message.includes("Hello"));
});

await test("script with syntax error", async () => {
	await using agent = await ivm.Agent.create();
	const error = unwrapThrowCompletion(await agent.compileScript("}"));
	// @ts-expect-error
	const message: unknown = error.message;
	assert.ok(typeof message === "string");
	assert.ok(message.includes("Unexpected token '}'"));
});
