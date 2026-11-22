import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect, expectComplete } from "@isolated-vm/experimental";
import { makeDirectResolver, makeLinker, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";
import "@isolated-vm/experimental/test/fixtures";

await test("catastrophic error", async () => {
	const catastrophe = Promise.withResolvers<string>();
	await using agent = await Agent.create({ onCatastrophicError: catastrophe.resolve });
	const realm = expect(await agent.createRealm());
	const runtime = expect(await realm.instantiateRuntime());
	const module = expectComplete(await agent.compileModule(`
		import { triggerFatalForTesting } from "isolated-vm:runtime";
		globalThis.trigger = triggerFatalForTesting;
	`));
	await module.link(realm, makeLinker(makeDirectResolver(), makeStaticLoader({ "isolated-vm:runtime": runtime })));
	expectComplete(await module.evaluate(realm));
	const script = expectComplete(await agent.compileScript("globalThis.trigger();"));
	assert.strictEqual(await script.run(realm), null);
	assert.match(await catastrophe.promise, /Empty MaybeLocal/);
});
