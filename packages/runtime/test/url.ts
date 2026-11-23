import type { Realm, Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/test/fixtures";
import { makeCompositeLinker, makeDirectResolver, makeFileSystemCompilationLoader, makeLinker, makeLocalResolver, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

type AnyFunction = (...args: unknown[]) => unknown;

/** Installs the URL runtime in a fresh realm and returns a reference to a function running `body` */
async function makeRuntimeFunction(agent: Agent, realm: Realm, body: string) {
	const runtime = makeStaticLoader({
		"isolated-vm:runtime": expect(await realm.instantiateRuntime()),
	});
	const linker = makeCompositeLinker(
		makeLinker(makeDirectResolver(), runtime),
		makeLinker(makeLocalResolver(import.meta), makeFileSystemCompilationLoader(agent)));
	const module = expectComplete(await agent.compileModule(`
		import "@isolated-vm/intrinsics/install";
		import "@isolated-vm/runtime/url/install";
		const attempt = fn => { try { return fn(); } catch (error) { return { error: error.constructor.name }; } };
		globalThis.fn = () => { ${body} };
	`));
	await module.link(realm, linker);
	expectComplete(await module.evaluate(realm));
	const global = await realm.acquireGlobalObject();
	return expect(await global.get("fn")) as Reference<AnyFunction>;
}

await test("url", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const url = new URL("https://user:pass@EXAMPLE.com:8080/a/../b?x=1#frag");
		url.hostname = "example.org";
		url.search = "y=\\u00e4";
		url.pathname = "c d";
		url.hash = "";
		return {
			href: url.href,
			origin: url.origin,
			components: [ url.protocol, url.username, url.password, url.host, url.hostname, url.port, url.pathname, url.search, url.hash ],
			json: JSON.stringify({ url }),
			tag: Object.prototype.toString.call(url),
			statics: [ URL.canParse("nope"), URL.parse("/path", "https://example.com").href ],
			invalid: attempt(() => new URL("nope")),
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		href: "https://user:pass@example.org:8080/c%20d?y=%C3%A4",
		origin: "https://example.org:8080",
		components: [ "https:", "user", "pass", "example.org:8080", "example.org", "8080", "/c%20d", "?y=%C3%A4", "" ],
		json: '{"url":"https://user:pass@example.org:8080/c%20d?y=%C3%A4"}',
		tag: "[object URL]",
		statics: [ false, "https://example.com/path" ],
		invalid: { error: "TypeError" },
	});
});
