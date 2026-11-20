import { clearInterval, clearTimeout, setInterval, setTimeout } from "@isolated-vm/runtime/html/timers";

export { initializeTimers } from "@isolated-vm/runtime/html/timers";

// @ts-expect-error
globalThis.clearInterval = clearInterval;
// @ts-expect-error
globalThis.clearTimeout = clearTimeout;
// @ts-expect-error
globalThis.setInterval = setInterval;
// @ts-expect-error
globalThis.setTimeout = setTimeout;
