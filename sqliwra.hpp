// -*- C++ -*-
// Copyright (C) 2020 Dmitry Igrishin
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

#ifndef DMITIGR_SQLIWRA_HPP
#define DMITIGR_SQLIWRA_HPP

#include <sqlite3.h>

#include <cassert>
#include <cstring>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#if __GNUG__
  #if (__GNUC__ >= 8)
    #include <filesystem>
  #else
    #include <experimental/filesystem>
    namespace std {
    namespace filesystem = experimental::filesystem;
    } // namespace std
  #endif
#else
#include <filesystem>
#endif

namespace dmitigr::sqliwra {

/// A category of SQLite errors.
class Sqlite_error_category final : public std::error_category {
public:
  /// @returns The literal `dmitigr_sqliwra_sqlite_error`.
  const char* name() const noexcept override
  {
    return "dmitigr_sqliwra_sqlite_error";
  }

  /// @returns The string that describes the error condition denoted by `ev`.
  std::string message(int ev) const override
  {
    return std::string{name()}.append(" ").append(std::to_string(ev));
  }
};

/// The instance of type Sqlite_error_category.
inline Sqlite_error_category sqlite_error_category;

/// An exception.
class Exception final : public std::system_error {
public:
  /// The constructor.
  Exception(const int ev, const std::string& what)
    : system_error{ev, sqlite_error_category, what}
  {}
};

// =============================================================================

namespace detail {
inline void check_bind(const int result)
{
  if (result != SQLITE_OK)
    throw Exception{result, "cannot bind prepared statement parameter"};
}
} // namespace detail

/// The centralized "namespace" for column data conversions.
template<typename> struct Conversions;

/// The implementation of `int` conversions.
template<>
struct Conversions<int> final {
  static int data(sqlite3_stmt* const handle, const int index)
  {
    return sqlite3_column_int(handle, index);
  }

  static void bind(sqlite3_stmt* const handle, const int index, const int value)
  {
    detail::check_bind(sqlite3_bind_int(handle, index, value));
  }
};

/// The implementation of `sqlite3_int64` conversions.
template<>
struct Conversions<sqlite3_int64> final {
  static sqlite3_int64 data(sqlite3_stmt* const handle, const int index)
  {
    return sqlite3_column_int64(handle, index);
  }

  static void bind(sqlite3_stmt* const handle, const int index, const sqlite3_int64 value)
  {
    detail::check_bind(sqlite3_bind_int64(handle, index, value));
  }
};

/// The implementation of `double` conversions.
template<>
struct Conversions<double> final {
  static double data(sqlite3_stmt* const handle, const int index)
  {
    return sqlite3_column_double(handle, index);
  }

  static void bind(sqlite3_stmt* const handle, const int index, const double value)
  {
    detail::check_bind(sqlite3_bind_double(handle, index, value));
  }
};

/// The implementation of `std::string` conversions.
template<>
struct Conversions<std::string> final {
  static std::string data(sqlite3_stmt* const handle, const int index)
  {
    return std::string{reinterpret_cast<const char*>(sqlite3_column_text(handle, index)),
        static_cast<std::string::size_type>(sqlite3_column_bytes(handle, index))};
  }

  template<typename S>
  static std::enable_if_t<std::is_same_v<std::decay_t<S>, std::string>>
  bind(sqlite3_stmt* const handle, const int index, S&& value)
  {
    constexpr auto destr = std::is_rvalue_reference_v<S&&> ? SQLITE_TRANSIENT : SQLITE_STATIC;
    detail::check_bind(sqlite3_bind_text64(handle, index,
        value.data(), value.size(), destr, SQLITE_UTF8));
  }
};

/// The implementation of `std::string_view` conversions.
template<>
struct Conversions<std::string_view> final {
  static std::string_view data(sqlite3_stmt* const handle, const int index)
  {
    return std::string_view{static_cast<const char*>(sqlite3_column_blob(handle, index)),
        static_cast<std::string_view::size_type>(sqlite3_column_bytes(handle, index))};
  }

  template<typename Sv>
  static std::enable_if_t<std::is_same_v<std::decay_t<Sv>, std::string_view>>
  bind(sqlite3_stmt* const handle, const int index, Sv&& value)
  {
    constexpr auto destr = std::is_rvalue_reference_v<Sv&&> ? SQLITE_TRANSIENT : SQLITE_STATIC;
    detail::check_bind(sqlite3_bind_blob64(handle, index,
        value.data(), value.size(), destr));
  }
};

/// The implementation of `std::optional<T>` conversions.
template<typename T>
struct Conversions<std::optional<T>> final {
  static std::optional<T> data(sqlite3_stmt* const handle, const int index)
  {
    if (sqlite3_column_type(handle, index) != SQLITE_NULL)
      return Conversions<T>::data(handle, index);
    else
      return std::nullopt;
  }

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
      detail::check_bind(sqlite3_bind_null(handle, index));
  }
};

