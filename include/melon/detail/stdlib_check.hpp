#pragma once

// <version> defines the __cpp_lib_* feature-test macros and, on libstdc++,
// _GLIBCXX_RELEASE. It has to be included before anything can be checked.
#include <version>

// A C++23-capable compiler does not imply a C++23 standard library. Clang 18
// against libstdc++ 12 accepts -std=c++23 and then fails deep inside melon
// with hundreds of lines of template diagnostics, because std::views::zip and
// std::format are missing. These checks turn that into a single readable
// error.
//
// This cannot be expressed in the Conan recipe: its compiler.libcxx setting
// selects the ABI (libstdc++ vs libstdc++11), never the release, so for clang
// the standard library version is invisible to the package manager.

#if defined(__GLIBCXX__)

// The GCC 14 baseline melon documents is really a libstdc++ 14 baseline: the
// same requirement applies to clang built against libstdc++.
//
// To fix: use GCC 14 or newer, or point clang at a GCC 14+ toolchain --
// install libstdc++-14-dev, and if an older GCC is also present select it
// explicitly with --gcc-install-dir= (clang 18+) or --gcc-toolchain=.
#if !defined(_GLIBCXX_RELEASE) || _GLIBCXX_RELEASE < 14
#error "melon requires libstdc++ 14 or newer -- see stdlib_check.hpp"
#endif

#elif defined(_LIBCPP_VERSION)

// libc++ defines __cpp_lib_ranges_zip only from release 22, when the last
// piece of P2321R2 landed, although std::views::zip itself is far older;
// checking the macro would reject every Apple toolchain to date. The floor is
// the release instead: 20 is the first whose std::jthread and std::stop_token
// (knapsack_bnb) are not behind -fexperimental-library.
#if _LIBCPP_VERSION < 200000
#error "melon requires libc++ 20 or newer -- see stdlib_check.hpp"
#endif

#else

// The MSVC STL publishes no release macro comparable to _GLIBCXX_RELEASE, so
// rather than invent a version floor, require the features themselves.
#if !defined(__cpp_lib_format)
#error "melon requires std::format (__cpp_lib_format)."
#endif
#if !defined(__cpp_lib_ranges_zip)
#error "melon requires std::views::zip (__cpp_lib_ranges_zip)."
#endif

#endif
