import * as assert from "node:assert/strict";
import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("realm global reference", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		// @ts-expect-error
		globalThis.test = 123;
	});
	assert.equal(await vm.copy("test"), 123);
});
