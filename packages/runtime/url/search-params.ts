/* eslint-disable @typescript-eslint/no-base-to-string */
/* eslint-disable @typescript-eslint/restrict-template-expressions */
/* eslint-disable @typescript-eslint/prefer-for-of */
// https://url.spec.whatwg.org/#interface-urlsearchparams
import { ArrayPush, ArraySort, FunctionCall, ObjectEntries, ReflectDeleteProperty, StringCharCodeAt, StringSlice, StringToWellFormed, SymbolIterator, SymbolToStringTag, TypeErrorConstructor, Uint8ArrayConstructor } from "@isolated-vm/intrinsics";
import { inFormUrlencodedPercentEncodeSet, percentEncode } from "@isolated-vm/runtime/url/percent-encode";
import { mappedComparator, primitiveComparator } from "@isolated-vm/runtime/utility/comparator";
import { InternalParameter, defineInterface, requireArguments } from "@isolated-vm/runtime/webidl";
import { StringReader, StringWriter } from "isolated-vm:runtime";

const kSpace = 0x20;
const kPercent = 0x25;
const kAmpersand = 0x26;
const kPlus = 0x2b;
const kEquals = 0x3d;
const kQuestionMark = 0x3f;

// What an iterator yields for each pair
type IteratorTransform<Type> = (name: string, value: string) => Type;
const toEntry: IteratorTransform<[ string, string ]> = (name, value) => [ name, value ];
const toKey: IteratorTransform<string> = name => name;
const toValue: IteratorTransform<string> = (name, value) => value;

/**
 * State shared between a `URLSearchParams` and the `URL` it belongs to, if any. The list is flat:
 * `name, value, name, value, ...`
 * @internal
 */
export interface URLSearchParamsInternal {
	list: string[];
	/** Serialization of the list, or `undefined` when it has changed since */
	serialization: string | undefined;
	/** Receives the serialization of the list after each change, to become the URL's query */
	update: ((serialization: string) => void) | undefined;
}

interface URLSearchParamsIteratorInternal<Type> {
	internal: URLSearchParamsInternal;
	transform: IteratorTransform<Type>;
	index: number;
}

// Percent-decoded bytes are UTF-8
// TODO: Will these cause problems with v8 snapshots?
const reader = new StringReader("utf-8", false, true);
const writer = new StringWriter();

function hexDigitValue(code: number): number {
	if (code >= 0x30 && code <= 0x39) {
		return code - 0x30;
	} else if (code >= 0x41 && code <= 0x46) {
		return code - 0x37;
	} else if (code >= 0x61 && code <= 0x66) {
		return code - 0x57;
	} else {
		return -1;
	}
}

/**
 * Decodes one name or value of `application/x-www-form-urlencoded`: `+` is a space, `%XX` is a byte,
 * and the bytes are UTF-8.
 */
// https://url.spec.whatwg.org/#percent-decode
function decodeFormUrlencoded(input: string): string {
	// Fast path with nothing to decode
	let ii = 0;
	for (; ii < input.length; ++ii) {
		const code = StringCharCodeAt(input, ii);
		if (code === kPlus || code === kPercent) {
			break;
		}
	}
	if (ii === input.length) {
		return input;
	}
	// The UTF-8 encoding of the input is decoded in place, since decoding never grows it
	const bytes = writer.write(input, true).destination;
	const decoded = new Uint8ArrayConstructor(bytes.length);
	let length = 0;
	for (let jj = 0; jj < bytes.length; ++jj) {
		const byte = bytes[jj]!;
		if (byte === kPlus) {
			decoded[length++] = kSpace;
		} else if (byte === kPercent && jj + 2 < bytes.length) {
			const high = hexDigitValue(bytes[jj + 1]!);
			const low = hexDigitValue(bytes[jj + 2]!);
			if (high === -1 || low === -1) {
				decoded[length++] = byte;
			} else {
				decoded[length++] = (high << 4) | low;
				jj += 2;
			}
		} else {
			decoded[length++] = byte;
		}
	}
	return reader.read(new Uint8ArrayConstructor(decoded.buffer, 0, length), true) ?? "";
}

