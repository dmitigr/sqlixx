# Easy to use C++ API for SQLite

`sqlixx` - is a header-only C++ API for SQLite. Just include `sqlixx.hpp`.

To use this library, a compiler with C++17 support (such as GCC 7.5+) is required.

**ATTENTION, the API is a subject to change!**

Any feedback are [welcome][dmitigr_mail].

## Features

  - easy to use library in a single header;
  - almost no overhead compared to native C API;
  - zero-based indices for both prepared statement parameters and result columns;
  - easy to use extensible data type conversion system.

## Example

```cpp
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
```

## Licenses and copyrights

`sqlixx` itself is distributed under zlib [LICENSE](LICENSE.txt).

Copyright (C) 2020 Dmitry Igrishin

[dmitigr_mail]: mailto:dmitigr@gmail.com
