// npm run clean; CXX=/usr/lib/llvm-22/bin/clang++ cmake "-DCMAKE_MODULE_PATH=$(npm exec auto_js_cmake_include)" -DCMAKE_BUILD_TYPE=Debug -G Ninja -B build; npm run build
import auto_js;
import isolated_vm;
import std;
import util;
using namespace js;
using namespace isolated_vm;

isolated_vm::addon module_namespace{
	std::type_identity<std::monostate>{},
	[]() -> auto {
		// auto local = js::transfer_in<value_of<number_tag>>(123, env);
		// auto native = js::transfer_out<double>(local, env);
		// std::print("hello world {}\n", native);
		return std::tuple{
			std::pair{util::cw<"one">, 0},
			std::pair{util::cw<"two">, 0},
		};
	}
};