// https://url.spec.whatwg.org/#concept-urlencoded-parser
/** @internal */
export function parseFormUrlencoded(input: string): string[] {
	const list: string[] = [];
	for (let start = 0; start < input.length;) {
		// A sequence runs to the next `&`, and its first `=` separates the name from the value
		let end = start;
		let equals = -1;
		for (; end < input.length; ++end) {
			const code = StringCharCodeAt(input, end);
			if (code === kAmpersand) {
				break;
			} else if (code === kEquals && equals === -1) {
				equals = end;
			}
		}
		if (end > start) {
			const name = StringSlice(input, start, equals === -1 ? end : equals);
			const value = equals === -1 ? "" : StringSlice(input, equals + 1, end);
			ArrayPush(list, decodeFormUrlencoded(name), decodeFormUrlencoded(value));
		}
		start = end + 1;
	}
	return list;
}

// https://url.spec.whatwg.org/#concept-urlencoded-serializer
function serializeFormUrlencoded(list: readonly string[]): string {
	let output = "";
	for (let ii = 0; ii < list.length; ii += 2) {
		const pair = `${percentEncode(list[ii]!, inFormUrlencodedPercentEncodeSet, true)}=${percentEncode(list[ii + 1]!, inFormUrlencodedPercentEncodeSet, true)}`;
		output = ii === 0 ? pair : `${output}&${pair}`;
	}
	return output;
}

/** Converts the constructor's `init` union into a list */
function parseInit(init: unknown): string[] {
	if ((typeof init === "object" && init !== null) || typeof init === "function") {
		const list: string[] = [];
		if ((init as Record<symbol, unknown>)[SymbolIterator] == null) {
			// `{ param: 'value', ... }`
			const entries = ObjectEntries(init as Record<string, unknown>);
			for (let ii = 0; ii < entries.length; ++ii) {
				const entry = entries[ii]!;
				const name = StringToWellFormed(entry[0]);
				const value = StringToWellFormed(`${entry[1]}`);
				// Keys which read the same once converted merge, with the last value in the first position
				let jj = 0;
				for (; jj < list.length; jj += 2) {
					if (list[jj] === name) {
						list[jj + 1] = value;
						break;
					}
				}
				if (jj === list.length) {
					ArrayPush(list, name, value);
				}
			}
		} else {
			// `[ [ 'param', 'value' ], ... }`
			for (const pair of init as Iterable<unknown>) {
				if ((typeof pair !== "object" || pair === null) && typeof pair !== "function") {
					throw new TypeErrorConstructor("Provided value is not of type 'object'");
				}
				let count = 0;
				let name = "";
				let value = "";
				for (const item of pair as Iterable<unknown>) {
					if (count === 0) {
						name = StringToWellFormed(`${item}`);
					} else if (count === 1) {
						value = StringToWellFormed(`${item}`);
					}
					++count;
				}
				if (count !== 2) {
					throw new TypeErrorConstructor("Each query pair must be an iterable [name, value] tuple");
				}
				ArrayPush(list, name, value);
			}
		}
		return list;
	} else {
		// `param=value&...`
		const query = StringToWellFormed(`${init}`);
		return parseFormUrlencoded(StringCharCodeAt(query, 0) === kQuestionMark ? StringSlice(query, 1) : query);
	}
}

const compareNames = mappedComparator(primitiveComparator, (pair: readonly [ string, string ]) => pair[0]);

export class URLSearchParams {
	readonly #internal: URLSearchParamsInternal;

	static {
		// https://webidl.spec.whatwg.org/#es-iterable
		// eslint-disable-next-line @typescript-eslint/unbound-method -- `@@iterator` is the same function object as `entries`
		defineInterface(this, { [SymbolIterator]: { value: this.prototype.entries, writable: true, configurable: true } });
	}

