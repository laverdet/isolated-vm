import type { Realm, Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/test/fixtures";
import { makeDirectResolver, makeLinker, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

type AnyFunction = (...args: unknown[]) => unknown;

/** Returns a reference to a function defined with the runtime `StringReader` & `StringWriter` classes in scope */
async function makeRuntimeFunction(agent: Agent, realm: Realm, body: string) {
	const runtime = expect(await realm.instantiateRuntime());
	const module = expectComplete(await agent.compileModule(`
		import { StringReader, StringWriter } from "isolated-vm:runtime";
		const bytes = array => Uint8Array.from(array);
		const codes = string => string === undefined ? undefined : Array.from({ length: string.length }, (_, ii) => string.charCodeAt(ii));
		const attempt = fn => { try { return fn(); } catch (error) { return { error: error.constructor.name }; } };
		globalThis.fn = () => { ${body} };
	`));
	await module.link(realm, makeLinker(makeDirectResolver(), makeStaticLoader({ "isolated-vm:runtime": runtime })));
	expectComplete(await module.evaluate(realm));
	const global = await realm.acquireGlobalObject();
	return expect(await global.get("fn")) as Reference<AnyFunction>;
}

await test("string reader resolves labels", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		return {
			normalized: [ "UTF-8", " utf8\\t", "latin1", "unicode", "x-user-defined", "cseucpkdfmtjapanese" ].map(label => new StringReader(label, false, false).encoding),
			invalid: [ "", "replacement", "utf-8\\u00a0", "utf-8\\u2028", "cseucpkdfmtjapanesex", "bogus" ].map(label => attempt(() => new StringReader(label, false, false))),
			notString: attempt(() => new StringReader(Symbol(), false, false)),
			withoutNew: attempt(() => StringReader("utf-8", false, false)),
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		normalized: [ "utf-8", "utf-8", "windows-1252", "utf-16le", "x-user-defined", "euc-jp" ],
		invalid: Array.from({ length: 6 }, () => ({ error: "RangeError" })),
		notString: { error: "TypeError" },
		withoutNew: { error: "TypeError" },
	});
});

await test("string reader decodes", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const read = (label, array, fatal = false, ignoreBOM = false) => codes(new StringReader(label, fatal, ignoreBOM).read(bytes(array), true));
		return {
			utf8: read("utf-8", [ 0x68, 0xc3, 0xa9, 0xf0, 0x9f, 0x98, 0x80 ]),
			utf8Invalid: read("utf-8", [ 0xe0, 0x80, 0x41, 0xf0, 0x9f, 0x98 ]),
			utf8Fatal: read("utf-8", [ 0xff ], true),
			utf8FatalTruncated: read("utf-8", [ 0xf0, 0x9f ], true),
			bom: read("utf-8", [ 0xef, 0xbb, 0xbf, 0x41 ]),
			bomKept: read("utf-8", [ 0xef, 0xbb, 0xbf, 0x41 ], false, true),
			bomOnly: read("utf-8", [ 0xef, 0xbb, 0xbf ]),
			legacyBOM: read("windows-1252", [ 0xef, 0xbb, 0xbf ]),
			utf16: read("utf-16le", [ 0xff, 0xfe, 0x41, 0x00, 0x42 ]),
			utf16Fatal: read("utf-16le", [ 0x41 ], true),
			windows1252: read("windows-1252", [ 0x41, 0x80, 0xff ]),
			shiftJis: read("shift_jis", [ 0x82, 0xa0, 0xa0 ]),
			xUserDefined: read("x-user-defined", [ 0x41, 0x80, 0xff ]),
			empty: read("utf-8", []),
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		utf8: [ 0x68, 0xe9, 0xd83d, 0xde00 ],
		utf8Invalid: [ 0xfffd, 0xfffd, 0x41, 0xfffd ],
		utf8Fatal: undefined,
		utf8FatalTruncated: undefined,
		bom: [ 0x41 ],
		bomKept: [ 0xfeff, 0x41 ],
		bomOnly: [],
		legacyBOM: [ 0xef, 0xbb, 0xbf ],
		utf16: [ 0x41, 0xfffd ],
		utf16Fatal: undefined,
		windows1252: [ 0x41, 0x20ac, 0xff ],
		shiftJis: [ 0x3042, 0xfffd ],
		xUserDefined: [ 0x41, 0xf780, 0xf7ff ],
		empty: [],
	});
});

