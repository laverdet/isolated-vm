import type { SourceOrigin } from "./script.js";
import * as backend from "#backend_v8";
import { Module } from "./module.js";

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
		randomSeed?: number | undefined;
	}

	export interface CompileModuleOptions {
		origin?: SourceOrigin;
	}

	export interface CompileScriptOptions {
		origin?: SourceOrigin;
	}
}

let didInitialize = false;

export class Agent extends backend.Agent {
	static override create(options?: Agent.CreateOptions): Promise<Agent> {
		if (!didInitialize) {
			backend.initialize({ Agent, Module });
			didInitialize	= true;
		}
		return backend.Agent.create(options);
	}

	// eslint-disable-next-line @typescript-eslint/require-await
	async dispose(): Promise<boolean> {
		// :)
		return true;
	}

	async [Symbol.asyncDispose](): Promise<void> {
		await this.dispose();
	}
}
