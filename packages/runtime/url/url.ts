// https://url.spec.whatwg.org/#url-class
/* eslint-disable @typescript-eslint/no-base-to-string, @typescript-eslint/restrict-template-expressions -- `USVString` is `ToString` made well-formed */
import type { URLComponent, URLRecord } from "isolated-vm:runtime";
import { StringCharCodeAt, StringSlice, StringToWellFormed, TypeErrorConstructor } from "@isolated-vm/intrinsics";
import { InternalParameter, defineInterface, requireArguments } from "@isolated-vm/runtime/webidl";
import { parseURL, updateURL } from "isolated-vm:runtime";

const kSpace = 0x20;
const kNumberSign = 0x23;
const kSlash = 0x2f;
const kQuestionMark = 0x3f;
const kAtSign = 0x40;
const kHexDigits = "0123456789ABCDEF";

// `url_component`
const kProtocol = 0;
const kUsername = 1;
const kPassword = 2;
const kHost = 3;
const kHostname = 4;
const kPort = 5;
const kPathname = 6;

/** A parsed URL, carried through the constructor by `URL.parse` */
interface ParsedURL {
	href: string;
	record: URLRecord;
}

// https://url.spec.whatwg.org/#special-scheme
function isSpecialScheme(protocol: string): boolean {
	switch (protocol) {
		case "ftp:":
		case "file:":
		case "http:":
		case "https:":
		case "ws:":
		case "wss:":
			return true;
		default:
			return false;
	}
}

// https://url.spec.whatwg.org/#c0-control-percent-encode-set
function inC0ControlPercentEncodeSet(code: number): boolean {
	return code < 0x20 || code > 0x7e;
}

// https://url.spec.whatwg.org/#fragment-percent-encode-set
function inFragmentPercentEncodeSet(code: number): boolean {
	return inC0ControlPercentEncodeSet(code) || code === 0x20 || code === 0x22 || code === 0x3c || code === 0x3e || code === 0x60;
}

// https://url.spec.whatwg.org/#query-percent-encode-set
function inQueryPercentEncodeSet(code: number): boolean {
	return inC0ControlPercentEncodeSet(code) || code === 0x20 || code === 0x22 || code === 0x23 || code === 0x3c || code === 0x3e;
}

// https://url.spec.whatwg.org/#special-query-percent-encode-set
function inSpecialQueryPercentEncodeSet(code: number): boolean {
	return inQueryPercentEncodeSet(code) || code === 0x27;
}

function percentEncodeByte(byte: number): string {
	return `%${kHexDigits[byte >> 4]}${kHexDigits[byte & 0xf]}`;
}

// https://url.spec.whatwg.org/#utf-8-percent-encode
function percentEncodeCodePoint(codePoint: number): string {
	if (codePoint < 0x800) {
		return `${percentEncodeByte(0xc0 | (codePoint >> 6))}${percentEncodeByte(0x80 | (codePoint & 0x3f))}`;
	} else if (codePoint < 0x10000) {
		return `${percentEncodeByte(0xe0 | (codePoint >> 12))}${percentEncodeByte(0x80 | ((codePoint >> 6) & 0x3f))}${percentEncodeByte(0x80 | (codePoint & 0x3f))}`;
	} else {
		return `${percentEncodeByte(0xf0 | (codePoint >> 18))}${percentEncodeByte(0x80 | ((codePoint >> 12) & 0x3f))}${percentEncodeByte(0x80 | ((codePoint >> 6) & 0x3f))}${percentEncodeByte(0x80 | (codePoint & 0x3f))}`;
	}
}

/**
 * Percent-encodes `input` for a query or fragment. Tabs and newlines are dropped, any code point which
 * `inPercentEncodeSet` selects becomes `%XX` for each of its UTF-8 bytes, and everything else is kept
 * as it is.
 */
// https://url.spec.whatwg.org/#string-percent-encode-after-encoding
function percentEncode(input: string, inPercentEncodeSet: (code: number) => boolean): string {
	let result = "";
	// Start of the run of characters which pass through as they are
	let start = 0;
	for (let ii = 0; ii < input.length; ++ii) {
		const code = StringCharCodeAt(input, ii);
		if (code < 0x80) {
			if (code === 0x09 || code === 0x0a || code === 0x0d) {
				result += StringSlice(input, start, ii);
				start = ii + 1;
			} else if (inPercentEncodeSet(code)) {
				result += `${StringSlice(input, start, ii)}${percentEncodeByte(code)}`;
				start = ii + 1;
			}
		} else if (code >= 0xd800 && code < 0xdc00) {
			// nb: Input is already `toWellFormed`
			const codePoint = 0x10000 + ((code - 0xd800) << 10) + (StringCharCodeAt(input, ii + 1) - 0xdc00);
			result += `${StringSlice(input, start, ii)}${percentEncodeCodePoint(codePoint)}`;
			start = ii + 2;
			++ii;
		} else {
			result += `${StringSlice(input, start, ii)}${percentEncodeCodePoint(code)}`;
			start = ii + 1;
		}
	}
	return start === 0 ? input : `${result}${StringSlice(input, start)}`;
}

