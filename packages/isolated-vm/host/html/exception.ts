// https://webidl.spec.whatwg.org/#dfn-DOMException
export class DOMException extends Error {
	constructor(message?: string, name?: string) {
		super(message);
		if (name !== undefined) {
			this.name = name;
		}
	}
}

// @ts-expect-error
globalThis.DOMException = DOMException;
