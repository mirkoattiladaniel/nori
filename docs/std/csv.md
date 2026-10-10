# std/csv

```nori
import "std/csv" as csv
```

std/csv: RFC 4180 CSV parse + write. `parse` turns CSV text into rows of string fields
(handling quoted fields with embedded commas/newlines and `""` escapes); `write` serializes rows back,
quoting any field that needs it. Round-trips.
  import "std/csv" as csv
  let rows = csv::parse("a,b\n1,\"x,y\"\n")   // [["a","b"], ["1","x,y"]]
  csv::write(rows)
### `fn parse(text: Str) -> Vec<Vec<Str>>`

parse CSV `text` into rows of fields. CR, LF, and CRLF all end a row; `"`-quoted fields may contain
commas/newlines and `""` for a literal quote.

### `fn quote_field(f: Str) -> Str`

`f` quoted per RFC 4180 if it needs it (wrap in `"`, double inner quotes); otherwise returned as-is.

### `fn write_row(fields: Vec<Str>) -> Str`

serialize one row of fields to a CSV line (no trailing newline).

### `fn write(rows: Vec<Vec<Str>>) -> Str`

serialize rows to CSV text, each row terminated by `\n`.