/** Drops trailing spaces */
function stripTrailingSpaces(href: string): string {
	let end = href.length;
	while (end > 0 && StringCharCodeAt(href, end - 1) === kSpace) {
		--end;
	}
	return end === href.length ? href : StringSlice(href, 0, end);
}

export class URL {
	#href: string;
	#record: URLRecord;

	static { defineInterface(this); }

	// https://url.spec.whatwg.org/#dom-url-url
	constructor(url: string | URL, base?: string | URL);
	/** @internal */
	constructor(parsed: InternalParameter<ParsedURL>);
	constructor(url: unknown, base: unknown = undefined) {
		const parsed = InternalParameter.unwrap<ParsedURL>(url);
		if (parsed !== undefined) {
			this.#href = parsed.href;
			this.#record = parsed.record;
			return;
		}
		requireArguments(arguments.length, 1);
		const input = StringToWellFormed(`${url}`);
		const record = parseURL(input, base === undefined ? undefined : StringToWellFormed(`${base}`));
		if (record === undefined) {
			throw new TypeErrorConstructor("Invalid URL");
		}
		this.#href = record.href ?? input;
		this.#record = record;
	}

	// https://url.spec.whatwg.org/#dom-url-canparse
	static canParse(url: string | URL, base?: string | URL): boolean;
	static canParse(url: unknown, base: unknown = undefined): boolean {
		requireArguments(arguments.length, 1);
		return parseURL(StringToWellFormed(`${url}`), base === undefined ? undefined : StringToWellFormed(`${base}`)) !== undefined;
	}

	// https://url.spec.whatwg.org/#dom-url-parse
	static parse(url: string | URL, base?: string | URL): URL | null;
	static parse(url: unknown, base: unknown = undefined): URL | null {
		requireArguments(arguments.length, 1);
		const input = StringToWellFormed(`${url}`);
		const record = parseURL(input, base === undefined ? undefined : StringToWellFormed(`${base}`));
		if (record === undefined) {
			return null;
		}
		return new URL(new InternalParameter<ParsedURL>({ href: record.href ?? input, record }));
	}

	// https://url.spec.whatwg.org/#dom-url-href
	get href(): string {
		return this.#href;
	}

	set href(value: unknown) {
		const input = StringToWellFormed(`${value}`);
		const record = parseURL(input);
		if (record === undefined) {
			throw new TypeErrorConstructor("Invalid URL");
		}
		this.#href = record.href ?? input;
		this.#record = record;
	}

