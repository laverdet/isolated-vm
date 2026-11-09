export class Reference<Type = unknown> {
	#private;
	constructor(value: Type);
}

export class Zone {
	#private;
	constructor();
	static get current(): Zone | undefined;
	run<Result>(callback: () => Result): Result;
}

export function clockTime(): number;
export function performanceTime(): number;
export function transfer(value: unknown, transferList?: readonly unknown[]): unknown;
