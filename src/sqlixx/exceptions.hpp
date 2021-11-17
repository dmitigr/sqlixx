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

#ifndef DMITIGR_SQLIXX_EXCEPTIONS_HPP
#define DMITIGR_SQLIXX_EXCEPTIONS_HPP

#include "errctg.hpp"
#include "../error/exceptions.hpp"

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
