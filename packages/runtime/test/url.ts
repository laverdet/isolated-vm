import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

// The realm has the URL runtime installed
const options = {
	packages: import.meta,
	install: `
		import "@isolated-vm/intrinsics/install";
		import "@isolated-vm/runtime/url/install";`,
};

await test("url", async () => {
	await using vm = await makeTestAgent(options);
	await vm.unsafeScript(() => {
		const url = new URL("https://user:pass@EXAMPLE.com:8080/a/../b?x=1#frag");
		url.hostname = "example.org";
		url.search = "y=ä";
		url.pathname = "c d";
		url.hash = "";
		assert.equal(url.href, "https://user:pass@example.org:8080/c%20d?y=%C3%A4");
		assert.equal(url.origin, "https://example.org:8080");
		assert.deepEqual(
			[
				url.protocol, url.username, url.password, url.host, url.hostname,
				url.port, url.pathname, url.search, url.hash,
			],
			[ "https:", "user", "pass", "example.org:8080", "example.org", "8080", "/c%20d", "?y=%C3%A4", "" ]);
		assert.equal(JSON.stringify({ url }), '{"url":"https://user:pass@example.org:8080/c%20d?y=%C3%A4"}');
		assert.equal(Object.prototype.toString.call(url), "[object URL]");
		assert.equal(URL.canParse("nope"), false);
		assert.equal(URL.parse("/path", "https://example.com")?.href, "https://example.com/path");
		assert.throws(() => new URL("nope"), TypeError);
	});
});

await test("url: search params", async () => {
	await using vm = await makeTestAgent(options);
	await vm.unsafeScript(() => {
		const url = new URL("https://example.com/?a=1&b=%C3%A4+c#f");
		const params = url.searchParams;
		params.append("d", "e f");
		params.delete("a");
		url.search = "x=y";
		assert.equal(url.searchParams, params);
		assert.equal(String(new URLSearchParams("?a=1&a=2&b")), "a=1&a=2&b=");
		assert.equal(String(new URLSearchParams([ [ "a", "1" ], [ "b", "ä" ] ])), "a=1&b=%C3%A4");
		assert.equal(String(new URLSearchParams({ a: "1", b: "2" })), "a=1&b=2");
		assert.deepEqual([ ...new URLSearchParams("a=1&b=2") ], [ [ "a", "1" ], [ "b", "2" ] ]);
		assert.equal(url.href, "https://example.com/?x=y#f");
		assert.deepEqual([ ...params ], [ [ "x", "y" ] ]);
		assert.equal(Object.prototype.toString.call(params.entries()), "[object URLSearchParams Iterator]");
		assert.throws(() => new URLSearchParams([ [ "a" ] ]), TypeError);
	});
});
