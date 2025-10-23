import type { CapabilityOrigin, SourceOrigin } from "./script.js";
import type { MaybeCompletionOf } from "backend_v8.node";
import * as ivm from "./backend.js";
import { compileModule, compileScript, createAgent, createCapability, createRealm } from "./backend.js";
import { Capability, Module } from "./module.js";
import { Realm } from "./realm.js";
import { Script } from "./script.js";
import { transformCompletion } from "./utility/completion.js";

export namespace Agent {
	export namespace CreateOptions {
		export type Clock = Clock.Deterministic | Clock.Microtask | Clock.Realtime | Clock.System;
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
	}

	export interface CreateCapabilityOptions {
		origin: CapabilityOrigin;
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

export class Agent {
	readonly #agent;

	/** @internal */
	private	constructor(agent: ivm.Agent) {
		this.#agent = agent;
	}

	static async create(options?: Agent.CreateOptions): Promise<Agent> {
		const agent = await createAgent(options);
		return new Agent(agent);
	}

	async createCapability(callback: (...args: unknown[]) => void, options: Agent.CreateCapabilityOptions): Promise<Capability> {
		const module = await createCapability(this.#agent, callback, options);
		return new Capability(module);
	}

	async createRealm(): Promise<Realm> {
		const realm = await createRealm(this.#agent);
		return new Realm(realm);
	}

	async compileModule(code: string, options?: Agent.CompileModuleOptions): Promise<Module> {
		const [ module, requests ] = await compileModule(this.#agent, code, options);
		return new Module(module, requests);
	}

	async compileScript(code: string, options?: Agent.CompileScriptOptions): Promise<MaybeCompletionOf<Script>> {
		const result = await compileScript(this.#agent, code, options);
		return transformCompletion(result, script => new Script(script));
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
