import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import "@isolated-vm/experimental/test/fixtures";

const hostSupportsSharedArraySupport =
	// @ts-expect-error
	process.isBun === undefined && process.versions.deno === undefined;

// TODO: deno fails when this is in the `describe` (??)
await test("numeric references", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const global = await realm.acquireGlobalObject();
	// doubles are stored as references in `value_t`, because they are bigger than 32-bit. there was
	// a failure with more than one of them in the same transfer operation.
	expect(await global.set("xx", { zero: 0.1, one: 0.2 }));
});

await test("numeric indices", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const global = await realm.acquireGlobalObject();
	// numeric array indices broke as part of a double to int32_t change.
	expect(await global.set("xx", [ false, true ]));
});

if (hostSupportsSharedArraySupport) {
	await test("seen index type", async () => {
		await using agent = await Agent.create();
		const realm = expect(await agent.createRealm());
		const global = await realm.acquireGlobalObject();
		const payload = {
			number: 0,
			buffer: [ new Uint8Array(new SharedArrayBuffer(0)) ],
		};
		expect(await global.set("xx", [ payload ]));
	});
}
