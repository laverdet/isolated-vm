import type { AllowSharedBufferSource } from "@isolated-vm/experimental/runtime";
import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("string reader resolves labels", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		const labels = [ "UTF-8", " utf8\t", "latin1", "unicode", "x-user-defined", "cseucpkdfmtjapanese" ];
		assert.deepEqual(
			labels.map(label => new rt.StringReader(label, false, false).encoding),
			[ "utf-8", "utf-8", "windows-1252", "utf-16le", "x-user-defined", "euc-jp" ]);
		for (const label of [ "", "replacement", "utf-8\u{a0}", "utf-8\u{2028}", "cseucpkdfmtjapanesex", "bogus" ]) {
			assert.throws(() => new rt.StringReader(label, false, false), RangeError);
		}
		// @ts-expect-error
		assert.throws(() => new rt.StringReader(Symbol("label"), false, false), TypeError);
		// @ts-expect-error
		assert.throws(() => rt.StringReader("utf-8", false, false), TypeError);
	});
});

await test("string reader decodes", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		const read = (label: string, array: number[], fatal = false, ignoreBOM = false) => {
			const string = new rt.StringReader(label, fatal, ignoreBOM).read(Uint8Array.from(array), true);
			return string === undefined
				? undefined
				: Array.from({ length: string.length }, (_, ii) => string.charCodeAt(ii));
		};
		assert.deepEqual(read("utf-8", [ 0x68, 0xc3, 0xa9, 0xf0, 0x9f, 0x98, 0x80 ]), [ 0x68, 0xe9, 0xd83d, 0xde00 ]);
		assert.deepEqual(read("utf-8", [ 0xe0, 0x80, 0x41, 0xf0, 0x9f, 0x98 ]), [ 0xfffd, 0xfffd, 0x41, 0xfffd ]);
		assert.equal(read("utf-8", [ 0xff ], true), undefined);
		assert.equal(read("utf-8", [ 0xf0, 0x9f ], true), undefined);
		assert.deepEqual(read("utf-8", [ 0xef, 0xbb, 0xbf, 0x41 ]), [ 0x41 ]);
		assert.deepEqual(read("utf-8", [ 0xef, 0xbb, 0xbf, 0x41 ], false, true), [ 0xfeff, 0x41 ]);
		assert.deepEqual(read("utf-8", [ 0xef, 0xbb, 0xbf ]), []);
		assert.deepEqual(read("windows-1252", [ 0xef, 0xbb, 0xbf ]), [ 0xef, 0xbb, 0xbf ]);
		assert.deepEqual(read("utf-16le", [ 0xff, 0xfe, 0x41, 0x00, 0x42 ]), [ 0x41, 0xfffd ]);
		assert.equal(read("utf-16le", [ 0x41 ], true), undefined);
		assert.deepEqual(read("windows-1252", [ 0x41, 0x80, 0xff ]), [ 0x41, 0x20ac, 0xff ]);
		assert.deepEqual(read("shift_jis", [ 0x82, 0xa0, 0xa0 ]), [ 0x3042, 0xfffd ]);
		assert.deepEqual(read("x-user-defined", [ 0x41, 0x80, 0xff ]), [ 0x41, 0xf780, 0xf7ff ]);
		assert.deepEqual(read("utf-8", []), []);
	});
});

await test("string reader streams", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		const codes = (string: string | undefined) =>
			string === undefined ? undefined : Array.from({ length: string.length }, (_, ii) => string.charCodeAt(ii));
		const stream = (label: string, chunks: number[][], fatal = false) => {
			const reader = new rt.StringReader(label, fatal, false);
			return chunks.map((chunk, ii) => codes(reader.read(Uint8Array.from(chunk), ii === chunks.length - 1)));
		};
		assert.deepEqual(
			stream("utf-8", [ [ 0xf0, 0x9f ], [ 0x98, 0x80, 0xc3 ], [ 0xa9 ], [] ]),
			[ [], [ 0xd83d, 0xde00 ], [ 0xe9 ], [] ]);
		assert.deepEqual(stream("utf-8", [ [ 0xef ], [ 0xbb ], [ 0xbf, 0x41 ], [] ]), [ [], [], [ 0x41 ], [] ]);
		assert.deepEqual(stream("utf-8", [ [ 0x41, 0xf0, 0x9f ], [] ]), [ [ 0x41 ], [ 0xfffd ] ]);
		assert.deepEqual(stream("utf-8", [ [ 0xf0, 0x9f ], [] ], true), [ [], undefined ]);
		assert.deepEqual(stream("shift_jis", [ [ 0x82 ], [ 0xa0 ] ]), [ [], [ 0x3042 ] ]);
		assert.deepEqual(stream("utf-16le", [ [ 0xff ], [ 0xfe, 0x41 ], [ 0x00 ] ]), [ [], [], [ 0x41 ] ]);
		// A flush ends the stream, so the next read is fresh and strips a BOM again
		const reader = new rt.StringReader("utf-8", false, false);
		reader.read(Uint8Array.from([ 0xf0 ]), false);
		assert.deepEqual(codes(reader.read(Uint8Array.from([ 0x41 ]), true)), [ 0xfffd, 0x41 ]);
		assert.deepEqual(codes(reader.read(Uint8Array.from([ 0xef, 0xbb, 0xbf, 0x42 ]), true)), [ 0x42 ]);
	});
});

