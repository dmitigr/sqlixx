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

#include <algorithm>
#include <cassert>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <system_error>

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

class Rowref final {
public:
  Rowref(const int field_count, char** field_values, char** field_names)
    : field_count_{field_count}
    , field_values_{field_values}
    , field_names_{field_names}
  {
    assert(field_count >=0 && field_values && field_names);
  }

  int field_count() const noexcept
  {
    return field_count_;
  }

  int field_index(const char* const field_name) const noexcept
  {
    assert(field_name);
    const auto b = field_names_;
    const auto e = field_names_ + field_count_;
    const auto i = std::find_if(b, e,
      [field_name](const char* const name) { return !std::strcmp(name, field_name); });
    return i != e ? i - b : -1;
  }

  const char* field_name(const int index) const noexcept
  {
    assert(index < field_count());
    return field_names_[index];
  }

  const char* field_value(const int index) const noexcept
  {
    assert(index < field_count());
    return field_values_[index];
  }

  const char* field_value(const char* const field_name) const
  {
    if (const int index = field_index(field_name); index >= 0)
      return field_value(index);
    else
      throw std::logic_error{std::string{"no field with name "}.append(field_name)};
  }

private:
  int field_count_{};
  char** field_values_{};
  char** field_names_{};
};

// =============================================================================

class Db final {
public:
  using Exec_callback = std::function<int(const Rowref&)>;

  ~Db()
  {
    close();
  }

  explicit Db(sqlite3* handle = nullptr)
    : handle_{handle}
  {}

  Db(const std::filesystem::path& path, const int flags)
  {
    if (const int err = sqlite3_open_v2(path.c_str(), &handle_, flags, nullptr))
      throw Exception{err, std::string{"cannot open the database: "}.append(sqlite3_errmsg(handle_))};
    assert(handle_);
  }

  /// Non-copyable.
  Db(const Db&) = delete;

  /// Non-copyable.
  Db& operator=(const Db&) = delete;

  /// The move constructor.
  Db(Db&& rhs) noexcept
    : handle_{rhs.handle_}
  {
    rhs.handle_ = nullptr;
  }

  /// The move assignment operator.
  Db& operator=(Db&& rhs) noexcept
  {
    if (this != &rhs) {
      Db tmp{std::move(rhs)};
      swap(tmp);
    }
    return *this;
  }

  /// The swap operation.
  void swap(Db& other) noexcept
  {
    std::swap(handle_, other.handle_);
  }

  sqlite3* handle() const noexcept
  {
    return handle_;
  }

  operator sqlite3*() const noexcept
  {
    return handle();
  }

  int close() noexcept
  {
    const int result = sqlite3_close(handle_);
    if (!result)
      handle_ = nullptr;
    return result;
  }

  void exec(const char* const sql, Exec_callback callback = {})
  {
    assert(handle());
    assert(sql);
    char* p{};
    const int err = sqlite3_exec(handle_, sql,
      [](void* cbp, int field_count, char** field_values, char** field_names) -> int
      {
        const auto* const callback = static_cast<Exec_callback*>(cbp);
        return (*callback)(Rowref{field_count, field_values, field_names});
      },
      &callback, &p);
    std::unique_ptr<char, void(*)(void*)> errmsg{p, &sqlite3_free};
    if (err)
      throw Exception{err, errmsg.get()};
  }

private:
  sqlite3* handle_{};
};

// =============================================================================

class Stmt final {
public:
  ~Stmt()
  {
    finalize();
  }

  explicit Stmt(sqlite3_stmt* const handle = nullptr)
    : handle_{handle}
  {}

  Stmt(sqlite3* const db, const std::string_view sql, const unsigned int flags = 0)
  {
    assert(db);
    if (const int err = sqlite3_prepare_v3(db, sql.data(), sql.size(), flags, &handle_, nullptr))
      throw Exception{err, std::string{"cannot prepare statement "}.append(sql)};
    assert(handle_);
  }

  /// Non-copyable.
  Stmt(const Stmt&) = delete;

  /// Non-copyable.
  Stmt& operator=(const Stmt&) = delete;

  /// The move constructor.
  Stmt(Stmt&& rhs) noexcept
    : handle_{rhs.handle_}
  {
    rhs.handle_ = nullptr;
  }

  /// The move assignment operator.
  Stmt& operator=(Stmt&& rhs) noexcept
  {
    if (this != &rhs) {
      Stmt tmp{std::move(rhs)};
      swap(tmp);
    }
    return *this;
  }

  /// The swap operation.
  void swap(Stmt& other) noexcept
  {
    std::swap(handle_, other.handle_);
  }

  sqlite3_stmt* handle() const noexcept
  {
    return handle_;
  }

  operator sqlite3_stmt*() const noexcept
  {
    return handle();
  }

  int finalize() noexcept
  {
    const int result = sqlite3_finalize(handle_);
    if (!result)
      handle_ = nullptr;
    return result;
  }

private:
  sqlite3_stmt* handle_{};
};

} // namespace dmitigr::sqliwra

#endif  // DMITIGR_SQLIWRA_HPP