/// A prepared statement.
class Ps final {
public:
  /// The destructor.
  ~Ps()
  {
    try {
      close();
    } catch(const std::exception& e) {
      std::fprintf(stderr, "%s\n", e.what());
    } catch(...) {}
  }

  /// The constructor.
  explicit Ps(sqlite3_stmt* const handle = nullptr)
    : handle_{handle}
  {}

  /// @overload
  Ps(sqlite3* const handle, const std::string_view sql, const unsigned int flags = 0)
  {
    assert(handle);
    if (const int err = sqlite3_prepare_v3(handle, sql.data(), sql.size(), flags, &handle_, nullptr))
      throw Exception{err, std::string{"cannot prepare statement "}.append(sql)};
    assert(handle_);
  }

  /// Non-copyable.
  Ps(const Ps&) = delete;

  /// Non-copyable.
  Ps& operator=(const Ps&) = delete;

  /// The move constructor.
  Ps(Ps&& rhs) noexcept
    : handle_{rhs.handle_}
  {
    rhs.handle_ = nullptr;
  }

  /// The move assignment operator.
  Ps& operator=(Ps&& rhs) noexcept
  {
    if (this != &rhs) {
      Ps tmp{std::move(rhs)};
      swap(tmp);
    }
    return *this;
  }

  /// The swap operation.
  void swap(Ps& other) noexcept
  {
    std::swap(handle_, other.handle_);
  }

  /// @returns The underlying handle.
  sqlite3_stmt* handle() const noexcept
  {
    return handle_;
  }

  /// @returns The underlying handle.
  operator sqlite3_stmt*() const noexcept
  {
    return handle_;
  }

  /// Closes the prepared statement.
  void close()
  {
    if (const int result = sqlite3_finalize(handle_))
      throw Exception{result, "error upon closing prepared statement"};
    else
      handle_ = nullptr;
  }

  // ---------------------------------------------------------------------------

  /// @name Parameters
  /// @remarks Parameter indexes starts from zero!
  /// @{

  /// @returns The number of parameters.
  int parameter_count() const
  {
    assert(handle_);
    return sqlite3_bind_parameter_count(handle_);
  }

  /// @returns The parameter index, or -1 if no parameter `name` presents.
  int parameter_index(const char* const name) const
  {
    assert(handle_ && name);
    return sqlite3_bind_parameter_index(handle_, name) - 1;
  }

  /**
   * @returns The parameter index.
   *
   * @throws `std::logic_error` if no parameter `name` presents.
   */
  int parameter_index_throw(const char* const name) const
  {
    assert(handle_ && name);
    if (const int index = parameter_index(name); index >= 0)
      return index;
    else
      throw std::logic_error{std::string{"no parameter with name "}.append(name)};
  }

  /// @returns The name of the parameter by the `index`.
  std::string parameter_name(const int index) const noexcept
  {
    assert(handle_ && index < parameter_count());
    return sqlite3_column_name(handle_, index + 1);
  }

  /// Binds all the parameters with NULL.
  void bind_null()
  {
    assert(handle_);
    detail::check_bind(sqlite3_clear_bindings(handle_));
  }

  /// Binds the parameter of the specified index with NULL.
  void bind_null(const int index)
  {
    assert(handle_ && index < parameter_count());
    detail::check_bind(sqlite3_bind_null(handle_, index + 1));
  }

  /// Binds the parameter of the specified index with the value of type `T`.
  template<typename T>
  void bind(const int index, T&& value)
  {
    assert(handle_);
    using U = std::decay_t<T>;
    Conversions<U>::bind(handle_, index + 1, std::forward<T>(value));
  }

  /// @overload
  template<typename T>
  void bind(const char* const name, T&& value)
  {
    bind(parameter_index_throw(name), std::forward<T>(value));
  }

  /// @}

  // ---------------------------------------------------------------------------

  /// @name Execution
  /// @{

  /// Resets a prepared statement object to the ready to be re-executed state.
  void reset()
  {
    assert(handle_);
    if (const int r = sqlite3_reset(handle_); r != SQLITE_OK)
      throw Exception{r, "cannot reset a prepared statement"};
  }

  /**
   * @brief Executes the prepared statement.
   * @param callback A function to be called for each retrieved row. The function
   * must be defined with a parameter of type `const Ps&` and must returns a
   * boolean to indicate should the execution to be continued or not.
   */
  template<typename F>
  void execute(F&& callback)
  {
    assert(handle_);
    while (true) {
      const int step_result = sqlite3_step(handle_);
      switch (step_result) {
      case SQLITE_ROW:
        if (!callback(static_cast<const Ps&>(*this)))
          return;
        else
          continue;
      case SQLITE_OK:
        [[fallthrough]];
      case SQLITE_DONE:
        return;
      default: throw Exception(step_result, "error upon prepared statement execution");
      }
    }
  }

