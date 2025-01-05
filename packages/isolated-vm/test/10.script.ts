import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "isolated-vm";
import { unsafeIIFEAsString } from "./fixtures.js";

await test("script source origin", async () => {
	await using agent = await ivm.Agent.create();
	const script = await agent.compileScript(
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
	);
	const realm = await agent.createRealm();
	const result = await script.run(realm);
	assert.ok(typeof result === "string");
	assert.ok(result.includes("file:///test-script:103"));
});

await test("placeholder module test", async () => {
	await using agent = await ivm.Agent.create();
	const module = await agent.compileModule("import { foo } from 'bar'");
	assert.deepEqual(module.requests, [ { specifier: "bar" } ]);
});
