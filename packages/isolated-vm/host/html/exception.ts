import { ConsoleError } from "@isolated-vm/experimental/host/intrinsics";

// https://webidl.spec.whatwg.org/#dfn-DOMException
export class DOMException extends Error {
	constructor(message?: string, name?: string) {
		super(message);
		if (name !== undefined) {
			this.name = name;
		}
	}
}

// TODO:
export function reportException(error: unknown): void {
	ConsoleError(error);
}
