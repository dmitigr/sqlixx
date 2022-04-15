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

#ifndef DMITIGR_SQLIXX_EXCEPTIONS_HPP
#define DMITIGR_SQLIXX_EXCEPTIONS_HPP

#include "errctg.hpp"
#include "../base/exceptions.hpp"

namespace dmitigr::sqlixx {

// -----------------------------------------------------------------------------
// Exception
// -----------------------------------------------------------------------------

/**
 * @ingroup errors
 *
 * @brief The generic exception class.
 */
class Exception : public dmitigr::Exception {
  using dmitigr::Exception::Exception;
};

// -----------------------------------------------------------------------------
// Sqlite_exception
// -----------------------------------------------------------------------------

/// A exception which holds the error originated in SQLite.
class Sqlite_exception final : public Exception {
public:
  /// The constructor.
  Sqlite_exception(const int ev, const std::string& what)
    : Exception{std::error_condition{ev, sqlite_error_category()}, what}
  {}
};

} // namespace dmitigr::sqlixx

#endif  // DMITIGR_SQLIXX_EXCEPTIONS_HPP
