import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expectComplete } from "@isolated-vm/experimental";
import "@isolated-vm/experimental/test/fixtures";

await test("dispose while running", async () => {
	const result = async function() {
		await using agent = await Agent.create();
		const realm = await agent.createRealm();
		const script = expectComplete(await agent.compileScript("for(;;);"));
		// eslint-disable-next-line @typescript-eslint/return-await
		return script.run(realm);
	}();
	assert.strictEqual(await result, null);
});

await test("busy dispose", async () => {
	await using agent = await Agent.create();
	const [ dispose, realm ] = await Promise.all([ agent.disposeAsync(), agent.createRealm() ]);
	assert.strictEqual(dispose, undefined);
	assert.strictEqual(realm, null);
});
