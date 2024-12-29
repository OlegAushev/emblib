#pragma once

#include <algorithm>
#include <emblib/core.hpp>
#include <utility>

namespace emb {

template<class T>
inline const T& clamp(const T& value, const T& lo, const T& hi) {
    return (value < lo) ? lo : (hi < value) ? hi : value;
}

template<class T>
inline T median_of_three(T a, T b, T c) {
    if (a > c) {
        std::swap(a, c);
    }
    if (a > b) {
        std::swap(a, b);
    }
    if (b > c) {
        std::swap(b, c);
    }
    return b;
}

#if __cplusplus < 199700

template<class It, class T>
inline It find(It first, It last, const T& value) {
    for (; first != last; ++first) {
        if (*first == value) {
            return first;
        }
    }
    return last;
}

template<class It, class T>
inline It binary_find(It first, It last, const T& value) {
    It not_found = last;

    while (first < last) {
        It mid = first + (last - first) / 2;

        if (value == *mid) {
            return mid;
        }

        if (value < *mid) {
            last = mid;
        } else {
            first = mid + 1;
        }
    }

    return not_found;
}

template<class It, class T>
inline void fill(It first, It last, const T& value) {
    for (; first != last; ++first) {
        *first = value;
    }
}

template<class It, class T>
inline int count(It first, It last, const T& value) {
    int ret = 0;
    for (; first != last; ++first) {
        if (*first == value) {
            ++ret;
        }
    }
    return ret;
}

template<class InputIt, class OutputIt>
inline OutputIt copy(InputIt first, InputIt last, OutputIt d_first) {
    for (; first != last; ++first, ++d_first) {
        *d_first = *first;
    }
    return d_first;
}

template<class It>
inline bool equal(It first1, It last1, It first2) {
    for (; first1 != last1; ++first1, ++first2) {
        if (!(*first1 == *first2)) {
            return false;
        }
    }
    return true;
}

template<class It>
inline It max_element(It first, It last) {
    if (first == last) {
        return last;
    }

    It largest = first;
    ++first;
    for (; first != last; ++first) {
        if (*largest < *first) {
            largest = first;
        }
    }
    return largest;
}

template<class It>
inline It min_element(It first, It last) {
    if (first == last) {
        return last;
    }

    It smallest = first;
    ++first;
    for (; first != last; ++first) {
        if (*first < *smallest) {
            smallest = first;
        }
    }
    return smallest;
}

template<class It>
inline std::pair<It, It> minmax_element(It first, It last) {
    It min = first, max = first;

    if ((first == last) || (++first == last)) {
        return std::pair<It, It>(min, max);
    }

    if (*first < *min) {
        min = first;
    } else {
        max = first;
    }

    while (++first != last) {
        It i = first;
        if (++first == last) {
            if (*i < *min) {
                min = i;
            } else if (!(*i < *max)) {
                max = i;
            }
            break;
        } else {
            if (*first < *i) {
                if (*first < *min) {
                    min = first;
                }
                if (!(*i < *max)) {
                    max = i;
                }
            } else {
                if (*i < *min) {
                    min = i;
                }
                if (!(*first < *max)) {
                    max = first;
                }
            }
        }
    }
    return std::pair<It, It>(min, max);
}

#endif

} // namespace emb
