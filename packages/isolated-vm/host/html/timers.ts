// https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#dom-settimeout-dev
import type { Comparator } from "isolated-vm/utility/comparator";
import { ArrayIndexOf, ArrayShift, ArraySplice, DateNow, FunctionConstructor, GlobalIsNaN, MapConstructor, MapDelete, MapGet, MapSet, NumberConstructor, StringConstructor } from "isolated-vm/host/intrinsics";
import { arrayLowerBound } from "isolated-vm/utility/algorithm";
import schedule from "isolated-vm:capability/timers";

interface Timer {
	readonly callback: () => void;
	readonly id: number;
	timeout: number;
}

interface OneShotTimer extends Timer {
	readonly interval: undefined;
}

interface RepeatTimer extends Timer {
	readonly interval: number;
}

type AnyTimer = OneShotTimer | RepeatTimer;

let nextTimeout = Number.MAX_SAFE_INTEGER;
let timerIndex = 0;

const timersById = new MapConstructor<number, AnyTimer>();
/** Sorted (ascending) list of timers by absolute timeout point. */
const timersByTime: AnyTimer[] = [];

const timerComparator: Comparator<Timer> = (left, right) => left.timeout - right.timeout;

/** Accepts a timeout handler & arguments from the developer and returns a plain callback */
function coerceHandler(codeStringOrFunction: unknown, ...args: unknown[]): () => void {
	if (typeof codeStringOrFunction === "function") {
		if (args.length === 0) {
			return codeStringOrFunction as () => void;
		} else {
			// eslint-disable-next-line @typescript-eslint/no-unsafe-return, @typescript-eslint/no-unsafe-call
			return () => codeStringOrFunction(...args);
		}
	} else {
		const string = StringConstructor(codeStringOrFunction);
		return () => {
			const callback = new FunctionConstructor(string);
			// eslint-disable-next-line @typescript-eslint/no-unsafe-call
			callback();
		};
	}
}

function coerceInterval(delay: unknown) {
	let delayNumber = NumberConstructor(delay);
	if (GlobalIsNaN(delayNumber) || delayNumber < 0) {
		delayNumber = 0;
	}
	return delayNumber;
}

function timerLowerBound(timer: Timer) {
	return arrayLowerBound(timersByTime, timer, timerComparator);
}

// https://html.spec.whatwg.org/multipage/timers-and-user-prompts.html#timer-initialisation-steps
function initializeTimer(callback: Timer["callback"], duration: number, repeat: boolean) {
	const timeout = DateNow() + duration;
	const interval = repeat ? duration : undefined;
	const id = timerIndex++;
	const timer: AnyTimer = { id, callback, interval, timeout };
	const lowerBound = timerLowerBound(timer);
	ArraySplice(timersByTime, lowerBound, 0, timer);
	MapSet(timersById, id, timer);
	scheduleTimer(timer);
	return timer.id;
}

function reinitializeRepeatTimer(timer: RepeatTimer) {
	const timeout = DateNow() + timer.interval;
	timer.timeout = timeout;
	const lowerBound = timerLowerBound(timer);
	ArraySplice(timersByTime, lowerBound, 0, timer);
	scheduleTimer(timer);
}

function eraseTimer(timer: Timer) {
	const lowerBound = timerLowerBound(timer);
	const currentIndex = ArrayIndexOf(timersByTime, timer, lowerBound);
	ArraySplice(timersByTime, currentIndex, 1);
	MapDelete(timersById, timer.id);
}

function scheduleTimer(timer: Timer) {
	if (timer.timeout < nextTimeout) {
		nextTimeout = timer.timeout;
		schedule(nextTimeout);
	}
}

function runTimers() {
	let now = 0;
	let timer = timersByTime[0];
	while (timer) {
		if (now < timer.timeout) {
			now = DateNow();
			if (now < timer.timeout) {
				scheduleTimer(timer);
				break;
			}
		}
		ArrayShift(timersByTime);
		if (timer.interval === undefined) {
			MapDelete(timersById, timer.id);
		} else {
			reinitializeRepeatTimer(timer);
		}
		try {
			timer.callback();
		} catch (error) {
			console.error(error);
		}
		timer = timersByTime[0];
	}
}

export function clearInterval(id: unknown): void {
	clearTimeout(id);
}

export function clearTimeout(id: unknown): void {
	const timer = MapGet(timersById, id);
	if (timer) {
		eraseTimer(timer);
	}
}

export function setInterval(callbackOrCodeString: unknown, delay: unknown, ...args: unknown[]): number {
	const handler = coerceHandler(callbackOrCodeString, ...args);
	const interval = coerceInterval(delay);
	return initializeTimer(handler, interval, false);
}

export function setTimeout(callbackOrCodeString: unknown, delay: unknown, ...args: unknown[]): number {
	const handler = coerceHandler(callbackOrCodeString, ...args);
	const interval = coerceInterval(delay);
	return initializeTimer(handler, interval, false);
}

// TODO: Obviously put this somewhere else
// @ts-expect-error
globalThis.runTimers = runTimers;

globalThis.clearInterval = clearInterval;
globalThis.clearTimeout = clearTimeout;
// @ts-expect-error
globalThis.setInterval = setInterval;
// @ts-expect-error
globalThis.setTimeout = setTimeout;
