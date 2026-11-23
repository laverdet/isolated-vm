// https://url.spec.whatwg.org/#url-class
/* eslint-disable @typescript-eslint/no-base-to-string, @typescript-eslint/restrict-template-expressions -- `USVString` is `ToString` made well-formed */
import type { URLSearchParamsInternal } from "@isolated-vm/runtime/url/search-params";
import type { URLComponent, URLRecord } from "isolated-vm:runtime";
import { StringCharCodeAt, StringSlice, StringToWellFormed, TypeErrorConstructor } from "@isolated-vm/intrinsics";
import { inFragmentPercentEncodeSet, inQueryPercentEncodeSet, inSpecialQueryPercentEncodeSet, percentEncode } from "@isolated-vm/runtime/url/percent-encode";
import { URLSearchParams, parseFormUrlencoded } from "@isolated-vm/runtime/url/search-params";
import { InternalParameter, defineInterface, requireArguments } from "@isolated-vm/runtime/webidl";
import { parseURL, updateURL } from "isolated-vm:runtime";

const kSpace = 0x20;
const kNumberSign = 0x23;
const kSlash = 0x2f;
const kQuestionMark = 0x3f;
const kAtSign = 0x40;

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

/** The `URLSearchParams` of a URL, and the list it shares with the URL */
interface QueryObject {
	object: URLSearchParams;
	internal: URLSearchParamsInternal;
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

/** Removes ASCII tab or newline, as the basic URL parser does before anything else */
function stripTabsAndNewlines(input: string): string {
	let result = "";
	// Start of the run of characters which are kept
	let start = 0;
	for (let ii = 0; ii < input.length; ++ii) {
		const code = StringCharCodeAt(input, ii);
		if (code === 0x09 || code === 0x0a || code === 0x0d) {
			result += StringSlice(input, start, ii);
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
	#queryObject: QueryObject | undefined = undefined;

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
		this.#resetQueryObject();
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
			const query = stripTabsAndNewlines(StringCharCodeAt(input, 0) === kQuestionMark ? StringSlice(input, 1) : input);
			const inPercentEncodeSet = isSpecialScheme(StringSlice(this.#href, 0, this.#record.protocolEnd))
				? inSpecialQueryPercentEncodeSet
				: inQueryPercentEncodeSet;
			this.#setQueryAndFragment(`?${percentEncode(query, inPercentEncodeSet)}`, this.#fragment());
		}
		this.#resetQueryObject();
	}

	// https://url.spec.whatwg.org/#dom-url-searchparams
	get searchParams(): URLSearchParams {
		if (this.#queryObject === undefined) {
			const internal: URLSearchParamsInternal = {
				list: parseFormUrlencoded(this.#queryWithoutMark()),
				serialization: undefined,
				// https://url.spec.whatwg.org/#concept-urlsearchparams-update
				update: serialization => this.#setQueryAndFragment(serialization === "" ? undefined : `?${serialization}`, this.#fragment()),
			};
			this.#queryObject = { object: new URLSearchParams(new InternalParameter(internal)), internal };
		}
		return this.#queryObject.object;
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
			const fragment = stripTabsAndNewlines(StringCharCodeAt(input, 0) === kNumberSign ? StringSlice(input, 1) : input);
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

	/** The query without its `?`, which is empty when there is none */
	#queryWithoutMark(): string {
		const { searchStart, hashStart } = this.#record;
		return searchStart === hashStart ? "" : StringSlice(this.#href, searchStart + 1, hashStart);
	}

	/** The fragment with its `#`, or `undefined` when there is none */
	#fragment(): string | undefined {
		const { hashStart } = this.#record;
		return hashStart === this.#href.length ? undefined : StringSlice(this.#href, hashStart);
	}

	/** Parses the query into the list of the query object again, if there is one */
	#resetQueryObject(): void {
		if (this.#queryObject !== undefined) {
			const { internal } = this.#queryObject;
			internal.list = parseFormUrlencoded(this.#queryWithoutMark());
			internal.serialization = undefined;
		}
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
