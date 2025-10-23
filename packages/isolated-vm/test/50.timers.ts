import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "isolated-vm";
import { makeCompositeLinker, makeFileSystemCompilationLinker, makePreloadedLinker } from "isolated-vm/utility/linker";
import { unwrapCompletion } from "./fixtures.js";

await test("setTimeout capability", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const runTimers = unwrapCompletion(await agent.compileScript("runTimers()"));
	const resolvers = Promise.withResolvers();
	const capabilities = makePreloadedLinker(Object.entries({
		"isolated-vm:capability/timers":
			await agent.createCapability((/*timeout*/) => {
				void async function() {
					await runTimers.run(realm);
				}();
			}, { origin: { name: "isolated-vm:capability/timers" } }),
		"notify-test":
			await agent.createCapability((message: unknown) => {
				resolvers.resolve(message);
			}, { origin: { name: "notify-test" } }),
	}));
	// eslint-disable-next-line @typescript-eslint/unbound-method
	const runtime = makeFileSystemCompilationLinker(agent, import.meta.resolve);
	const linker = makeCompositeLinker(capabilities, runtime);
	const module = await agent.compileModule(`
		import "isolated-vm/host/html/timers";
		import hello from "notify-test";
		setTimeout(hello, 0, "hello");
	`);
	await module.link(realm, linker);
	await module.evaluate(realm);
	setTimeout(resolvers.resolve, 100);
	assert.equal(await resolvers.promise, "hello");
});