  /// @overload
  void execute()
  {
    return execute([](const auto&){ return true; });
  }

  /// @}

  // ---------------------------------------------------------------------------

  /// @name Result
  /// @{

  /// @returns The number of columns.
  int column_count() const
  {
    assert(handle_);
    return sqlite3_column_count(handle_);
  }

  /// @returns The column index, or -1 if no column `name` presents.
  int column_index(const char* const name) const
  {
    assert(handle_ && name);
    const int count = column_count();
    for (int i = 0; i < count; ++i) {
      if (const char* const nm = sqlite3_column_name(handle_, i)) {
        if (!std::strcmp(name, nm))
          return i;
      } else
        throw std::bad_alloc{};
    }
    return -1;
  }

  /**
   * @returns The columnt index.
   *
   * @throws `std::logic_error` if no column `name` presents.
   */
  int column_index_throw(const char* const name) const
  {
    if (const int index = column_index(name); index >= 0)
      return index;
    else
      throw std::logic_error{std::string{"no column with name "}.append(name)};
  }

  /// @returns The name of the column by the `index`.
  std::string column_name(const int index) const noexcept
  {
    assert(handle_ && index < column_count());
    return sqlite3_column_name(handle_, index);
  }

  /// @returns The column data size in bytes.
  int column_data_size(const int index) const
  {
    assert(handle_ && index < column_count());
    return sqlite3_column_bytes(handle_, index);
  }

  /// @overload
  int column_data_size(const char* const name) const
  {
    return column_data_size(column_index_throw(name));
  }

  /**
   * @returns The result data which may be zero-terminated or not,
   * depending on its type.
   */
  const char* column_data(const int index) const
  {
    assert(handle_ && index < column_count());
    return static_cast<const char*>(sqlite3_column_blob(handle_, index));
  }

  /// @overload
  const char* column_data(const char* const name) const
  {
    return column_data(column_index_throw(name));
  }

  /// @overload
  template<typename T>
  T column_data(const int index) const
  {
    assert(handle_ && index < column_count());
    using U = std::decay_t<T>;
    return Conversions<U>::data(handle_, index);
  }

  /// @overload
  template<typename T>
  T column_data(const char* const name) const
  {
    return column_data<T>(column_index_throw(name));
  }

  /// @}

private:
  sqlite3_stmt* handle_{};
};

// =============================================================================

/// A database connection.
class Conn final {
public:
  /// The destructor.
  ~Conn()
  {
    try {
      close();
    } catch(const std::exception& e) {
      std::fprintf(stderr, "%s\n", e.what());
    } catch(...) {}
  }

  /// The constructor.
  explicit Conn(sqlite3* handle = nullptr)
    : handle_{handle}
  {}

  /// overload.
  Conn(const std::filesystem::path& path, const int flags)
  {
    if (const int err = sqlite3_open_v2(path.c_str(), &handle_, flags, nullptr))
      throw Exception{err, sqlite3_errmsg(handle_)};
    assert(handle_);
  }

  /// Non-copyable.
  Conn(const Conn&) = delete;

  /// Non-copyable.
  Conn& operator=(const Conn&) = delete;

  /// The move constructor.
  Conn(Conn&& rhs) noexcept
    : handle_{rhs.handle_}
  {
    rhs.handle_ = nullptr;
  }

  /// The move assignment operator.
  Conn& operator=(Conn&& rhs) noexcept
  {
    if (this != &rhs) {
      Conn tmp{std::move(rhs)};
      swap(tmp);
    }
    return *this;
  }

  /// The swap operation.
  void swap(Conn& other) noexcept
  {
    std::swap(handle_, other.handle_);
  }

  /// @returns The guarded handle.
  sqlite3* handle() const noexcept
  {
    return handle_;
  }

  /// @returns The guarded handle.
  operator sqlite3*() const noexcept
  {
    return handle();
  }

  /// Closes the database connection.
  void close()
  {
    if (const int result = sqlite3_close(handle_))
      throw Exception{result, "error upon closing database connection"};
    else
      handle_ = nullptr;
  }

  /**
   * @brief Constructs the prepared statement from `sql`.
   *
   * @see Ps::Ps().
   */
  Ps prepare(const std::string_view sql, const unsigned int flags = 0)
  {
    return Ps{handle_, sql, flags};
  }

  /**
   * Executes the `sql`.
   *
   * @see Ps::execute().
   */
  template<typename F>
  void execute(const std::string_view sql, F&& callback)
  {
    assert(handle());
    prepare(sql).execute(std::forward<F>(callback));
  }

  /// @overload
  void execute(const std::string_view sql)
  {
    return execute(sql, [](const auto&){ return true; });
  }

private:
  sqlite3* handle_{};
};

} // namespace dmitigr::sqliwra

#endif  // DMITIGR_SQLIWRA_HPP
