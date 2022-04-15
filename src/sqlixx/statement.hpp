// -*- C++ -*-
//
// Copyright 2022 Dmitry Igrishin
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef DMITIGR_SQLIXX_STATEMENT_HPP
#define DMITIGR_SQLIXX_STATEMENT_HPP

#include "conversions.hpp"
#include "../base/assert.hpp"

#include <sqlite3.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dmitigr::sqlixx {

class Statement;

namespace detail {
template<typename F, typename = void>
struct Execute_callback_traits final {
  constexpr static bool is_valid = false;
};

template<typename F>
struct Execute_callback_traits<F,
  std::enable_if_t<std::is_invocable_v<F, const Statement&>>> final {
  using Result = std::invoke_result_t<F, const Statement&>;
  constexpr static bool is_result_void = std::is_same_v<Result, void>;
  constexpr static bool is_valid =
    std::is_invocable_r_v<bool, F, const Statement&> || is_result_void;
  constexpr static bool has_error_parameter = false;
};

template<typename F>
struct Execute_callback_traits<F,
  std::enable_if_t<std::is_invocable_v<F, const Statement&, int>>> final {
  using Result = std::invoke_result_t<F, const Statement&, int>;
  constexpr static bool is_result_void = std::is_same_v<Result, void>;
  constexpr static bool is_valid =
    std::is_invocable_r_v<bool, F, const Statement&, int> || is_result_void;
  constexpr static bool has_error_parameter = true;
};
} // namespace detail

/// A prepared statement.
class Statement final {
public:
  /// The destructor.
  ~Statement()
  {
    try {
      close();
    } catch(const std::exception& e) {
      std::fprintf(stderr, "%s\n", e.what());
    } catch(...) {}
  }

  /// The constructor.
  explicit Statement(sqlite3_stmt* const handle = {})
    : handle_{handle}
  {}

  /// @overload
  Statement(sqlite3* const handle, const std::string_view sql,
    const unsigned int flags = 0)
  {
    if (!handle)
      throw Exception{"cannot create SQLite statement using invalid handle"};

    if (const int r = sqlite3_prepare_v3(handle, sql.data(),
        static_cast<int>(sql.size()), flags, &handle_, nullptr); r != SQLITE_OK)
      throw Sqlite_exception{r, std::string{"cannot prepare SQLite statement "}
        .append(sql).append(" (").append(sqlite3_errmsg(handle)).append(")")};

    DMITIGR_ASSERT(handle_);
  }

  /// Non-copyable.
  Statement(const Statement&) = delete;

  /// Non-copyable.
  Statement& operator=(const Statement&) = delete;

  /// The move constructor.
  Statement(Statement&& rhs) noexcept
  {
    Statement tmp;
    tmp.swap(rhs); // reset rhs to the default state
    swap(tmp);
  }

  /// The move assignment operator.
  Statement& operator=(Statement&& rhs) noexcept
  {
    if (this != &rhs) {
      Statement tmp{std::move(rhs)};
      swap(tmp);
    }
    return *this;
  }

  /// The swap operation.
  void swap(Statement& other) noexcept
  {
    using std::swap;
    swap(last_step_result_, other.last_step_result_);
    swap(handle_, other.handle_);
  }

  /// @returns The underlying handle.
  sqlite3_stmt* handle() const noexcept
  {
    return handle_;
  }

  /// @returns `true` if this object keeps handle, or `false` otherwise.
  explicit operator bool() const noexcept
  {
    return handle_;
  }

  /// @returns The released handle.
  sqlite3_stmt* release() noexcept
  {
    auto* const result = handle_;
    last_step_result_ = -1;
    handle_ = {};
    return result;
  }

  /**
   * @brief Closes the statement.
   *
   * @returns Non SQLITE_OK if the most recent execution of statement failed.
   */
  int close()
  {
    const int result = sqlite3_finalize(handle_);
    last_step_result_ = -1;
    handle_ = {};
    return result;
  }

  // ---------------------------------------------------------------------------

  /// @name Parameters
  /// @remarks Parameter indexes starts from zero!
  /// @{

  /**
   * @returns The number of parameters.
   *
   * @par Requires
   * `handle()`.
   */
  int parameter_count() const
  {
    if (!handle_)
      throw Exception{"cannot get parameter count of invalid SQLite statement"};

    return sqlite3_bind_parameter_count(handle_);
  }

  /**
   * @returns The parameter index, or -1 if no parameter `name` presents.
   *
   * @par Requires
   * `handle() && name`.
   */
  int parameter_index(const char* const name) const
  {
    if (!handle_)
      throw Exception{"cannot get parameter index of invalid SQLite statement"};
    else if (!name)
      throw Exception{"cannot get SQLite statement parameter index using "
        "invalid name"};

    return sqlite3_bind_parameter_index(handle_, name) - 1;
  }

