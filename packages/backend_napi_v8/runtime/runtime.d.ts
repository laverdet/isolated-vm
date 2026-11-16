export class Reference<Type = unknown> {
	#private;
	constructor(value: Type);
}

export interface ZoneOptions {
	timeout?: number | undefined;
}

export class Zone {
	#private;
	constructor(options?: ZoneOptions);
	static get current(): Zone | undefined;
	static detach(callback: () => void, options?: ZoneOptions): void;
	get data(): Map<unknown, unknown>;
	get timeout(): number | undefined;
	adjustTimeout(delta: number): boolean;
	applyTimeout(timeout: number): boolean;
	clone(): Zone;
	run<Result>(callback: () => Result): Result;
}

export function clockTime(): number;
export function enqueueMicrotask(callback: () => void): void;
export function getPromiseInternal<Type>(promise: Promise<Type>): [ true, Type ] | [ false, unknown ] | undefined;
export function performanceTime(): number;
export function transfer(value: unknown, transferList?: readonly unknown[]): unknown;
