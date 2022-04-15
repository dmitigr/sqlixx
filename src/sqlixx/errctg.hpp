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

#ifndef DMITIGR_SQLIXX_ERRCTG_HPP
#define DMITIGR_SQLIXX_ERRCTG_HPP

#include <string>
#include <system_error>

namespace dmitigr::sqlixx {

// -----------------------------------------------------------------------------
// SQLite error category
// -----------------------------------------------------------------------------

/// The category of SQLite errors.
class Sqlite_error_category final : public std::error_category {
public:
  /// @returns The literal `dmitigr_sqlixx_sqlite_error`.
  const char* name() const noexcept override
  {
    return "dmitigr_sqlixx_sqlite_error";
  }

  /// @returns The string that describes the error condition denoted by `ev`.
  std::string message(const int ev) const override
  {
    return std::string{name()}.append(" ").append(std::to_string(ev));
  }
};

/**
 * @ingroup errors
 *
 * @returns The reference to the instance of type Sqlite_error_category.
 */
inline const Sqlite_error_category& sqlite_error_category() noexcept
{
  static const Sqlite_error_category result;
  return result;
}

} // namespace dmitigr::sqlixx

#endif  // DMITIGR_SQLIXX_ERRCTG_HPP
