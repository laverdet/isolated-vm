import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("setTimeout capability", async () => {
	await using vm = await makeTestAgent({
		packages: import.meta,
		install: `
			import { initializeTimers } from "@isolated-vm/runtime/html/install/timers";
			initializeTimers();`,
	});
	await vm.unsafeModule(async () => {
		const value = await new Promise(resolve => { setTimeout(resolve, 0, "hello"); });
		assert.equal(value, "hello");
	});
});
