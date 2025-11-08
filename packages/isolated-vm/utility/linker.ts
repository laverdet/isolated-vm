import type { AbstractModule, Agent, ModuleLinker } from "isolated-vm";
import * as fs from "node:fs/promises";
import { mappedComparator, selectKey, stringComparator } from "./comparator.js";

type ImportResolve = (specifier: string, parent?: string | URL) => string;

function cacheKeyFor(specifier: string, attributes: ImportAttributes | undefined): string {
	const sortedAttributes = Object.entries(attributes ?? {}).sort(mappedComparator(stringComparator, selectKey));
	return `${specifier}#${JSON.stringify(sortedAttributes)}`;
}

export function makeCachedLinker(linker: ModuleLinker): ModuleLinker {
	const modules = new Map<string, ReturnType<ModuleLinker>>();
	return (specifier, origin, attributes) => {
		const cacheKey = cacheKeyFor(specifier, attributes);
		const cached = modules.get(cacheKey);
		if (cached) {
			return cached;
		}
		const module = function() {
			try {
				return linker(specifier, origin, attributes);
			} catch (error) {
				// eslint-disable-next-line @typescript-eslint/prefer-promise-reject-errors
				return Promise.reject(error);
			}
		}();
		modules.set(cacheKey, module);
		return module;
	};
}

export function makeCompositeLinker(...linkers: ModuleLinker[]): ModuleLinker {
	return async (specifier, origin, attributes) => {
		for (const linker of linkers) {
			const module = await linker(specifier, origin, attributes);
			if (module) {
				return module;
			}
		}
	};
}

export function makeFileSystemCompilationLinker(agent: Agent, resolve: ImportResolve): ModuleLinker {
	return makeCachedLinker(async (specifier, parentName) => {
		const path = function() {
			try {
				return resolve(specifier, parentName);
			} catch {}
		}();
		if (path !== undefined) {
			const origin = { name: path };
			const sourceText = await fs.readFile(new URL(path), "utf8");
			const result = await agent.compileModule(sourceText, { origin });
			if (!result?.complete) {
				throw new SyntaxError(`Module compilation incomplete for ${specifier} at ${path}`, { cause: result?.error });
			}
			return result.result;
		}
	});
}

export function makePreloadedLinker(preloadedModules: Iterable<[ string, AbstractModule ]>): ModuleLinker {
	const modules = new Map(preloadedModules);
	return specifier => modules.get(specifier);
}
