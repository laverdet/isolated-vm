import type { Realm, Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, Capability, expect, expectComplete } from "@isolated-vm/experimental";
import { makeDirectResolver, makeLinker, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";
import "@isolated-vm/experimental/test/fixtures";

type Resolvers = PromiseWithResolvers<unknown>;

const busy = "const busy = ms => { const end = Date.now() + ms; while (Date.now() < end); };";

/** Links the runtime and evaluates a module body */
async function evaluateWithZone(agent: Agent, realm: Realm, body: string) {
	const runtime = expect(await realm.instantiateRuntime());
	const module = expectComplete(await agent.compileModule(`
		import { Zone } from "isolated-vm:runtime";
		${busy}
		${body}
	`));
	await module.link(realm, makeLinker(makeDirectResolver(), makeStaticLoader({ "isolated-vm:runtime": runtime })));
	return module.evaluate(realm);
}

/** Creates `globalThis.resolvers` and returns a host reference to its `resolve` function */
async function makeResolvers(agent: Agent, realm: Realm) {
	const script = expectComplete(await agent.compileScript("globalThis.resolvers = Promise.withResolvers(); undefined;"));
	expectComplete(await script.run(realm));
	const global = await realm.acquireGlobalObject();
	const resolvers = expect(await global.get("resolvers")) as Reference<Resolvers>;
	return expect(await resolvers.get("resolve")) as Reference<() => void>;
}

/** Reads `globalThis.result` */
async function readResult(realm: Realm) {
	const global = await realm.acquireGlobalObject();
	return expect(await global.get("result")).copy();
}

await test("promise reaction inherits the timeout of the task which enqueued it", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const resolve = await makeResolvers(agent, realm);
	const script = expectComplete(await agent.compileScript("resolvers.promise.then(() => { for(;;); }); undefined;"));
	expectComplete(await script.run(realm, { timeout: 50 }));
	// The host resumes the isolate without a timeout, but the reaction still runs under the budget
	// of the script which enqueued it. The termination happens inside the microtask checkpoint, after
	// `resolve()` itself returned, so the host task sees a normal completion.
	const start = performance.now();
	expectComplete(await resolve.invoke([]));
	assert.ok(performance.now() - start < 1000);
	// The realm is still usable afterwards
	const ok = expectComplete(await agent.compileScript("'ok'"));
	assert.strictEqual(expectComplete(await ok.run(realm)), "ok");
});

await test("zone budget terminates and is shared by clones", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	// A zone created by the runtime applies its own budget
	assert.strictEqual(await evaluateWithZone(agent, realm, "new Zone({ timeout: 20 }).run(() => { for(;;); });"), null);
	// A clone spends the same budget, which is paused while neither zone is current
	expectComplete(await evaluateWithZone(agent, realm, `
		const zone = new Zone({ timeout: 1000 });
		const clone = zone.clone();
		clone.run(() => { busy(50); });
		globalThis.result = [ zone.timeout < 960, clone.timeout < 960, new Zone().timeout ];`));
	assert.deepStrictEqual(await readResult(realm), [ true, true, undefined ]);
});

await test("budget can be replaced or adjusted while live", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	// Extending the budget from inside the zone outlives the original deadline
	expectComplete(await evaluateWithZone(agent, realm, `
		const zone = new Zone({ timeout: 20 });
		zone.run(() => { Zone.current.applyTimeout(1000); busy(50); });
		const adjusted = new Zone({ timeout: 20 });
		adjusted.run(() => { Zone.current.adjustTimeout(1000); busy(50); });
		globalThis.result = [ zone.timeout > 900, adjusted.timeout > 900, new Zone().applyTimeout(1) ];`));
	assert.deepStrictEqual(await readResult(realm), [ true, true, false ]);
	// Adjusting a paused budget below zero expires it, which terminates the next run
	assert.strictEqual(await evaluateWithZone(agent, realm, `
		const zone = new Zone({ timeout: 1000 });
		globalThis.result = [ zone.adjustTimeout(-2000), zone.timeout, zone.applyTimeout(1000) ];
		zone.run(() => { for(;;); });`), null);
	assert.deepStrictEqual(await readResult(realm), [ true, 0, false ]);
});

await test("timeout paused during sync capability inside a reaction", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const wait = Capability.sync(() => new Promise<void>(resolve => { setTimeout(resolve, 100); }));
	const capability = expect(await realm.createCapability({ default: wait }, { origin: "wait" }));
	const module = expectComplete(await agent.compileModule('import wait from "wait"; globalThis.wait = wait;'));
	await module.link(realm, () => capability);
	expectComplete(await module.evaluate(realm));
	const resolve = await makeResolvers(agent, realm);
	const script = expectComplete(await agent.compileScript("resolvers.promise.then(() => { wait(); globalThis.result = true; }); undefined;"));
	expectComplete(await script.run(realm, { timeout: 50 }));
	expectComplete(await resolve.invoke([]));
	assert.strictEqual(await readResult(realm), true);
});
