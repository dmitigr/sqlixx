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

#ifndef DMITIGR_SQLIXX_ERRCTG_HPP
#define DMITIGR_SQLIXX_ERRCTG_HPP

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
