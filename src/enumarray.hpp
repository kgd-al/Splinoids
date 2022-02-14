#ifndef KGD_ENUMARRAY_H
#define KGD_ENUMARRAY_H

#include <array>

namespace utils {

// Credit goes to xskxzr (but does not seem to work)
// https://stackoverflow.com/questions/60013879/how-to-write-a-constexpr-quick-sort-in-c17
template<typename T, std::size_t N, typename P>
constexpr void constexpr_quickSort(std::array<T, N> &array,
                                   std::size_t low, std::size_t high,
                                   const P &lower) {
  if (high <= low) return;
  auto i = low, j = high + 1;
  auto key = array[low];
  for (;;) {
    while (lower(array[++i], key)) if (i == high) break;
    while (lower(key, array[--j])) if (j == low) break;
    if (i >= j) break;

    auto tmp = array[i];
    array[i] = array[j];
    array[j] = tmp;
  }

  auto tmp = array[low];
  array[low] = array[j];
  array[j] = tmp;

  constexpr_quickSort(array, low, j - 1, lower);
  constexpr_quickSort(array, j + 1, high, lower);
}

template<typename T, std::size_t N, typename P>
constexpr void constexpr_quickSort(std::array<T, N> &array, const P &lower) {
  constexpr_quickSort(array, 0, N - 1, lower);
}

template <typename V, typename E, E... INDICES>
struct enumarray {
  static_assert(std::is_enum<E>::value, "Type must be an enumeration!");
  static_assert(sizeof...(INDICES) > 1, "Not enough values provided");

  using size_type = std::size_t;

  static constexpr auto SIZE = sizeof...(INDICES);

  struct LUI {
    E value;
    size_type index;
  };
  using LUT = std::array<LUI, SIZE>;
  static constexpr LUT _createLUT (void) {
    LUT lut {};
    uint i=0;
    const auto assign = [&lut, &i] (E v) {
      lut[i] = {v, i};
      i++;
    };
    (..., assign(INDICES));
//    std::sort(lut.begin(), lut.end(), [] (LUI lhs, LUI rhs) {
//    constexpr_quickSort(lut, [] (LUI lhs, LUI rhs) {
//      using ut = typename std::underlying_type<E>::type;
//      return ut(lhs.value) < ut(rhs.value);
//    });
    return lut;
  }
  static constexpr LUT _lut = _createLUT();

  static constexpr size_type indexOf (E v, size_type l, size_type r) {
    size_type m = (l+r) / 2;
    const LUI &i = _lut[m];
    if (i.value == v)     return i.index;
    else if (i.value > v) return indexOf(v, l, m-1);
    else                  return indexOf(v, m+1, r);
  }

  static constexpr size_type indexOf (E v) {
#ifndef NDEBUG
    assert(isValid(v));
    auto j = indexOf(v, 0, SIZE-1);
    assert(0 <= j);
    assert(j < SIZE);
    return j;
#else
    return indexOf(v, 0, SIZE-1);
#endif
  }

  using buffer_type = std::array<V, SIZE>;
  buffer_type _buffer;

  using iterator = typename buffer_type::iterator;
  using pointer = typename buffer_type::pointer;
  using const_pointer = typename buffer_type::const_pointer;

 static constexpr bool isValid (E v) {
    return (... || (INDICES == v));
  }

  void fill (V v) {
    _buffer.fill(v);
  }

  const V& operator[] (E i) const {
    return _buffer[indexOf(i)];
  }

  V& operator[] (E i) {
    return _buffer[indexOf(i)];
  }

  constexpr const V& at (E i) const {
    return isValid(i) ? this->operator [](i)
        : (utils::doThrow<std::invalid_argument>(
          "Provided enumeration value is not a registered index"),
          _buffer[0]);
  }

  constexpr V& at (E i) {
    return isValid(i) ? this->operator [](i)
        : (utils::doThrow<std::invalid_argument>(
          "Provided enumeration value is not a registered index"),
          _buffer[0]);
  }

  constexpr iterator begin (void) noexcept {
    return iterator(data());
  }

  constexpr iterator begin (void) const noexcept {
    return iterator(data());
  }

  constexpr iterator end (void) noexcept {
    return iterator(data() + SIZE);
  }

  constexpr iterator end (void) const noexcept {
    return iterator(data() + SIZE);
  }

  constexpr pointer data (void) noexcept {
    return _buffer.data();
  }

  constexpr const_pointer data (void) const noexcept {
    return _buffer.data();
  }
};

} // end of namespace utils

#endif // KGD_ENUMARRAY_H
