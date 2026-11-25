import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent } from "@isolated-vm/experimental";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("dispose while running", async () => {
	const result = async function() {
		await using vm = await makeTestAgent();
		// eslint-disable-next-line @typescript-eslint/return-await
		return vm.unsafeScript(() => { for (;;); });
	}();
	assert.strictEqual(await result, null);
});

await test("busy dispose", async () => {
	await using agent = await Agent.create();
	const [ dispose, realm ] = await Promise.all([ agent.disposeAsync(), agent.createRealm() ]);
	assert.strictEqual(dispose, undefined);
	assert.strictEqual(realm, null);
});
