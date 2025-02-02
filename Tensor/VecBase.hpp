//
//  VecBase.hpp
//  Tensor
//
//  Created by 陳均豪 on 2022/2/16.
//

#ifndef VecBase_hpp
#define VecBase_hpp

#include <cstring>
#include <functional>
#include <cmath>
#include <type_traits>
#include <bitset>

#include "Utils.hpp"
#include "Macro.hpp"
#include "Math.hpp"
#include "VecIntrinsic.hpp"
#include "TypeCast.hpp"

// These macros helped us unify vec_base.h
#ifdef CPU_CAPABILITY_AVX512
#if defined(__GNUC__)
#define __otter_align__ __attribute__((aligned(64)))
#elif defined(_WIN32)
#define __otter_align__ __declspec(align(64))
#else
#define __otter_align__
#endif
#define VECTOR_WIDTH 64
#define int_vector __m512i
#else // CPU_CAPABILITY_AVX512
#if defined(__GNUC__)
#define __otter_align__ __attribute__((aligned(32)))
#elif defined(_WIN32)
#define __otter_align__ __declspec(align(32))
#else
#define __otter_align__
#endif
#define VECTOR_WIDTH 32
#define int_vector __m256i
#endif // CPU_CAPABILITY_AVX512

namespace otter {
namespace vec {

template <typename T>
struct is_floating_point:
std::integral_constant<bool, std::is_floating_point<T>::value> {
};

template<size_t n> struct int_of_size;

#define DEFINE_INT_OF_SIZE(int_t) \
template<> struct int_of_size<sizeof(int_t)> { using type = int_t; }

DEFINE_INT_OF_SIZE(int64_t);
DEFINE_INT_OF_SIZE(int32_t);
DEFINE_INT_OF_SIZE(int16_t);
DEFINE_INT_OF_SIZE(int8_t);

#undef DEFINE_INT_OF_SIZE

template <typename T>
using int_same_size_t = typename int_of_size<sizeof(T)>::type;

template <class T>
struct Vectorized {
private:
    __otter_align__ T values[VECTOR_WIDTH / sizeof(T)];
public:
    using value_type = T;
    using size_type = int;
    
    static constexpr size_type size() {
        return VECTOR_WIDTH / sizeof(T);
    }
    
    Vectorized() : values{0} {}
    
    Vectorized(T val) {
        for (int i = 0; i != size(); i++) {
            values[i] = val;
        }
    }
    
    template<typename... Args, typename = std::enable_if_t<(sizeof...(Args) == size())>>
    Vectorized(Args... vals) : values{vals...} {}
    
    inline operator const T*() const {
        return values;
    }
    
    inline operator T*() {
        return values;
    }
    
    auto as_bytes() const -> const char* {
        return reinterpret_cast<const char*>(values);
    }
    
    template <int64_t mask_>
    static Vectorized<T> blend(const Vectorized<T>& a, const Vectorized<T>& b) {
        int64_t mask = mask_;
        Vectorized vector;
        for (const auto i : otter::irange(size())) {
            if (mask & 0x01) {
                vector[i] = b[i];
            } else {
                vector[i] = a[i];
            }
            mask = mask >> 1;
        }
        return vector;
    }
    
    static Vectorized<T> blendv(const Vectorized<T>& a, const Vectorized<T>& b, const Vectorized<T>& mask) {
        Vectorized vector;
        int_same_size_t<T> buffer[size()];
        mask.store(buffer);
        for (const auto i : otter::irange(size())) {
            if (buffer[i] & 0x01) {
                vector[i] = b[i];
            } else {
                vector[i] = a[i];
            }
        }
        return vector;
    }
    
    static Vectorized<T> loadu(const void* ptr) {
        Vectorized vector;
        std::memcpy(vector.values, ptr, VECTOR_WIDTH);
        return vector;
    }
    
    static Vectorized<T> loadu(const void* ptr, int64_t count) {
        Vectorized vector;
        std::memcpy(vector.values, ptr, count * sizeof(T));
        return vector;
    }
    
