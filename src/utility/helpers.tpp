#include "helpers.hpp"

template <typename T, typename Func>
bool every(T data, Func _func) {
	typedef typename T::iterator Iter;

	for (Iter it = data.begin(); it != data.end(); it++)
		if (!_func(*it)) return false;
	return true;
}

template <typename Iter, typename Func>
bool every(Iter begin, Iter end, Func _func) {
	for (Iter it = begin; it != end; it++)
		if (!_func(*it)) return false;
	return true;
}