await test("string reader accepts buffer sources", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		const read = (view: unknown) =>
			new rt.StringReader("utf-8", false, false).read(view as AllowSharedBufferSource, true);
		const bytes = () => Uint8Array.from([ 0x41, 0x42, 0x43 ]);
		assert.equal(read(bytes().buffer), "ABC");
		assert.equal(read(new Uint8Array(bytes().buffer, 1, 1)), "B");
		assert.equal(read(new DataView(bytes().buffer, 1, 1)), "B");
		assert.equal(read(new Uint16Array([ 0x4241 ])), "AB");
		assert.equal(read(new Uint8Array(new SharedArrayBuffer(1)).fill(0x41)), "A");
		assert.equal(read(new SharedArrayBuffer(0)), "");
		for (const view of [ "abc", [ 0x41 ], {}, null, undefined ]) {
			assert.throws(() => read(view), TypeError);
		}
	});
});

await test("string reader decodes large input", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		const length = 1 << 20;
		const legacy = new rt.StringReader("windows-1252", false, false).read(new Uint8Array(length).fill(0x80), true);
		assert.equal(legacy?.length, length);
		assert.equal(legacy.charCodeAt(0), 0x20ac);
		assert.equal(legacy.charCodeAt(length - 1), 0x20ac);
		const utf16 = new Uint8Array(length * 2 + 2);
		utf16.set([ 0xff, 0xfe ]);
		for (let ii = 2; ii < utf16.length; ii += 2) {
			utf16[ii] = 0x41;
		}
		const withBOM = new rt.StringReader("utf-16le", false, false).read(utf16, true);
		assert.equal(withBOM?.length, length);
		assert.equal(withBOM.charCodeAt(0), 0x41);
		assert.equal(withBOM.charCodeAt(length - 1), 0x41);
	});
});

await test("string writer encodes", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		const writer = new rt.StringWriter();
		const encode = (string: string) => {
			const { read, written, destination } = writer.write(string, true);
			return [ read, written, destination.constructor.name, Array.from(destination) ];
		};
		assert.deepEqual(encode(""), [ 0, 0, "Uint8Array", [] ]);
		assert.deepEqual(encode("hi"), [ 2, 2, "Uint8Array", [ 0x68, 0x69 ] ]);
		assert.deepEqual(
			encode("h\u{e9}\u{1f600}"),
			[ 4, 7, "Uint8Array", [ 0x68, 0xc3, 0xa9, 0xf0, 0x9f, 0x98, 0x80 ] ]);
		assert.deepEqual(
			encode("\u{10000}\ud800x\udc00"),
			[ 5, 11, "Uint8Array", [ 0xf0, 0x90, 0x80, 0x80, 0xef, 0xbf, 0xbd, 0x78, 0xef, 0xbf, 0xbd ] ]);
		// @ts-expect-error
		assert.throws(() => writer.write(123, true), TypeError);
		// @ts-expect-error
		assert.throws(() => writer.write("abc", true, new Uint16Array(4)), TypeError);
		// @ts-expect-error
		assert.throws(() => rt.StringWriter(), TypeError);
	});
});

await test("string writer encodes into a destination", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		const writer = new rt.StringWriter();
		const into = (string: string, length: number) => {
			const buffer = new Uint8Array(length);
			const { read, written, destination } = writer.write(string, true, buffer);
			return [ read, written, destination === buffer, Array.from(buffer) ];
		};
		assert.deepEqual(into("h\u{e9}", 3), [ 2, 3, true, [ 0x68, 0xc3, 0xa9 ] ]);
		assert.deepEqual(into("\u{e9}\u{e9}", 3), [ 1, 2, true, [ 0xc3, 0xa9, 0 ] ]);
		assert.deepEqual(into("a\u{1f600}", 4), [ 1, 1, true, [ 0x61, 0, 0, 0 ] ]);
		assert.deepEqual(into("abc", 0), [ 0, 0, true, [] ]);
		// Writes land at the view's offset
		const offset = new Uint8Array(4);
		const { read, written } = writer.write("abc", true, new Uint8Array(offset.buffer, 1, 2));
		assert.deepEqual([ read, written, Array.from(offset) ], [ 2, 2, [ 0, 0x61, 0x62, 0 ] ]);
		// A detached destination accepts nothing
		const detached = new Uint8Array(4);
		detached.buffer.transfer();
		const result = writer.write("abc", true, detached);
		assert.deepEqual([ result.read, result.written ], [ 0, 0 ]);
	});
});

await test("string writer holds a trailing lead surrogate", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		const writer = new rt.StringWriter();
		const write = (string: string, flush: boolean, destination?: Uint8Array) => {
			const result = writer.write(string, flush, destination);
			return [ result.read, result.written, Array.from(result.destination) ];
		};
		// Held
		assert.deepEqual(write("a\ud83d", false), [ 1, 1, [ 0x61 ] ]);
		// Completed by the next write; the held unit is not counted as read
		assert.deepEqual(write("\ude00", false), [ 1, 4, [ 0xf0, 0x9f, 0x98, 0x80 ] ]);
		assert.deepEqual(write("\ud83d", false), [ 0, 0, [] ]);
		// Replaced when the next write doesn't complete it
		assert.deepEqual(write("x", false), [ 1, 4, [ 0xef, 0xbf, 0xbd, 0x78 ] ]);
		assert.deepEqual(write("\ud83d", false), [ 0, 0, [] ]);
		// Replaced by a flush
		assert.deepEqual(write("", true), [ 0, 3, [ 0xef, 0xbf, 0xbd ] ]);
		// Not held when flushing
		assert.deepEqual(write("\ud83d", true), [ 1, 3, [ 0xef, 0xbf, 0xbd ] ]);
		// Held even when there is room in the destination
		assert.deepEqual(write("a\ud83d", false, new Uint8Array(4)), [ 1, 1, [ 0x61, 0, 0, 0 ] ]);
		assert.deepEqual(write("\ude00", true, new Uint8Array(4)), [ 1, 4, [ 0xf0, 0x9f, 0x98, 0x80 ] ]);
	});
});
