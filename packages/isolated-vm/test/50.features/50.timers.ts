import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/test/fixtures";
import { makeCompositeLinker, makeDirectResolver, makeFileSystemCompilationLoader, makeLinker, makeLocalResolver, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

await test("setTimeout capability", async () => {
	await using agent = await Agent.create();
	const realm = await agent.createRealm();
	const runTimers = expectComplete(await agent.compileScript("runTimers()"));
	const resolvers = Promise.withResolvers();
	const capabilities = makeStaticLoader({
		"isolated-vm:capability/timers": await realm.createCapability(
			() => ({
				default: (/*timeout*/) => {
					void async function() {
						await runTimers.run(realm);
					}();
				},
			}),
			{ origin: "isolated-vm:capability/timers" }),
		"notify-test": await realm.createCapability(
			() => ({
				default: (message: unknown) => { resolvers.resolve(message); },
			}),
			{ origin: "notify-test" }),
	});

	const runtime = makeFileSystemCompilationLoader(agent);
	const linker = makeCompositeLinker(
		makeLinker(makeDirectResolver(), capabilities),
		makeLinker(makeLocalResolver(import.meta), runtime));
	const module = expectComplete(await agent.compileModule(`
		import "@isolated-vm/experimental/host/html/timers";
		import hello from "notify-test";
		setTimeout(hello, 0, "hello");
	`));
	await module.link(realm, linker);
	await module.evaluate(realm);
	setTimeout(resolvers.resolve, 100);
	assert.equal(await resolvers.promise, "hello");
});
