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

#include "../../src/base/assert.hpp"
#include "../../src/sqlixx/sqlixx.hpp"

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
  s.execute(0, 1.2, std::to_string(3), sqlixx::Blob{"four", 4});
  s.execute(1, 2.3, std::string_view{"four", 4}, sqlixx::Blob{"five", 4});
  s.execute(2, 3.4, sqlixx::Text_utf8{"five", 4}, sqlixx::Blob{"six", 3});
  c.execute("end");

  // Query the test table.
  c.execute([](const sqlixx::Statement& s)
  {
    const auto b = s.result<sqlixx::Blob>("cb");
    const std::string_view cb{static_cast<const char*>(b.data()), b.size()};
    const auto t1 = s.result<sqlixx::Text_utf8>("ct");
    const auto t2 = s.result<std::string>("ct");
    const auto t3 = s.result<std::string_view>("ct");
    DMITIGR_ASSERT(!std::strcmp(t1.data(), t2.data()) && (t2 == t3));
    std::cout << "id: " << s.result<int>("id") << "\n"
              << "cr: " << s.result<double>("cr") << "\n"
              << "ct: " << t3 << "\n"
              << "cb: " << cb << "\n";
  },
  "select * from tab where id >= ? and id < ?", 0, 3);
}