await test("string reader streams", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const stream = (label, chunks, fatal = false) => {
			const reader = new StringReader(label, fatal, false);
			return chunks.map((chunk, ii) => codes(reader.read(bytes(chunk), ii === chunks.length - 1)));
		};
		return {
			split: stream("utf-8", [ [ 0xf0, 0x9f ], [ 0x98, 0x80, 0xc3 ], [ 0xa9 ], [] ]),
			splitBOM: stream("utf-8", [ [ 0xef ], [ 0xbb ], [ 0xbf, 0x41 ], [] ]),
			truncated: stream("utf-8", [ [ 0x41, 0xf0, 0x9f ], [] ]),
			fatalTruncated: stream("utf-8", [ [ 0xf0, 0x9f ], [] ], true),
			legacy: stream("shift_jis", [ [ 0x82 ], [ 0xa0 ] ]),
			utf16: stream("utf-16le", [ [ 0xff ], [ 0xfe, 0x41 ], [ 0x00 ] ]),
			reset: (() => {
				const reader = new StringReader("utf-8", false, false);
				reader.read(bytes([ 0xf0 ]), false);
				// A flush ends the stream, so the next read is fresh and strips a BOM again
				return [ codes(reader.read(bytes([ 0x41 ]), true)), codes(reader.read(bytes([ 0xef, 0xbb, 0xbf, 0x42 ]), true)) ];
			})(),
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		split: [ [], [ 0xd83d, 0xde00 ], [ 0xe9 ], [] ],
		splitBOM: [ [], [], [ 0x41 ], [] ],
		truncated: [ [ 0x41 ], [ 0xfffd ] ],
		fatalTruncated: [ [], undefined ],
		legacy: [ [], [ 0x3042 ] ],
		utf16: [ [], [], [ 0x41 ] ],
		reset: [ [ 0xfffd, 0x41 ], [ 0x42 ] ],
	});
});

await test("string reader accepts buffer sources", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const read = view => new StringReader("utf-8", false, false).read(view, true);
		return {
			views: [
				bytes([ 0x41, 0x42, 0x43 ]).buffer,
				new Uint8Array(bytes([ 0x41, 0x42, 0x43 ]).buffer, 1, 1),
				new DataView(bytes([ 0x41, 0x42, 0x43 ]).buffer, 1, 1),
				new Uint16Array([ 0x4241 ]),
				new Uint8Array(new SharedArrayBuffer(1)).fill(0x41),
				new SharedArrayBuffer(0),
			].map(read),
			invalid: [ "abc", [ 0x41 ], {}, null, undefined ].map(view => attempt(() => read(view))),
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		views: [ "ABC", "B", "B", "AB", "A", "" ],
		invalid: Array.from({ length: 5 }, () => ({ error: "TypeError" })),
	});
});

