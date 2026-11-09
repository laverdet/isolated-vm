import type { Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, Capability, expect } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/test/fixtures";
import { makeCompositeLinker, makeDirectResolver, makeFileSystemCompilationLoader, makeLinker, makeLocalResolver, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

await test("setTimeout capability", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const resolvers = Promise.withResolvers();
	let wake: Reference<() => void> | undefined;
	const capabilities = makeStaticLoader({
		"isolated-vm:runtime": expect(await realm.instantiateRuntime()),
		"isolated-vm:capability/timers": expect(await realm.createCapability(
			{
				initialize: Capability.void((reference: unknown) => {
					wake = reference as Reference<() => void>;
				}),
				schedule: Capability.void((timeout: unknown) => {
					setTimeout(() => void wake?.invoke([]), Math.max(0, Number(timeout) - Date.now()));
				}),
			},
			{ origin: "isolated-vm:capability/timers" })),
		"notify-test": expect(await realm.createCapability(
			{
				default: Capability.void((message: unknown) => { resolvers.resolve(message); }),
			},
			{ origin: "notify-test" })),
	});

	const runtime = makeFileSystemCompilationLoader(agent);
	const linker = makeCompositeLinker(
		makeLinker(makeDirectResolver(), capabilities),
		makeLinker(makeLocalResolver(import.meta), runtime));
	const module = expectComplete(await agent.compileModule(`
		import { initializeTimers } from "@isolated-vm/experimental/host/html/install/timers";
		import hello from "notify-test";
		initializeTimers();
		setTimeout(hello, 0, "hello");
	`));
	await module.link(realm, linker);
	await module.evaluate(realm);
	setTimeout(resolvers.resolve, 100);
	assert.equal(await resolvers.promise, "hello");
});