    void store(void* ptr, int count = size()) const {
        std::memcpy(ptr, values, count * sizeof(T));
    }
    
    Vectorized<T> map(T (*const f)(T)) const {
        Vectorized<T> ret;
        for (int64_t i = 0; i != size(); i++) {
            ret[i] = f(values[i]);
        }
        return ret;
    }
    Vectorized<T> map(T (*const f)(const T &)) const {
        Vectorized<T> ret;
        for (int64_t i = 0; i != size(); i++) {
            ret[i] = f(values[i]);
        }
        return ret;
    }
    
    Vectorized<T> isnan() const {
        Vectorized<T> vector;
        for (int64_t i = 0; i != size(); i++) {
            if (std::isnan(values[i])) {
                std::memset(static_cast<void*>(vector.values + i), 0xFF, sizeof(T));
            } else {
                std::memset(static_cast<void*>(vector.values + i), 0, sizeof(T));
            }
        }
        return vector;
    }
    
    template <typename other_t_abs = T, typename std::enable_if<!is_floating_point<other_t_abs>::value, int>::type = 0>
    Vectorized<T> abs() const {
        // other_t_abs is for SFINAE and clarity. Make sure it is not changed.
        static_assert(std::is_same<other_t_abs, T>::value, "other_t_abs must be T");
        return map([](T x) -> T { return x < static_cast<T>(0) ? -x : x; });
    }
    
    template <typename float_t_abs = T, typename std::enable_if<is_floating_point<float_t_abs>::value, int>::type = 0>
    Vectorized<T> abs() const {
        // float_t_abs is for SFINAE and clarity. Make sure it is not changed.
        static_assert(std::is_same<float_t_abs, T>::value, "float_t_abs must be T");
        // Specifically deal with floating-point because the generic code above won't handle -0.0 (which should result in
        // 0.0) properly.
        return map([](T x) -> T { return std::abs(x); });
    }
    
    Vectorized<T> neg() const {
        // NB: the trailing return type is needed because we need to coerce the
        // return value back to T in the case of unary operator- incuring a
        // promotion
        return map([](T x) -> T { return -x; });
    }
    
    Vectorized<T> exp() const {
        return map([](T x) -> T { return std::exp(x); });
    }
    
