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

#ifndef DMITIGR_SQLIXX_CONNECTION_HPP
#define DMITIGR_SQLIXX_CONNECTION_HPP

#include "statement.hpp"
#include "../error/assert.hpp"
#include "../filesystem.hpp"

#include <sqlite3.h>

#include <cstdio>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace dmitigr::sqlixx {

/// A database(-s) connection.
class Connection final {
public:
  /// The destructor.
  ~Connection()
  {
    try {
      close();
    } catch(const std::exception& e) {
      std::fprintf(stderr, "%s\n", e.what());
    } catch(...) {}
  }

  /// The constructor.
  explicit Connection(sqlite3* handle = {})
    : handle_{handle}
  {}

  /**
   * @brief The constructor.
   *
   * @param ref Path to a file or URI.
   *
   * @par Requires
   * `ref`.
   *
   * @see https://www.sqlite.org/uri.html
   */
  Connection(const char* const ref, const int flags)
  {
    if (!ref)
      throw Exception{"cannot open SQLite connection from null database reference"};

    if (const int r = sqlite3_open_v2(ref, &handle_, flags, nullptr); r != SQLITE_OK) {
      if (handle_)
        throw Sqlite_exception{r, sqlite3_errmsg(handle_)};
      else
        throw std::bad_alloc{};
    }

    DMITIGR_ASSERT(handle_);
  }

  /// @overload
  Connection(const std::string& ref, const int flags)
    : Connection{ref.c_str(), flags}
  {}

  /// @overload
  Connection(const std::filesystem::path& path, const int flags)
#ifdef _WIN32
    : Connection{path.string(), flags}
#else
    : Connection{path.c_str(), flags}
#endif
  {}

  /// Non-copyable.
  Connection(const Connection&) = delete;

  /// Non-copyable.
  Connection& operator=(const Connection&) = delete;

  /// The move constructor.
  Connection(Connection&& rhs) noexcept
    : handle_{rhs.handle_}
  {
    rhs.handle_ = {};
  }

  /// The move assignment operator.
  Connection& operator=(Connection&& rhs) noexcept
  {
    if (this != &rhs) {
      Connection tmp{std::move(rhs)};
      swap(tmp);
    }
    return *this;
  }

  /// The swap operation.
  void swap(Connection& other) noexcept
  {
    using std::swap;
    swap(handle_, other.handle_);
  }

  /// @returns The guarded handle.
  sqlite3* handle() const noexcept
  {
    return handle_;
  }

  /// @returns `true` if this object keeps handle, or `false` otherwise.
  explicit operator bool() const noexcept
  {
    return handle_;
  }

  /// @returns The released handle.
  sqlite3* release() noexcept
  {
    auto* const result = handle_;
    handle_ = {};
    return result;
  }

  /// Closes the database connection.
  void close()
  {
    if (const int r = sqlite3_close(handle_); r != SQLITE_OK)
      throw Sqlite_exception{r, "failed closing a SQLite database connection"};
    else
      handle_ = {};
  }

  /**
   * @returns An instance of type Statement.
   *
   * @see Statement::Statement().
   */
  Statement prepare(const std::string_view sql, const unsigned int flags = 0)
  {
    return Statement{handle_, sql, flags};
  }

  /**
   * @brief Executes the `sql`.
   *
   * @par Requires
   * `handle()`.
   *
   * @see Statement::execute().
   */
  template<typename F, typename ... Types>
  std::enable_if_t<detail::Execute_callback_traits<F>::is_valid>
  execute(F&& callback, const std::string_view sql, Types&& ... values)
  {
    if (!handle_)
      throw Exception{"cannot execute SQLite statement by using invalid connection"};

    prepare(sql).execute(std::forward<F>(callback), std::forward<Types>(values)...);
  }

  /// @overload
  template<typename ... Types>
  void execute(const std::string_view sql, Types&& ... values)
  {
    execute([](const auto&){ return true; }, sql, std::forward<Types>(values)...);
  }

  /**
   * @returns `true` if this connection is not in autocommit mode. Autocommit
   * mode is disabled by a `BEGIN` command and re-enabled by a `COMMIT` or
   * `ROLLBACK` commands.
   *
   * @par Requires
   * `handle()`.
   */
  bool is_transaction_active() const
  {
    if (!handle_)
      throw Exception{"cannot determine transaction status of invalid SQLite connection"};

    return (sqlite3_get_autocommit(handle_) == 0);
  }

  /**
   * @brief Calls the `callback`.
   *
   * @details If the call of `callback` fails with exception and there is an
   * active transaction, an attempt is made to rollback this transaction. If
   * this attempt is successful, the exception thrown by `callback` is rethrown
   * as is, otherwise the exception thrown by `callback` is rethrown with
   * a nested exception of type `Exception`.
   *
   * @tparam F A type of callback.
   */
  template<typename F>
  auto with_rollback_on_error(F&& callback)
  {
    try {
      return callback();
    } catch (...) {
      if (is_transaction_active()) {
        try {
          execute("rollback");
        } catch (...) {
          std::throw_with_nested(Exception{"SQLite ROLLBACK failed"});
        }
      }
      throw;
    }
  }

private:
  sqlite3* handle_{};
};

} // namespace dmitigr::sqlixx

#endif  // DMITIGR_SQLIXX_CONNECTION_HPP