  /**
   * @returns The parameter index.
   *
   * @par Requires
   * `handle() && name`.
   */
  int parameter_index_throw(const char* const name) const
  {
    const int index = parameter_index(name);
    if (index < 0)
      throw Exception{std::string{"SQLite statement has no parameter "}
        .append(name)};
    return index;
  }

  /**
   * @returns The name of the parameter by the `index`.
   *
   * @par Requires
   * `handle() && index < parameter_count()`.
   */
  std::string parameter_name(const int index) const
  {
    if (!handle_)
      throw Exception{"cannot get parameter name of invalid SQLite statement"};
    else if (!(index < parameter_count()))
      throw Exception{"cannot get SQLite statement parameter name using "
        "invalid index"};

    return sqlite3_column_name(handle_, index + 1);
  }

  /**
   * @brief Binds all the parameters with NULL.
   *
   * @par Requires
   * `handle()`.
   */
  void bind_null()
  {
    if (!handle_)
      throw Exception{"cannot bind NULL to parameters of invalid SQLite "
        "statement"};

    detail::check_bind(handle_, sqlite3_clear_bindings(handle_));
  }

  /**
   * @brief Binds the parameter of the specified index with NULL.
   *
   * @par Requires
   * `handle() && index < parameter_count()`.
   */
  void bind_null(const int index)
  {
    if (!handle_)
      throw Exception{"cannot bind NULL to a parameter of invalid SQLite "
        "statement"};
    else if (!(index < parameter_count()))
      throw Exception{"cannot bind NULL to a parameter of SQLite statement "
        "using invalid index"};

    detail::check_bind(handle_, sqlite3_bind_null(handle_, index + 1));
  }

  /// @overload
  void bind_null(const char* const name)
  {
    bind_null(parameter_index_throw(name));
  }

  /**
   * @brief Binds the parameter of the specified index with `value`.
   *
   * @par Requires
   * `handle() && index < parameter_count()`.
   *
   * @remarks `value` is assumed to be UTF-8 encoded.
   */
  void bind(const int index, const char* const value)
  {
    if (!handle_)
      throw Exception{"cannot bind a text to a parameter of invalid SQLite "
        "statement"};
    else if (!(index < parameter_count()))
      throw Exception{"cannot bind a text to a parameter of SQLite statement "
        "using invalid index"};

    detail::check_bind(handle_,
      sqlite3_bind_text(handle_, index + 1, value, -1, SQLITE_STATIC));
  }

  /// @overload
  void bind(const char* const name, const char* const value)
  {
    bind(parameter_index_throw(name), value);
  }

  /**
   * @brief Binds the parameter of the specified index with the value of type `T`.
   *
   * @param index A zero-based index of the parameter.
   * @param value A value to bind. If this parameter is `lvalue`, then it's
   * assumed that the value is a constant and does not need to be copied. If this
   * parameter is `rvalue`, then it's assumed to be destructed after this function
   * returns, so SQLite is required to make a private copy of the value before
   * return.
   *
   * @par Requires
   * `handle() && index < parameter_count()`.
   */
  template<typename T>
  void bind(const int index, T&& value)
  {
    if (!handle_)
      throw Exception{"cannot bind a value to a parameter of invalid SQLite "
        "statement"};
    else if (!(index < parameter_count()))
      throw Exception{"cannot bind a value to a parameter of SQLite statement "
        "using invalid index"};

    using U = std::decay_t<T>;
    Conversions<U>::bind(handle_, index + 1, std::forward<T>(value));
  }

  /// @overload
  template<typename T>
  void bind(const char* const name, T&& value)
  {
    bind(parameter_index_throw(name), std::forward<T>(value));
  }

  /**
   * @brief Binds parameters by indexes in range [0, sizeof ... (values)).
   *
   * @details In other words:
   * @code bind_many(value1, value2, value3) @endcode
   * equivalently to
   * @code (bind(0, value1), bind(1, value1), bind(2, value2)) @endcode
   *
   * @see bind().
   */
  template<typename ... Types>
  void bind_many(Types&& ... values)
  {
    bind_many__(std::make_index_sequence<sizeof ... (Types)>{},
      std::forward<Types>(values)...);
  }

  /// @}

  // ---------------------------------------------------------------------------

  /// @name Execution
  /// @{

