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

#include "sqlixx.hpp"
#include <iostream>

int main()
{
  namespace sqlixx = dmitigr::sqlixx;
  sqlixx::Connection c{"", SQLITE_OPEN_READWRITE | SQLITE_OPEN_MEMORY};

  // Create the test table.
  c.execute(
    R"(
    create table if not exists tab(
      id integer primary key,
      cr real,
      ct text,
      cb blob)
    )"
  );

  // Truncate the test table.
  c.execute("delete from tab");

  // Populate the test table.
  auto s = c.prepare("insert into tab(id, cr, ct, cb) values(?, ?, ?, ?)");
  c.execute("begin");
  for (int i = 0; i < 100; ++i)
    s.execute(i, i + 1.1, std::to_string(i), sqlixx::Blob{"blob", 4});
  c.execute("end");

  // Query the test table.
  c.execute([](const sqlixx::Statement& s)
  {
    std::cout << "id: " << s.result<int>("id") << "\n";
    std::cout << "cr: " << s.result<double>("cr") << "\n";
    std::cout << "ct: " << s.result<std::string_view>("ct") << "\n";
    const auto b = s.result<sqlixx::Blob>("cb");
    std::cout << "cb: " << std::string_view{static_cast<const char*>(b.data()), b.size()} << "\n";
    return true;
  },
  "select * from tab where id > ? and id < ?", 5, 10);
}
