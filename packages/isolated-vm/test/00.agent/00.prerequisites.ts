import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expectComplete } from "@isolated-vm/experimental";
import "@isolated-vm/experimental/test/fixtures";

// Minimal support, without which the rest of the suites don't really make any sense.
await test("prerequisites", async () => {
	await using agent = await Agent.create();
	const realm = await agent.createRealm();
	const script = expectComplete(await agent.compileScript('"hello world";'));
	assert.strictEqual(expectComplete(await script.run(realm)), "hello world");
});
