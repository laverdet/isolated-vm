import type { Realm, Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/test/fixtures";
import { makeDirectResolver, makeLinker, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

type AnyFunction = (...args: unknown[]) => unknown;

/** Returns a reference to a function defined with the runtime `parseURL` & `updateURL` functions in scope */
async function makeRuntimeFunction(agent: Agent, realm: Realm, body: string) {
	const runtime = expect(await realm.instantiateRuntime());
	const module = expectComplete(await agent.compileModule(`
		import { parseURL, updateURL } from "isolated-vm:runtime";
		const attempt = fn => { try { return fn(); } catch (error) { return { error: error.constructor.name }; } };
		// Resolves the serialization back to the input when it was omitted
		const href = (input, record) => record === undefined ? undefined : record.href ?? input;
		globalThis.fn = () => { ${body} };
	`));
	await module.link(realm, makeLinker(makeDirectResolver(), makeStaticLoader({ "isolated-vm:runtime": runtime })));
	expectComplete(await module.evaluate(realm));
	const global = await realm.acquireGlobalObject();
	return expect(await global.get("fn")) as Reference<AnyFunction>;
}

await test("parseURL serializes", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const parse = (input, base) => href(input, parseURL(input, base));
		return {
			// The serialization is omitted when it is the input
			normalized: parseURL("https://example.com/a?b#c").href,
			changed: parse("HTTPS://EXAMPLE.com:443/a/../b"),
			relative: parse("../c?y=2", "https://example.com/a/b/"),
			absoluteWithBase: parse("https://other.example/", "https://example.com/"),
			// Non-ASCII one-byte and two-byte input is transcoded for Ada
			latin1: parse("https://ex\\u00e4mple.com/\\u00e4"),
			twoByte: parse("https://\\u4f8b\\u3048.jp/\\u4f8b"),
			// A two-byte string with ASCII content is still the serialization
			twoByteASCII: parseURL("\\u4f8bhttps://example.com/".slice(1)).href,
			invalid: [ parseURL("nope"), parseURL("https://exa mple.com"), parseURL("/path", "nope") ],
			// URLs are limited to 2 MiB
			long: [ parseURL("data:" + "a".repeat((2 << 20) - 5)) !== undefined, parseURL("data:" + "a".repeat((2 << 20) - 4)) ],
			notString: [ attempt(() => parseURL(Symbol())), attempt(() => parseURL(123)), attempt(() => parseURL("https://example.com", null)) ],
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		normalized: undefined,
		changed: "https://example.com/b",
		relative: "https://example.com/a/c?y=2",
		absoluteWithBase: "https://other.example/",
		latin1: "https://xn--exmple-cua.com/%C3%A4",
		twoByte: "https://xn--r8jz45g.jp/%E4%BE%8B",
		twoByteASCII: undefined,
		invalid: [ undefined, undefined, undefined ],
		long: [ true, undefined ],
		notString: Array.from({ length: 3 }, () => ({ error: "TypeError" })),
	});
});

await test("parseURL records components", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		return [
			parseURL("https://user:pass@example.com:8080/path?query#hash"),
			parseURL("https://example.com"),
			parseURL("file:///tmp/x"),
			parseURL("mailto:someone@example.com"),
		];`);
	assert.deepEqual(expectComplete(await fn.invoke([])), [
		{ href: undefined, protocolEnd: 6, usernameEnd: 12, hostStart: 17, hostEnd: 29, port: 8080, pathnameStart: 34, searchStart: 39, hashStart: 45 },
		// A missing query or fragment starts where the next component does, and a missing port is `undefined`
		{ href: "https://example.com/", protocolEnd: 6, usernameEnd: 8, hostStart: 8, hostEnd: 19, port: undefined, pathnameStart: 19, searchStart: 20, hashStart: 20 },
		{ href: undefined, protocolEnd: 5, usernameEnd: 7, hostStart: 7, hostEnd: 7, port: undefined, pathnameStart: 7, searchStart: 13, hashStart: 13 },
		{ href: undefined, protocolEnd: 7, usernameEnd: 7, hostStart: 7, hostEnd: 7, port: undefined, pathnameStart: 7, searchStart: 26, hashStart: 26 },
	]);
});

await test("updateURL applies setters", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const input = "https://user:pass@example.com:8080/path?query#hash";
		const [ protocol, username, password, host, hostname, port, pathname ] = [ 0, 1, 2, 3, 4, 5, 6 ];
		const update = (component, value) => href(input, updateURL(input, component, value));
		return {
			protocol: update(protocol, "http"),
			username: update(username, "u"),
			password: update(password, ""),
			host: update(host, "other.example:9090"),
			hostname: update(hostname, "other.example"),
			port: update(port, "443"),
			pathname: update(pathname, "a b"),
			// A rejected value is reported as such
			rejected: [ updateURL(input, port, "nope"), updateURL(input, hostname, ""), updateURL(input, protocol, "x y") ],
			// The components follow the change
			components: (() => {
				const record = updateURL(input, pathname, "/longer/path");
				return [ record.pathnameStart, record.searchStart, record.hashStart ];
			})(),
			unparsable: [ updateURL("nope", pathname, "x"), updateURL(input, 7, "x") ],
			invalid: [ attempt(() => updateURL(input, pathname)), attempt(() => updateURL(input, "pathname", "x")) ],
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		protocol: "http://user:pass@example.com:8080/path?query#hash",
		username: "https://u:pass@example.com:8080/path?query#hash",
		password: "https://user@example.com:8080/path?query#hash",
		host: "https://user:pass@other.example:9090/path?query#hash",
		hostname: "https://user:pass@other.example:8080/path?query#hash",
		port: "https://user:pass@example.com/path?query#hash",
		pathname: "https://user:pass@example.com:8080/a%20b?query#hash",
		rejected: [ undefined, undefined, undefined ],
		components: [ 34, 46, 52 ],
		unparsable: [ undefined, undefined ],
		invalid: Array.from({ length: 2 }, () => ({ error: "TypeError" })),
	});
});
