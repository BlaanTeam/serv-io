#include "line_reader.hpp"

LineReader::LineReader() {
	_search.init("\n", this);
}

LineReader::~LineReader() {}

void LineReader::onData(const char *data, size_t len) {
	_pending.append(data, len);
}

// Returns bytes consumed. Stops as soon as ONE complete line has been
// queued (or the buffer is exhausted) — the caller is then expected to
// drain the queued line and decide whether to keep feeding. This
// "feed-one" cadence lets callers (e.g. Request) check the parser state
// between lines and stop forwarding body bytes through the line scanner
// once headers are done.
size_t LineReader::feed(const char *buf, size_t len) {
	size_t i = 0;
	while (i < len) {
		const size_t taken = _search.feed(buf + i, len - i);
		i += taken;
		if (!_search.matched())
			break;

		// Strip a trailing CR — CRLF and bare LF normalize to the same line.
		if (!_pending.empty() && _pending[_pending.size() - 1] == '\r')
			_pending.erase(_pending.size() - 1);

		_ready.push_back(string());
		_ready.back().swap(_pending);
		_search.clearMatch();
		return i;
	}
	return i;
}

servio::Option<string> LineReader::takeLine() {
	if (_ready.empty())
		return servio::None<string>();
	string out;
	out.swap(_ready.front());
	_ready.pop_front();
	return servio::Some(out);
}

size_t LineReader::pendingSize() const {
	return _pending.size();
}

void LineReader::reset() {
	_search.reset();
	_pending.clear();
	_ready.clear();
}
