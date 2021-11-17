// -*- C++ -*-
// Copyright (C) 2021 Dmitry Igrishin
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
// Dmitry Igrishin
// dmitigr@gmail.com

#ifndef DMITIGR_SQLIXX_DATA_HPP
#define DMITIGR_SQLIXX_DATA_HPP

#include "../error/assert.hpp"

#include <sqlite3.h>

#include <utility>

namespace dmitigr::sqlixx {

/// A data.
template<typename T, unsigned char E>
struct Data final {
  static_assert((E == 0) || (E == SQLITE_UTF8) ||
    (E == SQLITE_UTF16LE) || (E == SQLITE_UTF16BE) || (E == SQLITE_UTF16),
    "invalid data encoding");

  /// An alias of deleter.
  using Deleter = sqlite3_destructor_type;

  /// The data type.
  using Type = T;

  /// The data size type.
  using Size = sqlite3_uint64;

  /// The data encoding.
  constexpr static unsigned char Encoding = E;

  /// The destructor.
  ~Data()
  {
    if (is_data_owner())
      deleter_(const_cast<T*>(data_));
  }

  /// The default constructor.
  Data() = default;

  /// The constructor.
  Data(const T* const data, const Size size,
    const Deleter deleter = SQLITE_STATIC) noexcept
    : data_{data}
    , size_{size}
    , deleter_{deleter}
  {}

  /// Non-copyable.
  Data(const Data&) = delete;

  /// Non-copyable.
  Data& operator=(const Data&) = delete;

  /// The move constructor.
  Data(Data&& rhs) noexcept
    : data_{rhs.data_}
    , size_{rhs.size_}
    , deleter_{rhs.deleter_}
  {
    rhs.data_ = {};
    rhs.size_ = {};
    rhs.deleter_ = SQLITE_STATIC;
  }

  /// The move assignment operator.
  Data& operator=(Data&& rhs) noexcept
  {
    if (this != &rhs) {
      Data tmp{std::move(rhs)};
      swap(tmp);
    }
    return *this;
  }

  /// The swap operation.
  void swap(Data& other) noexcept
  {
    using std::swap;
    swap(data_, other.data_);
    swap(size_, other.size_);
    swap(deleter_, other.deleter_);
  }

  /// @returns The released data.
  T* release() noexcept
  {
    auto* const result = data_;
    deleter_ = SQLITE_STATIC;
    DMITIGR_ASSERT(!is_data_owner());
    Data{}.swap(*this);
    return result;
  }

  /// @returns The data.
  const T* data() const noexcept { return data_; }

  /// @returns The data size.
  Size size() const noexcept { return size_; }

  /// @returns The deleter.
  Deleter deleter() const noexcept { return deleter_; }

  /// @returns `true` if this instance is owns the data.
  bool is_data_owner() const noexcept
  {
    return deleter_ && (deleter_ != SQLITE_STATIC) && (deleter_ != SQLITE_TRANSIENT);
  }

  /// The data.
  const T* data_{};

  /// The data size.
  Size size_{};

  /// The deleter.
  Deleter deleter_{SQLITE_STATIC};
};

/// An alias of Blob type.
using Blob = Data<void, 0>;

/// An alias of UTF8 text type.
using Text_utf8 = Data<char, SQLITE_UTF8>;

/// An alias of UTF16 text type.
using Text_utf16 = Data<char, SQLITE_UTF16>;

/// An alias of UTF16LE text type.
using Text_utf16le = Data<char, SQLITE_UTF16LE>;

/// An alias of UTF16BE text type.
using Text_utf16be = Data<char, SQLITE_UTF16BE>;

} // namespace dmitigr::sqlixx

#endif  // DMITIGR_SQLIXX_DATA_HPP
