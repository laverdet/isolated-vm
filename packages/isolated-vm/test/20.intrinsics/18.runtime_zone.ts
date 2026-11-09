import type { Realm } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/test/fixtures";
import { makeDirectResolver, makeLinker, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

async function evaluateWithZone(agent: Agent, realm: Realm, body: string) {
	const runtime = expect(await realm.instantiateRuntime());
	const module = expectComplete(await agent.compileModule(`
		import { Zone } from "isolated-vm:runtime";
		${body}
	`));
	await module.link(realm, makeLinker(makeDirectResolver(), makeStaticLoader({ "isolated-vm:runtime": runtime })));
	expectComplete(await module.evaluate(realm));
	const global = await realm.acquireGlobalObject();
	return expect(await global.get("result")).copy();
}

await test("zones nest and run returns the callback result", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const result = await evaluateWithZone(agent, realm, `
		const outer = new Zone();
		const inner = new Zone();
		globalThis.result = outer.run(() => [
			Zone.current === outer,
			inner.run(() => Zone.current === inner),
			Zone.current === outer,
		]);`);
	assert.deepStrictEqual(result, [ true, true, true ]);
});

await test("zone follows await and then", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const result = await evaluateWithZone(agent, realm, `
		const zone = new Zone();
		const results = {};
		zone.run(async () => {
			await null;
			results.awaited = Zone.current === zone;
			await Promise.resolve().then(() => { results.chained = Zone.current === zone; });
		});
		Promise.resolve().then(() => { results.outside = Zone.current; });
		globalThis.result = results;`);
	// The module's microtask checkpoint runs all of these before `evaluate` settles
	assert.deepStrictEqual(result, { awaited: true, chained: true, outside: undefined });
});
