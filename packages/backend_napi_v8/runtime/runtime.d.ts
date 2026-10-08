export class Reference<Type = unknown> {
	#private;
	constructor(value: Type);
}

export function clockTime(): number;
export function performanceTime(): number;
export function transfer(value: unknown, transferList?: readonly unknown[]): unknown;