  /**
   * @brief Executes the prepared statement.
   *
   * @param callback A function to be called for each retrieved row. The callback:
   *   -# can be defined with a parameter of type `const Statement&`. The exception
   *   will be thrown on error in this case.
   *   -# can be defined with two parameters of type `const Statement&` and `int`.
   *   In case of error an error code will be passed as the second argument of
   *   the callback instead of throwing exception and execution will be stopped
   *   after callback returns. In case of success, `0` will be passed as the second
   *   argument of the callback.
   *   -# can return a value convertible to `bool` to indicate should the execution
   *   to be continued after the callback returns or not;
   *   -# can return `void` to indicate that execution must be proceed until a
   *   completion or an error.
   * @param values Values to bind with parameters. Binding will take place if this
   * method is never been called before for this instance or if the last value it
   * return was `SQLITE_DONE`.
   *
   * @par Requires
   * `handle()`.
   *
   * @remarks If the last value of this method was `SQLITE_DONE` then `reset()`
   * will be called automatically.
   *
   * @returns The result of `sqlite3_step()`.
   *
   * @see reset().
   */
  template<typename F, typename ... Types>
  std::enable_if_t<detail::Execute_callback_traits<F>::is_valid, int>
  execute(F&& callback, Types&& ... values)
  {
    if (!handle_)
      throw Exception{"cannot execute invalid SQLite statement"};

    if (last_step_result_ == SQLITE_DONE)
      reset();

    if (last_step_result_ < 0 || last_step_result_ == SQLITE_DONE)
      bind_many(std::forward<Types>(values)...);

    while (true) {
      using Traits = detail::Execute_callback_traits<F>;
      switch (last_step_result_ = sqlite3_step(handle_)) {
      case SQLITE_ROW:
        if constexpr (!Traits::is_result_void) {
          if constexpr (!Traits::has_error_parameter) {
            if (!callback(static_cast<const Statement&>(*this)))
              return last_step_result_;
          } else {
            if (!callback(static_cast<const Statement&>(*this), last_step_result_))
              return last_step_result_;
          }
        } else {
          if constexpr (!Traits::has_error_parameter)
            callback(static_cast<const Statement&>(*this));
          else
            callback(static_cast<const Statement&>(*this), last_step_result_);
        }
        continue;
      case SQLITE_DONE:
        return last_step_result_;
      default:
        if constexpr (Traits::has_error_parameter) {
          callback(static_cast<const Statement&>(*this), last_step_result_);
          return last_step_result_;
        } else
          throw Sqlite_exception{last_step_result_,
            std::string{"SQLite statement execution failed"}
              .append(" (").append(sqlite3_errmsg(sqlite3_db_handle(handle_)))
              .append(")")};
      }
    }
  }

  /// @overload
  template<typename ... Types>
  int execute(Types&& ... values)
  {
    return execute([](const auto&){return true;}, std::forward<Types>(values)...);
  }

  /// Resets the statement back to its initial state, ready to be executed.
  int reset()
  {
    last_step_result_ = -1;
    return sqlite3_reset(handle_);
  }

  /// @}

  // ---------------------------------------------------------------------------

  /// @name Result
  /// @{

  /**
   * @returns The number of columns.
   *
   * @par Requires
   * `handle()`.
   */
  int column_count() const
  {
    if (!handle_)
      throw Exception{"cannot get result column count of invalid SQLite "
        "statement"};

    return sqlite3_column_count(handle_);
  }

  /**
   * @returns The column index, or -1 if no column `name` presents.
   *
   * @par Requires
   * `handle() && name`.
   */
  int column_index(const char* const name) const
  {
    if (!handle_)
      throw Exception{"cannot get result column index of invalid SQLite "
        "statement"};
    else if (!name)
      throw Exception{"cannot get result column index of SQLite statement "
        "using invalid name"};

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
   * @returns The column index.
   *
   * @par Requires
   * `handle() && name`.
   */
  int column_index_throw(const char* const name) const
  {
    const int index = column_index(name);
    if (index < 0)
      throw Exception{std::string{"SQLite result has no column "}.append(name)};
    return index;
  }

  /**
   * @returns The name of the column by the `index`.
   *
   * @par Requires
   * `handle() && index < column_count()`.
   */
  std::string column_name(const int index) const
  {
    if (!handle_)
      throw Exception{"cannot get result column name of invalid SQLite "
        "statement"};
    else if (!(index < column_count()))
      throw Exception{"cannot get result column count of SQLite statement "
        "using invalid index"};

    return sqlite3_column_name(handle_, index);
  }

  /**
   * @returns The result of statement execution.
   *
   * @tparam T The resulting type of statement execution result.
   *
   * @par Requires
   * `handle() && index < column_count()`.
   */
  template<typename T>
  T result(const int index) const
  {
    if (!handle_)
      throw Exception{"cannot get result column value of invalid SQLite "
        "statement"};
    else if (!(index < column_count()))
      throw Exception{"cannot get result column value of SQLite statement "
        "using invalid index"};

    using U = std::decay_t<T>;
    return Conversions<U>::result(handle_, index);
  }

  /// @overload
  template<typename T>
  T result(const char* const name) const
  {
    return result<T>(column_index_throw(name));
  }

  /// @}

private:
  int last_step_result_{-1};
  sqlite3_stmt* handle_{};

  template<std::size_t ... I, typename ... Types>
  void bind_many__(std::index_sequence<I...>, Types&& ... values)
  {
    (bind(static_cast<int>(I), std::forward<Types>(values)), ...);
  }
};

} // namespace dmitigr::sqlixx

#endif  // DMITIGR_SQLIXX_STATEMENT_HPP