    Vectorized<T> sin() const {
        return map([](T x) -> T { return std::sin(x); });
    }
    Vectorized<T> sinh() const {
        return map([](T x) -> T { return std::sinh(x); });
    }
    Vectorized<T> cos() const {
        return map([](T x) -> T { return std::cos(x); });
    }
    Vectorized<T> cosh() const {
        return map([](T x) -> T { return std::cosh(x); });
    }
    Vectorized<T> tan() const {
        return map([](T x) -> T { return std::tan(x); });
    }
    Vectorized<T> tanh() const {
        return map([](T x) -> T { return std::tanh(x); });
    }
    Vectorized<T> acos() const {
        return map([](T x) -> T { return std::acos(x); });
    }
    Vectorized<T> asin() const {
        return map([](T x) -> T { return std::asin(x); });
    }
    Vectorized<T> atan() const {
        return map([](T x) -> T { return std::atan(x); });
    }
    Vectorized<T> atan2(const Vectorized<T> &exp) const {
        Vectorized<T> ret;
        for (const auto i : otter::irange(size())) {
            ret[i] = std::atan2(values[i], exp[i]);
        }
        return ret;
    }
    Vectorized<T> sqrt() const {
        return map(std::sqrt);
    }
    Vectorized<T> reciprocal() const {
        return map([](T x) { return (T)(1) / x; });
    }
    Vectorized<T> rsqrt() const {
        return map([](T x) { return (T)1 / std::sqrt(x); });
    }
    Vectorized<T> pow(const Vectorized<T> &exp) const {
        Vectorized<T> ret;
        for (const auto i : otter::irange(size())) {
            ret[i] = std::pow(values[i], exp[i]);
        }
        return ret;
    }
    Vectorized<T> log() const {
        return map(std::log);
    }
    Vectorized<T> log10() const {
        return map(std::log10);
    }
    template <
    typename U = T,
    typename std::enable_if_t<is_floating_point<U>::value, int> = 0>
    Vectorized<T> fmod(const Vectorized<T>& q) const {
        // U is for SFINAE purposes only. Make sure it is not changed.
        static_assert(std::is_same<U, T>::value, "U must be T");
        Vectorized<T> ret;
        for (const auto i : otter::irange(size())) {
            ret[i] = std::fmod(values[i], q[i]);
        }
        return ret;
    }
    
private:
    template <typename Op>
    inline Vectorized<T> binary_pred(const Vectorized<T>& other, Op op) const {
        // All bits are set to 1 if the pred is true, otherwise 0.
        Vectorized<T> vector;
        for (int64_t i = 0; i != size(); i++) {
            if (op(values[i], other.values[i])) {
                std::memset(static_cast<void*>(vector.values + i), 0xFF, sizeof(T));
            } else {
                std::memset(static_cast<void*>(vector.values + i), 0, sizeof(T));
            }
        }
        return vector;
    }
    
public:
    Vectorized<T> operator==(const Vectorized<T>& other) const { return binary_pred(other, std::equal_to<T>()); }
    Vectorized<T> operator!=(const Vectorized<T>& other) const { return binary_pred(other, std::not_equal_to<T>()); }
    Vectorized<T> operator>=(const Vectorized<T>& other) const { return binary_pred(other, std::greater_equal<T>()); }
    Vectorized<T> operator<=(const Vectorized<T>& other) const { return binary_pred(other, std::less_equal<T>()); }
    Vectorized<T> operator>(const Vectorized<T>& other) const { return binary_pred(other, std::greater<T>()); }
    Vectorized<T> operator<(const Vectorized<T>& other) const { return binary_pred(other, std::less<T>()); }
};

struct Vectorizedi;

template<class T, typename std::enable_if_t<!std::is_base_of<Vectorizedi, Vectorized<T>>::value, int> = 0>
inline Vectorized<T> operator~(const Vectorized<T>& a) {
    Vectorized<T> ones;  // All bits are 1
    memset((T*) ones, 0xFF, VECTOR_WIDTH);
    return a ^ ones;
}

template <typename T>
inline Vectorized<T>& operator += (Vectorized<T>& a, const Vectorized<T>& b) {
    a = a + b;
    return a;
}
template <typename T>
inline Vectorized<T>& operator -= (Vectorized<T>& a, const Vectorized<T>& b) {
    a = a - b;
    return a;
}
template <typename T>
inline Vectorized<T>& operator /= (Vectorized<T>& a, const Vectorized<T>& b) {
    a = a / b;
    return a;
}
template <typename T>
inline Vectorized<T>& operator %= (Vectorized<T>& a, const Vectorized<T>& b) {
    a = a % b;
    return a;
}
template <typename T>
inline Vectorized<T>& operator *= (Vectorized<T>& a, const Vectorized<T>& b) {
    a = a * b;
    return a;
}

template <typename T>
inline Vectorized<T> fmadd(const Vectorized<T>& a, const Vectorized<T>& b, const Vectorized<T>& c) {
    return a * b + c;
}

template <class T> Vectorized<T>
inline operator+(const Vectorized<T> &a, const Vectorized<T> &b) {
    Vectorized<T> c;
    for (int i = 0; i != Vectorized<T>::size(); i++) {
        c[i] = a[i] + b[i];
    }
    return c;
}

template <class T> Vectorized<T>
inline operator-(const Vectorized<T> &a, const Vectorized<T> &b) {
    Vectorized<T> c;
    for (int i = 0; i != Vectorized<T>::size(); i++) {
        c[i] = a[i] - b[i];
    }
    return c;
}

template <class T> Vectorized<T>
inline operator*(const Vectorized<T> &a, const Vectorized<T> &b) {
    Vectorized<T> c;
    for (int i = 0; i != Vectorized<T>::size(); i++) {
        c[i] = a[i] * b[i];
    }
    return c;
}

template <class T> Vectorized<T>
inline operator/(const Vectorized<T> &a, const Vectorized<T> &b) __ubsan_ignore_float_divide_by_zero__ {
    Vectorized<T> c;
    for (int i = 0; i != Vectorized<T>::size(); i++) {
        c[i] = a[i] / b[i];
    }
    return c;
}

template <class T> Vectorized<T>
inline operator||(const Vectorized<T> &a, const Vectorized<T> &b) {
    Vectorized<T> c;
    for (int i = 0; i != Vectorized<T>::size(); i++) {
        c[i] = a[i] || b[i];
    }
    return c;
}

template<typename dst_t, typename src_t>
struct CastImpl {
    static inline Vectorized<dst_t> apply(const Vectorized<src_t>& src) {
        src_t src_arr[Vectorized<src_t>::size()];
        src.store(static_cast<void*>(src_arr));
        return Vectorized<dst_t>::loadu(static_cast<const void*>(src_arr));
    }
};

template<typename scalar_t>
struct CastImpl<scalar_t, scalar_t> {
    static inline Vectorized<scalar_t> apply(const Vectorized<scalar_t>& src) {
        return src;
    }
};

template<typename dst_t, typename src_t>
inline Vectorized<dst_t> cast(const Vectorized<src_t>& src) {
    return CastImpl<dst_t, src_t>::apply(src);
}

template <typename src_T, typename dst_T>
inline void convert(const src_T *src, dst_T *dst, int64_t n) {
#ifndef _MSC_VER
# pragma unroll
#endif
    for (const auto i : otter::irange(n)) {
        (void)i; //Suppress unused variable warning
        *dst = otter::static_cast_with_inter_type<dst_T, src_T>::apply(*src);
        src++;
        dst++;
    }
}

#if defined(CPU_CAPABILITY_AVX2) || defined(CPU_CAPABILITY_AVX512)
template <class T, typename Op>
static inline Vectorized<T> bitwise_binary_op(const Vectorized<T> &a, const Vectorized<T> &b, Op op) {
    int_vector buffer;
#if defined(CPU_CAPABILITY_AVX2)
    int_vector a_buffer = _mm256_load_si256(reinterpret_cast<const int_vector*>((const T*)a));
    int_vector b_buffer = _mm256_load_si256(reinterpret_cast<const int_vector*>((const T*)b));
#elif defined(CPU_CAPABILITY_AVX512)
    int_vector a_buffer = _mm512_load_si512(reinterpret_cast<const int_vector*>((const T*)a));
    int_vector b_buffer = _mm512_load_si512(reinterpret_cast<const int_vector*>((const T*)b));
#endif
    buffer = op(a_buffer, b_buffer);
    __otter_align__ T results[Vectorized<T>::size()];
    
#if defined(CPU_CAPABILITY_AVX2)
    _mm256_store_si256(reinterpret_cast<int_vector*>(results), buffer);
#elif defined(CPU_CAPABILITY_AVX512)
    _mm512_store_si512(reinterpret_cast<int_vector*>(results), buffer);
#endif
    return Vectorized<T>::loadu(results);
}

template<class T, typename std::enable_if_t<!std::is_base_of<Vectorizedi, Vectorized<T>>::value, int> = 0>
inline Vectorized<T> operator&(const Vectorized<T>& a, const Vectorized<T>& b) {
    // We enclose _mm512_and_si512 or _mm256_and_si256 with lambda because it is always_inline
#if defined(CPU_CAPABILITY_AVX2)
    return bitwise_binary_op(a, b, [](int_vector a, int_vector b) { return _mm256_and_si256(a, b); });
#elif defined(CPU_CAPABILITY_AVX512)
    return bitwise_binary_op(a, b, [](int_vector a, int_vector b) { return _mm512_and_si512(a, b); });
#endif
}
template<class T, typename std::enable_if_t<!std::is_base_of<Vectorizedi, Vectorized<T>>::value, int> = 0>
inline Vectorized<T> operator|(const Vectorized<T>& a, const Vectorized<T>& b) {
    // We enclose _mm512_or_si512 or _mm256_or_si256 with lambda because it is always_inline
#if defined(CPU_CAPABILITY_AVX2)
    return bitwise_binary_op(a, b, [](int_vector a, int_vector b) { return _mm256_or_si256(a, b); });
#elif defined(CPU_CAPABILITY_AVX512)
    return bitwise_binary_op(a, b, [](int_vector a, int_vector b) { return _mm512_or_si512(a, b); });
#endif
}
template<class T, typename std::enable_if_t<!std::is_base_of<Vectorizedi, Vectorized<T>>::value, int> = 0>
inline Vectorized<T> operator^(const Vectorized<T>& a, const Vectorized<T>& b) {
    // We enclose _mm512_xor_si512 or _mm256_xor_si256 with lambda because it is always_inline
#if defined(CPU_CAPABILITY_AVX2)
    return bitwise_binary_op(a, b, [](int_vector a, int_vector b) { return _mm256_xor_si256(a, b); });
#elif defined(CPU_CAPABILITY_AVX512)
    return bitwise_binary_op(a, b, [](int_vector a, int_vector b) { return _mm512_xor_si512(a, b); });
#endif
}

#else

template <typename T>
auto load(char const* data) -> T {
    T ret;
    std::memcpy(&ret, data, sizeof(ret));
    return ret;
}

template<class T, typename Op>
static inline Vectorized<T> bitwise_binary_op(const Vectorized<T> &a, const Vectorized<T> &b, Op op) {
    static constexpr uint32_t element_no = VECTOR_WIDTH / sizeof(intmax_t);
    __otter_align__ intmax_t buffer[element_no];
    static_assert(VECTOR_WIDTH % sizeof(intmax_t) == 0, "VECTOR_WIDTH not a multiple of sizeof(intmax_t)");
    static_assert(sizeof(buffer) == sizeof(Vectorized<T>), "sizeof(buffer) must match sizeof(Vectorized<T>)");
    // We should be using memcpy in order to respect the strict aliasing rule
    // see: https://github.com/pytorch/pytorch/issues/66119
    // Using char* is defined in the C11 standard 6.5 Expression paragraph 7
    // (http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf)
    const auto* a_data = a.as_bytes();
    const auto* b_data = b.as_bytes();
    // load each intmax_t chunk and process; increase pointers by sizeof(intmax_t)
    for (auto& out : buffer) {
        out = op(load<intmax_t>(a_data), load<intmax_t>(b_data));
        a_data += sizeof(intmax_t);
        b_data += sizeof(intmax_t);
    }
    assert(a_data == a.as_bytes() + sizeof(a));
    assert(b_data == b.as_bytes() + sizeof(b));
    return Vectorized<T>::loadu(buffer);
}

template<class T, typename std::enable_if_t<!std::is_base_of<Vectorizedi, Vectorized<T>>::value, int> = 0>
inline Vectorized<T> operator&(const Vectorized<T>& a, const Vectorized<T>& b) {
    return bitwise_binary_op(a, b, std::bit_and<intmax_t>());
}
template<class T, typename std::enable_if_t<!std::is_base_of<Vectorizedi, Vectorized<T>>::value, int> = 0>
inline Vectorized<T> operator|(const Vectorized<T>& a, const Vectorized<T>& b) {
    return bitwise_binary_op(a, b, std::bit_or<intmax_t>());
}
template<class T, typename std::enable_if_t<!std::is_base_of<Vectorizedi, Vectorized<T>>::value, int> = 0>
inline Vectorized<T> operator^(const Vectorized<T>& a, const Vectorized<T>& b) {
    return bitwise_binary_op(a, b, std::bit_xor<intmax_t>());
}

#endif // defined(CPU_CAPABILITY_AVX2) || defined(CPU_CAPABILITY_AVX512)


}   // end namespace vec
}   // end namespace otter

#endif /* VecBase_hpp */
