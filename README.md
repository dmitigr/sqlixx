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
  sqlixx::Connection c{"test.db", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE};
  c.execute(
    R"(
    create table if not exists tab(
      id integer primary key,
      cr real,
      ct text,
      cb blob)
    )"
  );

  c.execute("delete from tab");

  auto s = c.prepare("insert into tab(id, cr, ct, cb) values(?, ?, ?, ?)");
  c.execute("begin");
  for (int i = 0; i < 1000; ++i)
    s.execute(i, i + 1.1, std::to_string(i), sqlixx::Blob{"blob", 4});
  c.execute("end");

  c.execute([](const sqlixx::Statement& s)
  {
    std::cout << "id: " << s.column_data<int>("id") << "\n";
    std::cout << "cr: " << s.column_data<double>("cr") << "\n";
    std::cout << "ct: " << s.column_data<std::string_view>("ct") << "\n";
    const auto b = s.column_data<sqlixx::Blob>("cb");
    std::cout << "cb: " << std::string_view{static_cast<const char*>(b.data()), b.size()} << "\n";
    return true;
  },
  "select * from tab where id > ? and id < ?", 5, 10);
}
```

## Licenses and copyrights

`sqlixx` itself is distributed under zlib [LICENSE](LICENSE.txt).

[dmitigr_mail]: mailto:dmitigr@gmail.com
