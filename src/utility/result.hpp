#ifndef SERVIO_RESULT_HPP
#define SERVIO_RESULT_HPP

// Rust-inspired Option<T> and Result<T, E> for C++98.
//
// These are deliberately small and don't try to match the Rust standard
// library exactly — they exist to make ServIO's error-handling sites read
// like:
//
//     Option<string> Request::normalizedPath() const;
//
//     Result<Boundary, string> Boundary::parse(const string& headerValue);
//
//     if (Result<File, string> r = openFile(path); r.isOk()) { ... }
//
// instead of returning `pair<bool, T>` or "valid()" sentinel objects.
//
// Constraints stemming from C++98 (no move, no variadic templates):
//   - T (and E) must be default-constructible, copy-constructible, copy-
//     assignable. Holding pointer or value types is fine; references aren't.
//   - Both T and E are stored unconditionally inside Result (no tagged union)
//     to keep the implementation under fifty lines. The wasted bytes are
//     never noticeable for the small types we use here.

#include <cstdlib>
#include <iostream>
#include <string>

namespace servio {

// ----------------------------------------------------------- Option<T> -----

template <typename T>
class Option {
   public:
	Option() : _has(false), _value() {}

	bool isSome() const { return _has; }
	bool isNone() const { return !_has; }

	// Aborts the process on None. Use unwrapOr() for fallible call sites.
	const T &unwrap() const {
		if (!_has) panic("Option::unwrap() called on None");
		return _value;
	}

	T unwrapOr(const T &fallback) const {
		return _has ? _value : fallback;
	}

	// Map the inner value if present. The callable F takes `const T&` and
	// returns `U` (any default-constructible, copyable type).
	template <typename U, typename F>
	Option<U> map(F f) const {
		if (!_has) return Option<U>();
		return Option<U>::some(f(_value));
	}

	// Chain a fallible continuation. F takes `const T&` and returns
	// `Option<U>`.
	template <typename U, typename F>
	Option<U> andThen(F f) const {
		if (!_has) return Option<U>();
		return f(_value);
	}

	// Factory functions (constructor disambiguation in C++98 is annoying).
	static Option some(const T &v) {
		Option o;
		o._has = true;
		o._value = v;
		return o;
	}
	static Option none() { return Option(); }

   private:
	bool _has;
	T    _value;

	static void panic(const char *msg) {
		std::cerr << "panic: " << msg << std::endl;
		std::abort();
	}
};

// Free constructors so call sites can read `Some(x)` / `None<int>()`.
template <typename T>
inline Option<T> Some(const T &v) {
	return Option<T>::some(v);
}

template <typename T>
inline Option<T> None() {
	return Option<T>::none();
}

// ----------------------------------------------------------- Result<T, E> --

template <typename T, typename E>
class Result {
   public:
	bool isOk()  const { return _ok; }
	bool isErr() const { return !_ok; }

	const T &unwrap() const {
		if (!_ok) panic("Result::unwrap() called on Err");
		return _value;
	}

	const E &unwrapErr() const {
		if (_ok) panic("Result::unwrapErr() called on Ok");
		return _error;
	}

	T unwrapOr(const T &fallback) const {
		return _ok ? _value : fallback;
	}

	Option<T> ok() const {
		return _ok ? Some(_value) : None<T>();
	}

	Option<E> err() const {
		return _ok ? None<E>() : Some(_error);
	}

	template <typename U, typename F>
	Result<U, E> map(F f) const {
		if (!_ok) return Result<U, E>::err(_error);
		return Result<U, E>::ok(f(_value));
	}

	template <typename F, typename G>
	Result<T, F> mapErr(G g) const {
		if (_ok) return Result<T, F>::ok(_value);
		return Result<T, F>::err(g(_error));
	}

	template <typename U, typename F>
	Result<U, E> andThen(F f) const {
		if (!_ok) return Result<U, E>::err(_error);
		return f(_value);
	}

	// Factory functions.
	static Result ok(const T &v) {
		Result r;
		r._ok = true;
		r._value = v;
		return r;
	}
	static Result err(const E &e) {
		Result r;
		r._ok = false;
		r._error = e;
		return r;
	}

	Result() : _ok(false), _value(), _error() {}

   private:
	bool _ok;
	T    _value;
	E    _error;

	static void panic(const char *msg) {
		std::cerr << "panic: " << msg << std::endl;
		std::abort();
	}
};

// Free constructors. Result needs the type explicit at the call site (no
// CTAD in C++98), so we expose factory templates instead of free functions
// with the Rust-style `Ok(x)` / `Err(e)` naming.
//
//     Result<int, std::string>::ok(42);
//     Result<int, std::string>::err("oops");

}  // namespace servio

#endif
