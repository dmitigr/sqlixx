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
void example(const std::filesystem::path& path)
{
  dmitigr::sqlixx::Connection c{path, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE};
  c.execute(
    R"(
    create table foo(
      id integer primary key,
      bar integer,
      baz real)
    )";
  );

  auto s = c.prepare("insert into foo(id, bar, baz) values(?, ?, ?)");
  c.execute("begin");
  for (int i = 0; i < 1000; ++i)
    s.execute(i, i + 1, i + 1.1);
  c.execute("end");

  c.execute([](const sqlixx::Statement& s)
  {
    std::cout << "id: "  << s.column_data<std::string_view>("id") << "\n";
    std::cout << "bar: " << s.column_data<int>("bar") << "\n";
    std::cout << "baz: " << s.column_data<double>("baz") << "\n\n";
  },
  "select * from foo where id > ? and id < ?", 5, 10);
}
```

## Licenses and copyrights

`sqlixx` itself is distributed under zlib [LICENSE](LICENSE.txt).

[dmitigr_mail]: mailto:dmitigr@gmail.com
