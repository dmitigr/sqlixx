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

#ifndef DMITIGR_SQLIXX_CONVERSIONS_HPP
#define DMITIGR_SQLIXX_CONVERSIONS_HPP

#include "data.hpp"
#include "exceptions.hpp"
#include "../error/assert.hpp"

#include <sqlite3.h>

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dmitigr::sqlixx {

namespace detail {
inline void check_bind(sqlite3_stmt* const handle, const int r)
{
  DMITIGR_ASSERT(handle);
  if (r != SQLITE_OK)
    throw Sqlite_exception{r,
      std::string{"cannot bind a parameter to SQLite prepared statement"}
        .append(" (").append(sqlite3_errmsg(sqlite3_db_handle(handle))).append(")")};
}
} // namespace detail

/// The centralized "namespace" for data conversions.
template<typename, typename = void> struct Conversions;

/// The implementation of `int` conversions.
template<>
struct Conversions<int> final {
  static void bind(sqlite3_stmt* const handle, const int index, const int value)
  {
    detail::check_bind(handle, sqlite3_bind_int(handle, index, value));
  }

  static int result(sqlite3_stmt* const handle, const int index)
  {
    DMITIGR_ASSERT(handle);
    return sqlite3_column_int(handle, index);
  }
};

/// The implementation of `sqlite3_int64` conversions.
template<>
struct Conversions<sqlite3_int64> final {
  static void bind(sqlite3_stmt* const handle, const int index, const sqlite3_int64 value)
  {
    detail::check_bind(handle, sqlite3_bind_int64(handle, index, value));
  }

  static sqlite3_int64 result(sqlite3_stmt* const handle, const int index)
  {
    DMITIGR_ASSERT(handle);
    return sqlite3_column_int64(handle, index);
  }
};

/// The implementation of `double` conversions.
template<>
struct Conversions<double> final {
  static void bind(sqlite3_stmt* const handle, const int index, const double value)
  {
    detail::check_bind(handle, sqlite3_bind_double(handle, index, value));
  }

  static double result(sqlite3_stmt* const handle, const int index)
  {
    DMITIGR_ASSERT(handle);
    return sqlite3_column_double(handle, index);
  }
};

/// The implementation of `Data` conversions.
template<typename T, unsigned char E>
struct Conversions<Data<T, E>> final {
  template<typename B>
  static std::enable_if_t<std::is_same_v<std::decay_t<B>, Data<T, E>>>
  bind(sqlite3_stmt* const handle, const int index, B&& value)
  {
    const typename Data<T, E>::Deleter destr = [&value]
    {
      if constexpr (std::is_rvalue_reference_v<B&&>) {
        if (value.is_data_owner()) {
          const auto result = value.deleter_;
          value = {};
          DMITIGR_ASSERT(!value.is_data_owner());
          return result;
        } else
          return SQLITE_STATIC;
      } else
        return SQLITE_STATIC;
    }();

    const int br = [&]
    {
      if constexpr (E == 0)
        return sqlite3_bind_blob64(handle, index, value.data(), value.size(), destr);
      else
        return sqlite3_bind_text64(handle, index, value.data(), value.size(), destr, E);
    }();
    detail::check_bind(handle, br);
  }

  static Data<T, E> result(sqlite3_stmt* const handle, const int index)
  {
    static_assert((E == 0) || (E == SQLITE_UTF8) || (E == SQLITE_UTF16),
      "SQLite only provides sqlite3_column_text() and sqlite3_column_text16()");
    DMITIGR_ASSERT(handle);
    using R = Data<T, E>;
    if constexpr (E == 0)
      return R{sqlite3_column_blob(handle, index),
        static_cast<typename R::Size>(sqlite3_column_bytes(handle, index))};
    else if (E == SQLITE_UTF8)
      return R{reinterpret_cast<const typename R::Type*>(sqlite3_column_text(handle, index)),
        static_cast<typename R::Size>(sqlite3_column_bytes(handle, index))};
    else // SQLITE_UTF16
      return R{reinterpret_cast<const typename R::Type*>(sqlite3_column_text16(handle, index)),
        static_cast<typename R::Size>(sqlite3_column_bytes16(handle, index))};
  }
};

/// The implementation of `std::string` and `std::string_view` conversions.
template<typename T>
struct Conversions<T,
  std::enable_if_t<
    std::is_same_v<T, std::string> ||
    std::is_same_v<T, std::string_view>>> final {
  template<typename S>
  static std::enable_if_t<std::is_same_v<std::decay_t<S>, T>>
  bind(sqlite3_stmt* const handle, const int index, S&& value)
  {
    const auto destr = std::is_rvalue_reference_v<S&&> ? SQLITE_TRANSIENT : SQLITE_STATIC;
    detail::check_bind(handle, sqlite3_bind_text64(handle,
      index, value.data(), value.size(), destr, SQLITE_UTF8));
  }

  static T result(sqlite3_stmt* const handle, const int index)
  {
    DMITIGR_ASSERT(handle);
    return T{reinterpret_cast<const char*>(sqlite3_column_text(handle, index)),
        static_cast<typename T::size_type>(sqlite3_column_bytes(handle, index))};
  }
};

/// The implementation of `std::optional<T>` conversions.
template<typename T>
struct Conversions<std::optional<T>> final {
  template<typename O>
  static std::enable_if_t<std::is_same_v<std::decay_t<O>, std::optional<T>>>
  bind(sqlite3_stmt* const handle, const int index, O&& value)
  {
    if (value) {
      if constexpr (std::is_rvalue_reference_v<O&&>)
        bind(handle, index, std::move(*value));
      else
        bind(handle, index, *value);
    } else
      detail::check_bind(handle, sqlite3_bind_null(handle, index));
  }

  static std::optional<T> result(sqlite3_stmt* const handle, const int index)
  {
    DMITIGR_ASSERT(handle);
    if (sqlite3_column_type(handle, index) != SQLITE_NULL)
      return Conversions<T>::result(handle, index);
    else
      return std::nullopt;
  }
};

} // namespace dmitigr::sqlixx

#endif  // DMITIGR_SQLIXX_CONVERSIONS_HPP
