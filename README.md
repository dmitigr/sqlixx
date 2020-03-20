# Easy to use C++ API for SQLite

`sqlixx` - is a header-only C++ API for SQLite. Just include `sqlixx.hpp`.

**ATTENTION, this software is "alpha" quality, use it at your own risk!**

**ATTENTION, the API is a subject to change!**

Any feedback are [welcome][dmitigr_mail].

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