	// https://url.spec.whatwg.org/#dom-urlsearchparams-urlsearchparams
	constructor(init?: Iterable<readonly [ string, string ]> | Record<string, string> | string);
	/** @internal */
	constructor(internal: InternalParameter<URLSearchParamsInternal>);
	constructor(init: unknown = "") {
		const internal = InternalParameter.unwrap<URLSearchParamsInternal>(init);
		this.#internal = internal ?? { list: parseInit(init), serialization: undefined, update: undefined };
	}

	// https://url.spec.whatwg.org/#dom-urlsearchparams-size
	get size(): number {
		return this.#internal.list.length / 2;
	}

	// https://url.spec.whatwg.org/#dom-urlsearchparams-append
	append(name: string, value: string): void;
	append(name: unknown, value: unknown): void {
		const internal = this.#internal;
		requireArguments(arguments.length, 2);
		ArrayPush(internal.list, StringToWellFormed(`${name}`), StringToWellFormed(`${value}`));
		this.#update();
	}

	// https://url.spec.whatwg.org/#dom-urlsearchparams-delete
	delete(name: string, value?: string): void;
	delete(name: unknown, value: unknown = undefined): void {
		const internal = this.#internal;
		requireArguments(arguments.length, 1);
		const nameString = StringToWellFormed(`${name}`);
		const valueString = value === undefined ? undefined : StringToWellFormed(`${value}`);
		const { list } = internal;
		let kept = 0;
		for (let ii = 0; ii < list.length; ii += 2) {
			if (list[ii] !== nameString || (valueString !== undefined && list[ii + 1] !== valueString)) {
				list[kept] = list[ii]!;
				list[kept + 1] = list[ii + 1]!;
				kept += 2;
			}
		}
		list.length = kept;
		this.#update();
	}

	// https://url.spec.whatwg.org/#dom-urlsearchparams-get
	get(name: string): string | null;
	get(name: unknown): string | null {
		const { list } = this.#internal;
		requireArguments(arguments.length, 1);
		const nameString = StringToWellFormed(`${name}`);
		for (let ii = 0; ii < list.length; ii += 2) {
			if (list[ii] === nameString) {
				return list[ii + 1]!;
			}
		}
		return null;
	}

	// https://url.spec.whatwg.org/#dom-urlsearchparams-getall
	getAll(name: string): string[];
	getAll(name: unknown): string[] {
		const { list } = this.#internal;
		requireArguments(arguments.length, 1);
		const nameString = StringToWellFormed(`${name}`);
		const values: string[] = [];
		for (let ii = 0; ii < list.length; ii += 2) {
			if (list[ii] === nameString) {
				ArrayPush(values, list[ii + 1]!);
			}
		}
		return values;
	}

	// https://url.spec.whatwg.org/#dom-urlsearchparams-has
	has(name: string, value?: string): boolean;
	has(name: unknown, value: unknown = undefined): boolean {
		const { list } = this.#internal;
		requireArguments(arguments.length, 1);
		const nameString = StringToWellFormed(`${name}`);
		const valueString = value === undefined ? undefined : StringToWellFormed(`${value}`);
		for (let ii = 0; ii < list.length; ii += 2) {
			if (list[ii] === nameString && (valueString === undefined || list[ii + 1] === valueString)) {
				return true;
			}
		}
		return false;
	}

	// https://url.spec.whatwg.org/#dom-urlsearchparams-set
	set(name: string, value: string): void;
	set(name: unknown, value: unknown): void {
		const internal = this.#internal;
		requireArguments(arguments.length, 2);
		const nameString = StringToWellFormed(`${name}`);
		const valueString = StringToWellFormed(`${value}`);
		// The first pair with the name takes the value, and the rest are removed, compacting in place
		const { list } = internal;
		let kept = 0;
		let found = false;
		for (let ii = 0; ii < list.length; ii += 2) {
			if (list[ii] !== nameString) {
				list[kept] = list[ii]!;
				list[kept + 1] = list[ii + 1]!;
				kept += 2;
			} else if (!found) {
				found = true;
				list[kept] = nameString;
				list[kept + 1] = valueString;
				kept += 2;
			}
		}
		list.length = kept;
		if (!found) {
			ArrayPush(list, nameString, valueString);
		}
		this.#update();
	}

