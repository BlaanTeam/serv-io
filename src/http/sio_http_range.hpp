#ifndef __HTTP_RANGE_H__
#define __HTTP_RANGE_H__

#include <sstream>
#include <string>
#include <vector>

#include "utility/sio_helpers.hpp"

using namespace std;

enum UnitType {
	NON = (1 << 0),
	NOF = (1 << 1),
	NOL = (1 << 2)
};

struct RangeSpecifier {
	size_t   rangeStart;
	size_t   rangeEnd;
	UnitType type;

	size_t getContentLength(iostream *stream);
	size_t getContentLength(size_t fileSize);
	void   setupSeek(iostream *stream);
};

class Range {
	bool _valid;

	vector<RangeSpecifier> rangeSpecifiers;

   private:
	RangeSpecifier parse(const string &value);

   public:
	Range();
	Range(const string &value);

	// Getters
	bool                   valid(void) const;
	vector<RangeSpecifier> getRangeSpecifiers(void) const;
};

#endif