	// https://url.spec.whatwg.org/#dom-url-origin
	get origin(): string {
		const protocol = StringSlice(this.#href, 0, this.#record.protocolEnd);
		switch (protocol) {
			case "ftp:":
			case "http:":
			case "https:":
			case "ws:":
			case "wss:":
				return `${protocol}//${this.host}`;
			case "blob:": {
				// https://url.spec.whatwg.org/#concept-url-origin
				const url = URL.parse(this.pathname);
				if (url !== null) {
					const inner = url.protocol;
					if (inner === "http:" || inner === "https:") {
						return url.origin;
					}
				}
				return "null";
			}
			default:
				return "null";
		}
	}

	// https://url.spec.whatwg.org/#dom-url-protocol
	get protocol(): string {
		return StringSlice(this.#href, 0, this.#record.protocolEnd);
	}

	set protocol(value: unknown) {
		this.#update(kProtocol, value);
	}

	// https://url.spec.whatwg.org/#dom-url-username
	get username(): string {
		const { protocolEnd, usernameEnd } = this.#record;
		// The username, if any, follows the `//` of the authority
		return protocolEnd + 2 < usernameEnd ? StringSlice(this.#href, protocolEnd + 2, usernameEnd) : "";
	}

	set username(value: unknown) {
		this.#update(kUsername, value);
	}

	// https://url.spec.whatwg.org/#dom-url-password
	get password(): string {
		const { usernameEnd, hostStart } = this.#record;
		// The password, if any, is between the `:` ending the username and the `@` starting the host
		return usernameEnd < hostStart ? StringSlice(this.#href, usernameEnd + 1, hostStart) : "";
	}

	set password(value: unknown) {
		this.#update(kPassword, value);
	}

	// https://url.spec.whatwg.org/#dom-url-host
	get host(): string {
		const { hostEnd, pathnameStart } = this.#record;
		const start = this.#hostStart();
		// Without a host, the serialization of a non-special URL may hold `/.` before the pathname
		return start === hostEnd ? "" : StringSlice(this.#href, start, pathnameStart);
	}

	set host(value: unknown) {
		this.#update(kHost, value);
	}

	// https://url.spec.whatwg.org/#dom-url-hostname
	get hostname(): string {
		return StringSlice(this.#href, this.#hostStart(), this.#record.hostEnd);
	}

	set hostname(value: unknown) {
		this.#update(kHostname, value);
	}

	// https://url.spec.whatwg.org/#dom-url-port
	get port(): string {
		const { port } = this.#record;
		return port === undefined ? "" : `${port}`;
	}

	set port(value: unknown) {
		this.#update(kPort, value);
	}

	// https://url.spec.whatwg.org/#dom-url-pathname
	get pathname(): string {
		const { pathnameStart, searchStart } = this.#record;
		return StringSlice(this.#href, pathnameStart, searchStart);
	}

	set pathname(value: unknown) {
		const input = StringToWellFormed(`${value}`);
		const href = this.#href;
		const { pathnameStart, searchStart, hashStart } = this.#record;
		// Only the scheme and authority need parsing again, and the query and fragment go back as they
		// are. A leading slash stays with the prefix, so that a host-less URL doesn't come back with an
		// opaque path.
		const prefix = StringSlice(href, 0, StringCharCodeAt(href, pathnameStart) === kSlash ? pathnameStart + 1 : pathnameStart);
		const record = updateURL(prefix, kPathname, input);
		if (record === undefined) {
			return;
		}
		const head = record.href ?? prefix;
		this.#href = `${head}${StringSlice(href, searchStart)}`;
		this.#record = { ...record, searchStart: head.length, hashStart: head.length + hashStart - searchStart };
	}

	// https://url.spec.whatwg.org/#dom-url-search
	get search(): string {
		const { searchStart, hashStart } = this.#record;
		// A lone `?` serializes as an empty search
		return hashStart - searchStart <= 1 ? "" : StringSlice(this.#href, searchStart, hashStart);
	}

	set search(value: unknown) {
		const input = StringToWellFormed(`${value}`);
		if (input === "") {
			this.#setQueryAndFragment(undefined, this.#fragment());
		} else {
			const query = StringCharCodeAt(input, 0) === kQuestionMark ? StringSlice(input, 1) : input;
			const inPercentEncodeSet = isSpecialScheme(StringSlice(this.#href, 0, this.#record.protocolEnd))
				? inSpecialQueryPercentEncodeSet
				: inQueryPercentEncodeSet;
			this.#setQueryAndFragment(`?${percentEncode(query, inPercentEncodeSet)}`, this.#fragment());
		}
	}

	// https://url.spec.whatwg.org/#dom-url-hash
	get hash(): string {
		const { hashStart } = this.#record;
		// A lone `#` serializes as an empty hash
		return this.#href.length - hashStart <= 1 ? "" : StringSlice(this.#href, hashStart);
	}

	set hash(value: unknown) {
		const input = StringToWellFormed(`${value}`);
		if (input === "") {
			this.#setQueryAndFragment(this.#query(), undefined);
		} else {
			const fragment = StringCharCodeAt(input, 0) === kNumberSign ? StringSlice(input, 1) : input;
			this.#setQueryAndFragment(this.#query(), `#${percentEncode(fragment, inFragmentPercentEncodeSet)}`);
		}
	}

	// https://url.spec.whatwg.org/#dom-url-tojson
	toJSON(): string {
		return this.#href;
	}

	// https://url.spec.whatwg.org/#URL-stringification-behavior
	toString(): string {
		return this.#href;
	}

	/** Start of the host, past the `@` which ends the credentials */
	#hostStart(): number {
		const { hostStart } = this.#record;
		return StringCharCodeAt(this.#href, hostStart) === kAtSign ? hostStart + 1 : hostStart;
	}

	/** The query with its `?`, or `undefined` when there is none */
	#query(): string | undefined {
		const { searchStart, hashStart } = this.#record;
		return searchStart === hashStart ? undefined : StringSlice(this.#href, searchStart, hashStart);
	}

	/** The fragment with its `#`, or `undefined` when there is none */
	#fragment(): string | undefined {
		const { hashStart } = this.#record;
		return hashStart === this.#href.length ? undefined : StringSlice(this.#href, hashStart);
	}

	/** Replaces the query and fragment, each serialized with its delimiter, or `undefined` for none */
	#setQueryAndFragment(query: string | undefined, fragment: string | undefined): void {
		const record = this.#record;
		let head = StringSlice(this.#href, 0, record.searchStart);
		// https://url.spec.whatwg.org/#potentially-strip-trailing-spaces-from-an-opaque-path
		if (query === undefined && fragment === undefined) {
			head = stripTrailingSpaces(head);
		}
		const searchStart = head.length;
		const hashStart = query === undefined ? searchStart : searchStart + query.length;
		this.#href = `${head}${query ?? ""}${fragment ?? ""}`;
		this.#record = { ...record, searchStart, hashStart };
	}

	/** Runs a setter in the runtime, which leaves the URL alone when it rejects the value */
	#update(component: URLComponent, value: unknown): void {
		const record = updateURL(this.#href, component, StringToWellFormed(`${value}`));
		if (record !== undefined) {
			this.#href = record.href ?? this.#href;
			this.#record = record;
		}
	}
}