	// https://url.spec.whatwg.org/#dom-urlsearchparams-sort
	sort(): void {
		const internal = this.#internal;
		// Stable, by name, comparing code units
		const { list } = internal;
		const pairs: [ string, string ][] = [];
		for (let ii = 0; ii < list.length; ii += 2) {
			ArrayPush(pairs, [ list[ii]!, list[ii + 1]! ]);
		}
		ArraySort(pairs, compareNames);
		const sorted: string[] = [];
		for (let ii = 0; ii < pairs.length; ++ii) {
			ArrayPush(sorted, pairs[ii]![0], pairs[ii]![1]);
		}
		internal.list = sorted;
		this.#update();
	}

	// https://webidl.spec.whatwg.org/#es-iterable-entries
	entries(): IterableIterator<[ string, string ]> {
		return new URLSearchParamsIterator(new InternalParameter({ internal: this.#internal, transform: toEntry, index: 0 }));
	}

	keys(): IterableIterator<string> {
		return new URLSearchParamsIterator(new InternalParameter({ internal: this.#internal, transform: toKey, index: 0 }));
	}

	values(): IterableIterator<string> {
		return new URLSearchParamsIterator(new InternalParameter({ internal: this.#internal, transform: toValue, index: 0 }));
	}

	// https://webidl.spec.whatwg.org/#es-forEach
	forEach(callback: (value: string, name: string, searchParams: URLSearchParams) => void, thisArg?: unknown): void;
	forEach(callback: unknown, thisArg: unknown = undefined): void {
		const internal = this.#internal;
		requireArguments(arguments.length, 1);
		if (typeof callback !== "function") {
			throw new TypeErrorConstructor("Provided value is not a function");
		}
		// The list is read back each step, since the callback may change it
		for (let ii = 0; ii < internal.list.length; ii += 2) {
			type Callback = (this: unknown, value: string, name: string, searchParams: URLSearchParams) => void;
			FunctionCall(callback as Callback, thisArg, internal.list[ii + 1]!, internal.list[ii]!, this);
		}
	}

	// https://url.spec.whatwg.org/#urlsearchparams-stringification-behavior
	toString(): string {
		const internal = this.#internal;
		return internal.serialization ??= serializeFormUrlencoded(internal.list);
	}

	// https://url.spec.whatwg.org/#concept-urlsearchparams-update
	#update(): void {
		const internal = this.#internal;
		if (internal.update === undefined) {
			// Serialized when asked for
			internal.serialization = undefined;
		} else {
			const serialization = serializeFormUrlencoded(internal.list);
			internal.serialization = serialization;
			internal.update(serialization);
		}
	}
}

// https://webidl.spec.whatwg.org/#es-iterator-prototype-object
class URLSearchParamsIterator<Type> extends Iterator<Type, undefined> {
	readonly #internal: URLSearchParamsIteratorInternal<Type>;

	static {
		defineInterface(this, { [SymbolToStringTag]: { value: "URLSearchParams Iterator", configurable: true } });
		ReflectDeleteProperty(this.prototype, "constructor");
	}

	constructor(internal: unknown = undefined) {
		const unwrapped = InternalParameter.unwrap<URLSearchParamsIteratorInternal<Type>>(internal);
		if (unwrapped === undefined) {
			throw new TypeErrorConstructor("Illegal constructor");
		}
		super();
		this.#internal = unwrapped;
	}

	next(): IteratorResult<Type, undefined> {
		const state = this.#internal;
		// The list is read back each step, since it may have changed
		const { list } = state.internal;
		if (state.index >= list.length) {
			return { value: undefined, done: true };
		}
		const name = list[state.index]!;
		const value = list[state.index + 1]!;
		state.index += 2;
		return { value: state.transform(name, value), done: false };
	}
}
