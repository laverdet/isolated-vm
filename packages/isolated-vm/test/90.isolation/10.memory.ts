import assert from "node:assert";
import { test } from "node:test";
import { Agent, expectComplete } from "@isolated-vm/experimental";
import "@isolated-vm/experimental/test/fixtures";

await test("memory limit", async () => {
	await using agent = await Agent.create({ memoryLimitBytes: 128 << 20 });
	const realm = await agent.createRealm();
	const sourceText =
`const x = [];
for (;;) {
	let a = 'a';
	for(; a.length < 2**28;) a += a;
	x.push(a);
}`;
	const script = expectComplete(await agent.compileScript(sourceText));
	const result = await script.run(realm);
	assert.strictEqual(result, null);
});
