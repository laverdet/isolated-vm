import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "@isolated-vm/experimental";
import {} from "@isolated-vm/experimental/test/fixtures";

await test("dispose while running", async () => {
	const result = async function() {
		await using agent = await ivm.Agent.create();
		const realm = await agent.createRealm();
		const script = ivm.expectComplete(await agent.compileScript("for(;;);"));
		// eslint-disable-next-line @typescript-eslint/return-await
		return script.run(realm);
	}();
	assert.strictEqual(await result, undefined);
});
