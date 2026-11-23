// https://url.spec.whatwg.org/#percent-encoded-bytes
import { StringCharCodeAt, StringSlice } from "@isolated-vm/intrinsics";

const kSpace = 0x20;
const kHexDigits = "0123456789ABCDEF";

// https://url.spec.whatwg.org/#c0-control-percent-encode-set
function inC0ControlPercentEncodeSet(code: number): boolean {
	return code < 0x20 || code > 0x7e;
}

// https://url.spec.whatwg.org/#fragment-percent-encode-set
/** @internal */
export function inFragmentPercentEncodeSet(code: number): boolean {
	return inC0ControlPercentEncodeSet(code) || code === 0x20 || code === 0x22 || code === 0x3c || code === 0x3e || code === 0x60;
}

// https://url.spec.whatwg.org/#query-percent-encode-set
/** @internal */
export function inQueryPercentEncodeSet(code: number): boolean {
	return inC0ControlPercentEncodeSet(code) || code === 0x20 || code === 0x22 || code === 0x23 || code === 0x3c || code === 0x3e;
}

// https://url.spec.whatwg.org/#special-query-percent-encode-set
/** @internal */
export function inSpecialQueryPercentEncodeSet(code: number): boolean {
	return inQueryPercentEncodeSet(code) || code === 0x27;
}

// https://url.spec.whatwg.org/#application-x-www-form-urlencoded-percent-encode-set
/** Everything but ASCII alphanumerics and `*-._` */
/** @internal */
export function inFormUrlencodedPercentEncodeSet(code: number): boolean {
	return !(
		(code >= 0x30 && code <= 0x39) ||
		(code >= 0x41 && code <= 0x5a) ||
		(code >= 0x61 && code <= 0x7a) ||
		code === 0x2a || code === 0x2d || code === 0x2e || code === 0x5f);
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
 * Percent-encodes `input`: any code point which `inPercentEncodeSet` selects becomes `%XX` for each
 * of its UTF-8 bytes, and everything else is kept as it is. With `spaceAsPlus`, a space becomes `+`
 * instead, as `application/x-www-form-urlencoded` has it. The input is well-formed, so surrogates
 * come in pairs.
 * @internal
 */
// https://url.spec.whatwg.org/#string-percent-encode-after-encoding
export function percentEncode(input: string, inPercentEncodeSet: (code: number) => boolean, spaceAsPlus = false): string {
	let result = "";
	// Start of the run of characters which pass through as they are
	let start = 0;
	for (let ii = 0; ii < input.length; ++ii) {
		const code = StringCharCodeAt(input, ii);
		if (code < 0x80) {
			if (spaceAsPlus && code === kSpace) {
				result += `${StringSlice(input, start, ii)}+`;
				start = ii + 1;
			} else if (inPercentEncodeSet(code)) {
				result += `${StringSlice(input, start, ii)}${percentEncodeByte(code)}`;
				start = ii + 1;
			}
		} else if (code >= 0xd800 && code < 0xdc00) {
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
