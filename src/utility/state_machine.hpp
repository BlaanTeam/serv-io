#ifndef SERVIO_STATE_MACHINE_HPP
#define SERVIO_STATE_MACHINE_HPP

namespace servio {

// Tiny generic state-machine helpers.
//
// Two flavors:
//   - `Phase<E>`   — single-valued state (enum / int). Use for the
//                    sub-state machines inside parsers (ChunkedBodyParser,
//                    MultipartBodyParser).
//   - `Flags<T>`   — bit-mask state. Use for protocol-level state that
//                    a request goes through (REQ_INIT/LINE/HEADER/BODY).
//
// Both are template-light wrappers — the goal is to give state mutation a
// vocabulary (transition, is, any, all, set, clear) instead of bare
// assignment and `&`-masking spread across the codebase. The compiler is
// fully capable of inlining everything; there's no overhead beyond the
// underlying integer.

// ----------------------------------------------------------- Phase<E> -----
//
// Wraps a single-valued state. `transition(to)` is the only mutator —
// reads are `current()` and `is(value)`. The point isn't safety (the
// underlying enum has no invariants we'd violate), it's making the
// transitions self-documenting at call sites:
//
//     phase.transition(ReadData);          // not: _phase = ReadData;
//     if (phase.is(ReadSizeLine)) { ... }  // not: if (_phase == ReadSizeLine)

template <typename E>
class Phase {
   public:
	explicit Phase(E initial) : _value(initial) {}

	E    current()    const { return _value; }
	bool is(E other)  const { return _value == other; }

	void transition(E to) { _value = to; }

   private:
	E _value;
};

// ------------------------------------------------------------ Flags<T> ----
//
// Bit-mask state. The underlying integer type is templated (typically
// `short`) so the wrapper stays the same size as the original member.
// Semantics:
//
//     enter(F)      add bit(s) F
//     leave(F)      remove bit(s) F
//     replace(F)    discard the current state, install F
//     is(F)         all bits of F are set
//     any(F)        at least one bit of F is set
//     raw()         expose the integer (rarely needed)

template <typename T>
class Flags {
   public:
	explicit Flags(T initial) : _bits(initial) {}

	void enter(T mask)   { _bits |= mask; }
	void leave(T mask)   { _bits &= ~mask; }
	void replace(T mask) { _bits = mask; }

	bool is(T mask)  const { return (_bits & mask) == mask; }
	bool any(T mask) const { return (_bits & mask) != 0; }
	T    raw()       const { return _bits; }

   private:
	T _bits;
};

}  // namespace servio

#endif
