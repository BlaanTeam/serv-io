#include "framework.hpp"
#include "utility/state_machine.hpp"

using servio::Flags;
using servio::Phase;

namespace {
enum ChunkPhase {
	ReadSize,
	ReadData,
	ReadCRLF,
	Done
};

enum ReqFlag {
	INIT   = 1 << 0,
	LINE   = 1 << 1,
	HEADER = 1 << 2,
	BODY   = 1 << 3,
	DONE   = 1 << 4,
	INVALID = 1 << 5
};
}  // namespace

// ----------------------------------------------------------- Phase<E> -----

TEST(Phase, startsAtInitial) {
	Phase<ChunkPhase> p(ReadSize);
	ASSERT_TRUE(p.is(ReadSize));
	ASSERT_FALSE(p.is(ReadData));
	ASSERT_EQ(p.current(), ReadSize);
}

TEST(Phase, transitionUpdatesCurrent) {
	Phase<ChunkPhase> p(ReadSize);
	p.transition(ReadData);
	ASSERT_TRUE(p.is(ReadData));
	ASSERT_EQ(p.current(), ReadData);
}

TEST(Phase, isIsExclusive) {
	Phase<ChunkPhase> p(ReadCRLF);
	ASSERT_TRUE(p.is(ReadCRLF));
	ASSERT_FALSE(p.is(ReadSize));
	ASSERT_FALSE(p.is(ReadData));
	ASSERT_FALSE(p.is(Done));
}

// ------------------------------------------------------------- Flags<T> ---

TEST(Flags, enterAddsBits) {
	Flags<int> s(INIT);
	ASSERT_TRUE(s.is(INIT));
	s.enter(LINE);
	ASSERT_TRUE(s.any(INIT));
	ASSERT_TRUE(s.any(LINE));
}

TEST(Flags, leaveRemovesBits) {
	Flags<int> s(INIT | LINE);
	s.leave(LINE);
	ASSERT_TRUE(s.any(INIT));
	ASSERT_FALSE(s.any(LINE));
}

TEST(Flags, replaceDiscardsAll) {
	Flags<int> s(INIT | LINE | HEADER);
	s.replace(BODY);
	ASSERT_FALSE(s.any(INIT));
	ASSERT_FALSE(s.any(LINE));
	ASSERT_FALSE(s.any(HEADER));
	ASSERT_TRUE(s.any(BODY));
}

TEST(Flags, isRequiresAllBits) {
	Flags<int> s(INIT | LINE);
	ASSERT_TRUE(s.is(INIT));
	ASSERT_TRUE(s.is(LINE));
	ASSERT_TRUE(s.is(INIT | LINE));
	ASSERT_FALSE(s.is(INIT | BODY));   // BODY not set
}

TEST(Flags, anyMatchesAtLeastOne) {
	Flags<int> s(HEADER);
	ASSERT_TRUE(s.any(BODY | HEADER | DONE));
	ASSERT_FALSE(s.any(BODY | DONE));
}

TEST(Flags, rawReturnsRawBits) {
	Flags<short> s(short(INIT | BODY));
	ASSERT_EQ((int)s.raw(), INIT | BODY);
}
