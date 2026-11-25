import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Capability } from "@isolated-vm/experimental";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("catastrophic error", async () => {
	const catastrophe = Promise.withResolvers<string>();
	// A capability holds a reference which keeps the host's event loop alive, so the fatal must release it
	await using vm = await makeTestAgent({
		onCatastrophicError: catastrophe.resolve,
		capabilities: { capability: { default: Capability.void(() => {}) } },
		install: 'import "test:capability";',
	});
	assert.strictEqual(await vm.unsafeScript(() => rt.triggerFatalForTesting()), null);
	assert.match(await catastrophe.promise, /Empty MaybeLocal/);
});
