import { clearInterval, clearTimeout, setInterval, setTimeout } from "@isolated-vm/runtime/html/timers";

export { initializeTimers } from "@isolated-vm/runtime/html/timers";

globalThis.clearInterval = clearInterval;
globalThis.clearTimeout = clearTimeout;
// @ts-expect-error
globalThis.setInterval = setInterval;
// @ts-expect-error
globalThis.setTimeout = setTimeout;
