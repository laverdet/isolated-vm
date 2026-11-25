import type { URLComponent } from "@isolated-vm/experimental/runtime";
import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("parseURL serializes", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		// Resolves the serialization back to the input when it was omitted
		const parse = (input: string, base?: string) => {
			const record = rt.parseURL(input, base);
			return record === undefined ? undefined : record.href ?? input;
		};
		// The serialization is omitted when it is the input
		const normalized = rt.parseURL("https://example.com/a?b#c");
		assert.notEqual(normalized, undefined);
		assert.equal(normalized?.href, undefined);
		assert.equal(parse("HTTPS://EXAMPLE.com:443/a/../b"), "https://example.com/b");
		assert.equal(parse("../c?y=2", "https://example.com/a/b/"), "https://example.com/a/c?y=2");
		assert.equal(parse("https://other.example/", "https://example.com/"), "https://other.example/");
		// Non-ASCII one-byte and two-byte input is transcoded for Ada
		assert.equal(parse("https://exämple.com/ä"), "https://xn--exmple-cua.com/%C3%A4");
		assert.equal(parse("https://例え.jp/例"), "https://xn--r8jz45g.jp/%E4%BE%8B");
		// A two-byte string with ASCII content is still the serialization
		assert.equal(rt.parseURL("例https://example.com/".slice(1))?.href, undefined);
		assert.deepEqual(
			[ rt.parseURL("nope"), rt.parseURL("https://exa mple.com"), rt.parseURL("/path", "nope") ],
			[ undefined, undefined, undefined ]);
		// URLs are limited to 2 MiB
		assert.notEqual(rt.parseURL("data:" + "a".repeat((2 << 20) - 5)), undefined);
		assert.equal(rt.parseURL("data:" + "a".repeat((2 << 20) - 4)), undefined);
		// @ts-expect-error
		assert.throws(() => rt.parseURL(Symbol("input")), TypeError);
		// @ts-expect-error
		assert.throws(() => rt.parseURL(123), TypeError);
		// @ts-expect-error
		assert.throws(() => rt.parseURL("https://example.com", null), TypeError);
	});
});

await test("parseURL records components", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		assert.deepEqual(rt.parseURL("https://user:pass@example.com:8080/path?query#hash"), {
			href: undefined,
			protocolEnd: 6,
			usernameEnd: 12,
			hostStart: 17,
			hostEnd: 29,
			port: 8080,
			pathnameStart: 34,
			searchStart: 39,
			hashStart: 45,
		});
		// A missing query or fragment starts where the next component does, and a missing port is `undefined`
		assert.deepEqual(rt.parseURL("https://example.com"), {
			href: "https://example.com/",
			protocolEnd: 6,
			usernameEnd: 8,
			hostStart: 8,
			hostEnd: 19,
			port: undefined,
			pathnameStart: 19,
			searchStart: 20,
			hashStart: 20,
		});
		assert.deepEqual(rt.parseURL("file:///tmp/x"), {
			href: undefined,
			protocolEnd: 5,
			usernameEnd: 7,
			hostStart: 7,
			hostEnd: 7,
			port: undefined,
			pathnameStart: 7,
			searchStart: 13,
			hashStart: 13,
		});
		assert.deepEqual(rt.parseURL("mailto:someone@example.com"), {
			href: undefined,
			protocolEnd: 7,
			usernameEnd: 7,
			hostStart: 7,
			hostEnd: 7,
			port: undefined,
			pathnameStart: 7,
			searchStart: 26,
			hashStart: 26,
		});
	});
});

await test("updateURL applies setters", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		const input = "https://user:pass@example.com:8080/path?query#hash";
		const [ protocol, username, password, host, hostname, port, pathname ] = [ 0, 1, 2, 3, 4, 5, 6 ] as const;
		// Resolves the serialization back to the input when it was omitted
		const update = (component: URLComponent, value: string) => {
			const record = rt.updateURL(input, component, value);
			return record === undefined ? undefined : record.href ?? input;
		};
		assert.equal(update(protocol, "http"), "http://user:pass@example.com:8080/path?query#hash");
		assert.equal(update(username, "u"), "https://u:pass@example.com:8080/path?query#hash");
		assert.equal(update(password, ""), "https://user@example.com:8080/path?query#hash");
		assert.equal(update(host, "other.example:9090"), "https://user:pass@other.example:9090/path?query#hash");
		assert.equal(update(hostname, "other.example"), "https://user:pass@other.example:8080/path?query#hash");
		assert.equal(update(port, "443"), "https://user:pass@example.com/path?query#hash");
		assert.equal(update(pathname, "a b"), "https://user:pass@example.com:8080/a%20b?query#hash");
		// A rejected value is reported as such
		assert.deepEqual([
			rt.updateURL(input, port, "nope"),
			rt.updateURL(input, hostname, ""),
			rt.updateURL(input, protocol, "x y"),
		], [ undefined, undefined, undefined ]);
		// The components follow the change
		const record = rt.updateURL(input, pathname, "/longer/path");
		assert.deepEqual([ record?.pathnameStart, record?.searchStart, record?.hashStart ], [ 34, 46, 52 ]);
		assert.equal(rt.updateURL("nope", pathname, "x"), undefined);
		// @ts-expect-error
		assert.equal(rt.updateURL(input, 7, "x"), undefined);
		// @ts-expect-error
		assert.throws(() => rt.updateURL(input, pathname), TypeError);
		// @ts-expect-error
		assert.throws(() => rt.updateURL(input, "pathname", "x"), TypeError);
	});
});
