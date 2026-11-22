import type { SourceOrigin } from "./script.js";
import type { Constructor } from "@isolated-vm/experimental/utility/object";
import { extend } from "@isolated-vm/experimental/utility/object";
import * as backend from "#backend";

export namespace Agent {
	export namespace Clock {
		export interface Deterministic {
			type: "deterministic";
			epoch: Date;
			interval: number;
		}

		export interface Microtask {
			type: "microtask";
			epoch?: Date | undefined;
		}

		export interface Realtime {
			type: "realtime";
			epoch: Date;
		}

		export interface System {
			type: "system";
		}
	}

	export namespace CreateOptions {
		export type Clock = Clock.Deterministic | Clock.Microtask | Clock.Realtime | Clock.System;

	}

	export interface CreateOptions {
		clock?: CreateOptions.Clock | undefined;
		memoryLimitBytes?: number | undefined;

		/**
		 * Callback to be invoked when a *very bad* error occurs. If this is invoked it means that v8
		 * has lost all control over the isolate, and all resources in use are totally unrecoverable. If
		 * you receive this error you should log the error, stop serving requests, finish outstanding
		 * work, and stop the process.
		 */
		onCatastrophicError?: ((message: string) => void) | undefined;
		randomSeed?: number | undefined;
	}

	export interface CompileModuleOptions {
		origin?: SourceOrigin;
	}

	export interface CompileScriptOptions {
		origin?: SourceOrigin;
	}
}

export interface Agent extends backend.Agent {
	[Symbol.asyncDispose]: () => Promise<void>;
}

export const Agent: typeof backend.Agent = backend.Agent;

extend(Agent as unknown as Constructor<Agent>, {
	async [Symbol.asyncDispose](this: Agent) {
		await this.disposeAsync();
	},
});
