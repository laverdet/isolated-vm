// https://html.spec.whatwg.org/multipage/webappapis.html#atob
import { DOMException } from "isolated-vm/host/html/exception";
import { StringConstructor, StringFromCharCode, StringPrototypeCharCodeAt, isNaN } from "isolated-vm/host/intrinsics";

// https://datatracker.ietf.org/doc/html/rfc4648#section-4
const alphabet = [
	"A", "B", "C", "D", "E", "F", "G", "H", "I",
	"J", "K", "L", "M", "N", "O", "P", "Q", "R",
	"S", "T", "U", "V", "W", "X", "Y", "Z", "a",
	"b", "c", "d", "e", "f", "g", "h", "i", "j",
	"k", "l", "m", "n", "o", "p", "q", "r", "s",
	"t", "u", "v", "w", "x", "y", "z", "0", "1",
	"2", "3", "4", "5", "6", "7", "8", "9", "+",
	"/", "=",
];
const kAlphabetSizeish = 0x7f;
const reverseAlphabet: number[] = new Array<number>(0x7f);
reverseAlphabet.fill(-1);
for (const [ key, val ] of alphabet.entries()) {
	reverseAlphabet[val.charCodeAt(0)] = key;
}
// whitespace characters
reverseAlphabet[9] =
reverseAlphabet[10] =
reverseAlphabet[12] =
reverseAlphabet[13] =
reverseAlphabet[32] = kAlphabetSizeish;

/** @internal */
export function atob(inputString: string) {
	const string = StringConstructor(inputString);
	let result = "";
	for (let ii = 0, length = string.length; ii < length;) {
		const read = () => {
			while (true) {
				const char = StringPrototypeCharCodeAt(string, ii++);
				const byte = reverseAlphabet[char]!;
				if (byte === kAlphabetSizeish) {
					continue;
				} else if (byte === -1 || char > kAlphabetSizeish) {
					throw new DOMException("Invalid character", "InvalidCharacterError");
				}
				return byte;
			}
		};
		const byte1 = read();
		const byte2 = read();
		const byte3 = read();
		const byte4 = read();
		const char1 = (byte1 << 2) | (byte2 >> 4);
		const char2 = ((byte2 & 15) << 4) | (byte3 >> 2);
		const char3 = ((byte3 & 3) << 6) | byte4;
		result += StringFromCharCode(char1);
		if (byte3 !== 64) {
			result += StringFromCharCode(char2);
		}
		if (byte4 !== 64) {
			result += StringFromCharCode(char3);
		}
	}
	return result;
}

/** @internal */
export function btoa(inputString: string) {
	const string = StringConstructor(inputString);
	let result = "";
	for (let ii = 0, length = string.length; ii < length;) {
		const read = () => {
			const byte = StringPrototypeCharCodeAt(string, ii++);
			if (byte > 0xff) {
				throw new DOMException("Invalid character", "InvalidCharacterError");
			}
			return byte;
		};
		const byte1 = read();
		const byte2 = read();
		const byte3 = read();
		const char1 = byte1 >> 2;
		const char2 = ((byte1 & 3) << 4) | (byte2 >> 4);
		let char3 = ((byte2 & 15) << 2) | (byte3 >> 6);
		let char4 = byte3 & 63;
		if (isNaN(byte2)) {
			char3 = char4 = 64;
		} else if (isNaN(byte3)) {
			char4 = 64;
		}
		result += alphabet[char1]! + alphabet[char2]! + alphabet[char3]! + alphabet[char4]!;
	}
	return result;
}

globalThis.atob = atob;
globalThis.btoa = btoa;
