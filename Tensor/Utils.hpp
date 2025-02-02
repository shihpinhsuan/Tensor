//
//  Utils.hpp
//  Tensor
//
//  Created by 陳均豪 on 2022/1/29.
//

#ifndef Utils_hpp
#define Utils_hpp

#include "MaybeOwned.hpp"
#include "ArrayRef.hpp"
#include "SmallVector.hpp"

namespace otter {

constexpr size_t kDimVectorStaticSize = 5;
using DimVector = SmallVector<int64_t, kDimVectorStaticSize>;

// Structure: Range package
// Description: Convenience for packing the looping range
struct Range {
    Range(int64_t begin, int64_t end) : begin(begin), end(end) {}
    
    int64_t size() const { return end - begin; }
    
    Range operator/(int64_t divisor) {
        return Range(begin / divisor, end / divisor);
    }
    
    int64_t begin;
    int64_t end;
};

std::ostream& operator<<(std::ostream& out, const Range& range);
// End Range package


// Structure: Integer Iterator
// Description: Convenience for using for loop
//
// Usage:
// for (auto i : irange(begin, end)) {
//     do something
// }
//
// for (auto i : irange(end)) {
//     do something
// }
template < typename I, typename std::enable_if<std::is_integral<I>::value, int>::type = 0>
struct integer_iterator : std::iterator<std::input_iterator_tag, I> {
    explicit integer_iterator(I value) : value(value) {}
    
    I operator*() const {
        return value;
    }
    
    I const* operator->() const {
        return &value;
    }
    
    integer_iterator& operator++() {
        ++value;
        return *this;
    }
    
    integer_iterator operator++(int) {
        const auto copy = *this;
        ++*this;
        return copy;
    }
    
    bool operator==(const integer_iterator& other) const {
        return value == other.value;
    }
    
    bool operator!=(const integer_iterator& other) const {
        return value != other.value;
    }
    
protected:
    I value;
};

template <typename I, typename std::enable_if<std::is_integral<I>::value, bool>::type = true>
struct integer_range {
public:
    integer_range(I begin, I end) : begin_(begin), end_(end) {}
    integer_iterator<I> begin() const {
        return begin_;
    }
    integer_iterator<I> end() const {
        return end_;
    }
    
private:
    integer_iterator<I> begin_;
    integer_iterator<I> end_;
};

template <typename Integer1, typename Integer2, typename std::enable_if<std::is_integral<Integer1>::value, bool>::type = true, typename std::enable_if<std::is_integral<Integer2>::value, bool>::type = true>
integer_range<Integer2> irange(Integer1 begin, Integer2 end) {
  return { static_cast<Integer2>(begin), std::max(static_cast<Integer2>(begin), end) };
}

template <typename Integer, typename std::enable_if<std::is_integral<Integer>::value, bool>::type = true>
integer_range<Integer> irange(Integer end) {
  return { Integer(), std::max(Integer(), end) };
}
// End Iterger Iterator

template <typename T>
inline T data_index_init(T offset) {
    return offset;
}

template <typename T, typename... Args>
inline T data_index_init(T offset, T& x, const T& X, Args&&... args) {
    offset = data_index_init(offset, std::forward<Args>(args)...);
    x = offset % X;
    return offset / X;
}

inline bool data_index_step() {
    return true;
}

template <typename T, typename... Args>
inline bool data_index_step(T& x, const T& X, Args&&... args) {
    if (data_index_step(std::forward<Args>(args)...)) {
        x = ((x + 1) == X) ? 0 : (x + 1);
        return x == 0;
    }
    return false;
}

}   // end namespace otter

#endif /* Utils_hpp */
