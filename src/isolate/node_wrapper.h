// node.h has some deprecated v8 calls which blow up with warnings
#pragma once
#ifdef __clang__
#pragma clang system_header
#include <node.h>
#include <node_buffer.h>
#elif __GNUC__
#pragma GCC system_header
#include <node.h>
#include <node_buffer.h>
#elif _MSC_VER
#pragma warning(push, 0)
#include <node.h>
#include <node_buffer.h>
#pragma warning(pop)
#else
#include <node.h>
#include <node_buffer.h>
#endif
