[![npm version](https://badgen.now.sh/npm/v/@auto_js/napi)](https://www.npmjs.com/package/@auto_js/napi)
[![isc license](https://badgen.now.sh/npm/license/@auto_js/napi)](https://github.com/laverdet/isolated-vm/blob/experimental/LICENSE)
[![npm downloads](https://badgen.now.sh/npm/dm/@auto_js/napi)](https://www.npmjs.com/package/@auto_js/napi)

`@auto_js` -- Automatic C++ bindings for JavaScript
===================================================

<img align="right" width="33%" height="33%" src="https://github.com/laverdet/isolated-vm/raw/experimental/packages/auto_js/napi/upload/macro.jpg">

I made this C++ template thing that converts JavaScript functions and values automatically.

It requires at least clang 21 to build. I think gcc 16, when it is released, would also be able to
compile it. C++23 modules are used which requires a modern toolchain. It doesn't use reflection but
it does use C++26 variadic structured bindings which is only recently getting compiler support.

```c++
auto log(std::string message) -> void {
	std::print("{}", message);
}

js::napi::napi_js_module module_namespace{
	std::type_identity<environment>{},
	[](environment& env) -> auto {
		return std::tuple{
			std::in_place,
			std::pair{util::cw<"log">, js::free_function{log}},
		};
	}
};
```

## Example module

An example module is located at: [packages/auto_js/example](https://github.com/laverdet/isolated-vm/tree/experimental/packages/auto_js/example).

Instructions for building the demo on macOS:

```
# all this nonsense is just checking out the example subdirectory
git clone --branch=experimental --no-checkout --depth=1 https://github.com/laverdet/isolated-vm.git
cd isolated-vm
git sparse-checkout set packages/auto_js/example
git checkout
cd packages/auto_js/example

# install llvm, npm, configure, then build
brew install llvm
npm install
CXX=$(brew --prefix llvm)/bin/clang npm run configure
npm run build
node main.mjs
```


## It's better than what you're writing by hand

```c++
struct point {
		int x;
		int y;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"x">, &point::x},
			js::struct_member{util::cw<"y">, &point::y},
		};
};

auto take_points(std::vector<point> points) -> void {
	for (const auto& [ x, y ] : points) {
		std::print("({}, {})\n", x, y);
	}
}

constexpr auto string_literals = std::tuple{
	"x"sv,
	"y"sv,
};

class environment
		: public napi::environment,
			public napi::string_table<string_literals> {
	public:
		using napi::environment::environment;
};

js::napi::napi_js_module module_namespace{
	std::type_identity<environment>{},
	[](environment& env) -> auto {
		return std::tuple{
			std::in_place,
			std::pair{util::cw<"take_points">, js::free_function{take_points}},
		};
	}
};
```

`@auto_js/napi` compiles this in a way such that the "x" and "y" strings each get their own
`v8::Persistent<v8::String>` handles. Furthermore, for each invocation to the translation layer
those strings only need to dereference the persistent handle once, then they're cached in
`v8::Local<v8::String>` handles. The upshot is that your code is not hammering the v8 runtime with
unneeded work.


Runtimes
========

The core `@auto_js/js` module is runtime-agnostic. It describes *JavaScript* values in an abstract
way. A bunch of generic support code then provides the translation. The same support code which
translates an array of JavaScript arguments to C++ can be used to implement the structured clone
algorithm with a one-liner.

Modules `@auto_js/napi` and `@auto_js/v8` are provided which implement runtime support. The Napi
implementation is more complete than v8.


Transferable Types
==================

## Integrals

`bool` and `double` transfer exactly how you would expect.

`int32_t` and `uint32_t` will be coerced from the internal JavaScript `double` type. If the user
passes a value that is out of range an error will be thrown.


## Strings

Strings are interesting. Internally, JavaScript specifies that strings are UTF-16. Runtimes
typically also provide an optimized "one byte" representation for ASCII strings. On the C++ side you
probably want UTF-8. So that's at least three types of strings you'll want to know how to handle.

You can accept `std::u16string` which will always work. You can also accept `std::u8string` which
will automatically interpolate the string from UTF-16 to UTF-8. Another option is `std::string`
which would be an optimized "one byte" ASCII string. This string type will throw if a string
contains non-ASCII data.


## Bigints

You can accept `int64_t` or `uint64_t` for bigint types. These will throw if the bigint cannot be
coerced to the given C++ type. A utility class `js::bigint` is provided which can losslessly
represent any JavaScript bigint. Mathematical operations are not provided.


## Dates

A `std::chrono`-compatible `js::js_clock` is provided. This losslessly represents a JavaScript date
using the same `double` value that the runtime uses. It provides `std::chrono::clock_cast` for
casting to other C++ clock ranges.


## Optionals

`std::optional<T>` can be used on top of any other type to accept values of `undefined`.


## Enumerations

Enumerated values can be represented as strings in JavaScript by declaring those strings statically in C++.

```c++
enum class enum_test : std::int8_t {
	first,
	second,
	third,
};

namespace js {
template <>
struct enum_values<enum_test> {
		constexpr static auto values = std::array{
			std::pair{"first", enum_test::first},
			std::pair{"second", enum_test::second},
			std::pair{"third", enum_test::third}
		};
};
}
```


## Variants

`std::variant<std::u16string, double>` could be used to accept *either* a string or a number value.
You can mix and match types as needed.


## Covariants

`std::variant<double, int32_t>` can be used to accept *either* a floating point or a signed integer.
Note that this uses runtime function calls to resolve the covariance. In v8 it will invoke
`value->IsInt32()` to discover what to return. Napi does not support this, so a `double` will always
be returned.

`std::variant<std::string, std::u16string>` can be used in a similar way to accept either a UTF-16
string or the optimized ASCII string. This uses `value->IsOneByte()` to resolve the covariance.
Again, in napi a UTF-16 string will always be returned.


## Structures

`struct` & `class` types can be defined with either direct member access, or accessor functions.
Structures defined in this way are completely transferred to/from JavaScript. This isn't the "object
wrap" thing where a C++ value lives in JavaScript.

Direct member access:
```c++
struct point {
		int x;
		int y;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"x">, &point::x},
			js::struct_member{util::cw<"y">, &point::y},
		};
};
```

Getter & setter:
```c++
struct point {
		auto get_x() const -> int;
		auto set_x(int x) const -> void;

		constexpr static auto struct_template = js::struct_template{
			// Getter and setter are both optional. Pass `nullptr` to define a type without a
			// getter/setter. If an accessor is missing the type would only be transferable
			// one way, for example a type missing a setter could only be `return`ed and not
			// accepted as a function parameter.
			js::struct_accessor{util::cw<"x">, util::fn<&point::get_x>, util::fn<&point::set_x>},
			js::struct_member{util::cw<"y">, &point::y},
		};
};
```

## Discriminated Unions

TypeScript-style [discriminated
unions](https://www.typescriptlang.org/docs/handbook/2/everyday-types.html#union-types) can be
defined as a `std::variant` with a discriminant.

The following would accept either something like `{ type: "bear", honey: 1 }` or
`{ type: "horse", hay: 1 }`

```c++
struct bear; // imagine it defines: honey
struct horse; // imagine it defines: hay

namespace js {
template <>
struct union_of<std::variant<bear, horse>> {
		constexpr static auto& discriminant = util::cw<"type">;
		constexpr static auto alternatives = std::tuple{
			alternative<bear>{"bear"},
			alternative<horse>{"horse"},
		};
};
}
```


## Arrays

`std::vector<T>` will accept a JavaScript array of the given type.


## Classes

"Object wrap" Napi functions are supported.

```c++
export class database {
	public:
		using transfer_type = js::tagged_external<database>;

		auto query(environment& env, std::string query) -> std::string;
		static auto connect(environment& env, std::string uri) -> js::forward<js::napi::value<>>;
		static auto class_template(environment& env) -> js::napi::value<class_tag_of<database>> {
			// incomplete
		}
};
```
