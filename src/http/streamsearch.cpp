#include "streamsearch.hpp"

#include <cstring>

StreamSearch::Sink::~Sink() {}

StreamSearch::StreamSearch() : _matchPos(0), _matched(false), _sink(nullptr) {}

StreamSearch::~StreamSearch() {}

void StreamSearch::init(const string& needle, Sink* sink) {
	_needle = needle;
	_sink = sink;
	_matchPos = 0;
	_matched = false;
}

void StreamSearch::reset() {
	_matchPos = 0;
	_matched = false;
}

bool StreamSearch::matched() const {
	return _matched;
}

void StreamSearch::clearMatch() {
	_matched = false;
}

const string& StreamSearch::needle() const {
	return _needle;
}

size_t StreamSearch::feed(const char* buf, size_t len) {
	if (_matched || !_sink || _needle.empty() || len == 0)
		return 0;

	size_t consumed = 0;

	// Slow path: a partial match from a previous chunk carries over. The actual
	// bytes of that prefix are, by construction, _needle[0.._matchPos), so we
	// rebuild the conceptual scan window and process it as a single buffer.
	if (_matchPos > 0) {
		const size_t carry = _matchPos;
		vector<char> tmp;
		tmp.reserve(carry + len);
		tmp.insert(tmp.end(), _needle.data(), _needle.data() + carry);
		tmp.insert(tmp.end(), buf, buf + len);
		_matchPos = 0;

		size_t tmpConsumed = 0;
		scan(tmp.empty() ? nullptr : &tmp[0], tmp.size(), tmpConsumed);

		// Translate consumed back to caller-visible bytes.
		if (tmpConsumed <= carry) {
			// The "consumed" prefix is entirely within the carried-over bytes —
			// can happen when the carried prefix forms a rewind. The remaining
			// (carry - tmpConsumed) bytes get folded back into _matchPos by the
			// scan() tail logic, so nothing of `buf` was consumed yet.
			return 0;
		}
		consumed = tmpConsumed - carry;
		return consumed;
	}

	scan(buf, len, consumed);
	return consumed;
}

// Scans `buf[0..len)` byte-by-byte against the needle. Bytes confirmed not
// part of any (full or partial) needle match are flushed via `_sink->onData`.
// On a full match, `_matched` is set and `consumed` points just past the
// needle. Otherwise, the residual partial-match prefix is captured in
// `_matchPos` (the bytes themselves are not stored — they are needle[0..n)).
void StreamSearch::scan(const char* buf, size_t len, size_t& consumed) {
	const size_t n = _needle.size();
	const char*  needle = _needle.data();

	size_t i = 0;
	size_t matchStart = 0;
	size_t flushFrom = 0;
	size_t mp = 0;

	while (i < len) {
		if (buf[i] == needle[mp]) {
			if (mp == 0) matchStart = i;
			mp++;
			i++;
			if (mp == n) {
				if (matchStart > flushFrom)
					_sink->onData(buf + flushFrom, matchStart - flushFrom);
				_matched = true;
				consumed = i;
				return;
			}
		} else if (mp > 0) {
			// Rewind one byte past the failed match start. Bytes from flushFrom
			// up to (matchStart) are confirmed non-needle and could be flushed,
			// but we deliberately wait until the end of the scan to flush in
			// bulk — preserves at most one onData call per chunk.
			i = matchStart + 1;
			mp = 0;
		} else {
			i++;
		}
	}

	const size_t safeEnd = (mp > 0) ? matchStart : i;
	if (safeEnd > flushFrom)
		_sink->onData(buf + flushFrom, safeEnd - flushFrom);

	_matchPos = mp;
	consumed = i;
}