await test("string reader decodes large input", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const length = 1 << 20;
		const legacy = new StringReader("windows-1252", false, false).read(new Uint8Array(length).fill(0x80), true);
		const utf16 = new Uint8Array(length * 2 + 2);
		utf16.set([ 0xff, 0xfe ]);
		for (let ii = 2; ii < utf16.length; ii += 2) {
			utf16[ii] = 0x41;
		}
		const withBOM = new StringReader("utf-16le", false, false).read(utf16, true);
		return [ legacy.length, legacy.charCodeAt(0), legacy.charCodeAt(length - 1), withBOM.length, withBOM.charCodeAt(0), withBOM.charCodeAt(length - 1) ];`);
	assert.deepEqual(expectComplete(await fn.invoke([])), [ 1 << 20, 0x20ac, 0x20ac, 1 << 20, 0x41, 0x41 ]);
});

await test("string writer encodes", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const writer = new StringWriter();
		const encode = string => {
			const { read, written, destination } = writer.write(string, true);
			return [ read, written, destination.constructor.name, Array.from(destination) ];
		};
		return {
			empty: encode(""),
			ascii: encode("hi"),
			multibyte: encode("h\\u00e9\\u{1f600}"),
			loneSurrogates: encode("\\ud800\\udc00\\ud800x\\udc00"),
			notString: attempt(() => writer.write(123, true)),
			destinationType: attempt(() => writer.write("abc", true, new Uint16Array(4))),
			withoutNew: attempt(() => StringWriter()),
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		empty: [ 0, 0, "Uint8Array", [] ],
		ascii: [ 2, 2, "Uint8Array", [ 0x68, 0x69 ] ],
		multibyte: [ 4, 7, "Uint8Array", [ 0x68, 0xc3, 0xa9, 0xf0, 0x9f, 0x98, 0x80 ] ],
		loneSurrogates: [ 5, 11, "Uint8Array", [ 0xf0, 0x90, 0x80, 0x80, 0xef, 0xbf, 0xbd, 0x78, 0xef, 0xbf, 0xbd ] ],
		notString: { error: "TypeError" },
		destinationType: { error: "TypeError" },
		withoutNew: { error: "TypeError" },
	});
});

await test("string writer encodes into a destination", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const writer = new StringWriter();
		const into = (string, length) => {
			const buffer = new Uint8Array(length);
			const { read, written, destination } = writer.write(string, true, buffer);
			return [ read, written, destination === buffer, Array.from(buffer) ];
		};
		return {
			exact: into("h\\u00e9", 3),
			short: into("\\u00e9\\u00e9", 3),
			pair: into("a\\u{1f600}", 4),
			empty: into("abc", 0),
			offset: (() => {
				const buffer = new Uint8Array(4);
				const { read, written } = writer.write("abc", true, new Uint8Array(buffer.buffer, 1, 2));
				return [ read, written, Array.from(buffer) ];
			})(),
			detached: (() => {
				const buffer = new Uint8Array(4);
				buffer.buffer.transfer();
				const { read, written } = writer.write("abc", true, buffer);
				return [ read, written ];
			})(),
		};`);
	assert.deepEqual(expectComplete(await fn.invoke([])), {
		exact: [ 2, 3, true, [ 0x68, 0xc3, 0xa9 ] ],
		short: [ 1, 2, true, [ 0xc3, 0xa9, 0 ] ],
		pair: [ 1, 1, true, [ 0x61, 0, 0, 0 ] ],
		empty: [ 0, 0, true, [] ],
		offset: [ 2, 2, [ 0, 0x61, 0x62, 0 ] ],
		detached: [ 0, 0 ],
	});
});

await test("string writer holds a trailing lead surrogate", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const fn = await makeRuntimeFunction(agent, realm, `
		const writer = new StringWriter();
		const write = (string, flush, destination) => {
			const result = writer.write(string, flush, destination);
			return [ result.read, result.written, Array.from(result.destination) ];
		};
		return [
			// Held
			write("a\\ud83d", false),
			// Completed by the next write; the held unit is not counted as read
			write("\\ude00", false),
			write("\\ud83d", false),
			// Replaced when the next write doesn't complete it
			write("x", false),
			write("\\ud83d", false),
			// Replaced by a flush
			write("", true),
			// Not held when flushing
			write("\\ud83d", true),
			// Held even when there is room in the destination
			write("a\\ud83d", false, new Uint8Array(4)),
			write("\\ude00", true, new Uint8Array(4)),
		];`);
	assert.deepEqual(expectComplete(await fn.invoke([])), [
		[ 1, 1, [ 0x61 ] ],
		[ 1, 4, [ 0xf0, 0x9f, 0x98, 0x80 ] ],
		[ 0, 0, [] ],
		[ 1, 4, [ 0xef, 0xbf, 0xbd, 0x78 ] ],
		[ 0, 0, [] ],
		[ 0, 3, [ 0xef, 0xbf, 0xbd ] ],
		[ 1, 3, [ 0xef, 0xbf, 0xbd ] ],
		[ 1, 1, [ 0x61, 0, 0, 0 ] ],
		[ 1, 4, [ 0xf0, 0x9f, 0x98, 0x80 ] ],
	]);
});
