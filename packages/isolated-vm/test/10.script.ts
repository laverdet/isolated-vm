import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "@isolated-vm/experimental";
import { expectComplete, expectThrow, unsafeEvalAsStringInRealm, unsafeIIFEAsString } from "./fixtures.js";

await test("script source origin", async () => {
	await using agent = await ivm.Agent.create();
	const script = expectComplete(await agent.compileScript(
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
	const result = expectComplete(await script.run(realm));
	assert.ok(typeof result === "string");
	assert.ok(result.includes("file:///test-script:103"));
});

await test("script which throws", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const script = expectComplete(await agent.compileScript("throw new Error('wow');", { origin: { name: "test:script" } }));
	const error = expectThrow(await script.run(realm)) as Error;
	assert.equal(error.message, "wow");
	assert.ok(error.stack?.includes("test:script"));
});

await test("script with syntax error", async () => {
	await using agent = await ivm.Agent.create();
	const error = expectThrow(await agent.compileScript("}"));
	// @ts-expect-error
	const message: unknown = error.message;
	assert.ok(typeof message === "string");
	assert.ok(message.includes("Unexpected token '}'"));
});

await test("script which returns a circular object", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const result = await unsafeEvalAsStringInRealm(agent, realm, () => {
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
