import type { SourceOrigin } from "./script.js";
import type { MaybeCompletionOf } from "isolated-vm/utility/completion";
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

export class Agent extends backend.Agent {
	static create(options?: Agent.CreateOptions): Promise<Agent> {
		return backend.Agent._create(Agent, options);
	}

	async compileModule(code: string, options?: Agent.CompileModuleOptions): Promise<MaybeCompletionOf<Module>> {
		return this._compileModule(Module, code, options);
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
