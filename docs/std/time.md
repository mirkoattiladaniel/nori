# std/time

```nori
import "std/time" as time
```

std/time: monotonic + wall clocks and a UTC calendar breakdown. No C shim: each platform
reads its OS clock through `extern fn` into the system libc (linked automatically by clang).
Platform-specific code lives in `cfg(...)` blocks selected by the build target (`--os`, set by roll from
the host); everything below the blocks is shared.
### `fn now_ns() -> Int`

monotonic time in nanoseconds, for measuring durations (not affected by clock changes).

### `fn now_ms() -> Int`

monotonic time in milliseconds.

### `fn unix_ns() -> Int`

wall-clock time in nanoseconds since the Unix epoch (1970-01-01 UTC).

### `fn unix_ms() -> Int`

wall-clock time in milliseconds since the Unix epoch.

### `fn unix_secs() -> Int`

wall-clock time in whole seconds since the Unix epoch.

### `fn sleep_ms(ms: Int) -> Int`

sleep for `ms` milliseconds.

### `fn breakdown(secs: Int) -> DateTime`

the UTC civil date/time for a Unix timestamp (seconds), via Howard Hinnant's days<->civil algorithm
(valid for any date; weekday 0=Sunday). Assumes secs >= 0.

### `fn now() -> DateTime`

the current UTC date/time.

### `fn days_from_civil(y: Int, m: Int, d: Int) -> Int`

days since the Unix epoch (1970-01-01) for a UTC civil date y/m/d, the exact inverse of `breakdown`
(Howard Hinnant's days_from_civil; valid for any date).

### `fn time_to_unix(dt: DateTime) -> Int`

UTC DateTime -> Unix seconds (inverse of `breakdown`).

### `fn weekday_name(w: Int) -> Str`

weekday name for `w` (0=Sunday..6=Saturday, matching `breakdown`); out of range -> "".

### `fn days_in_month(y: Int, m: Int) -> Int`

how many days month `m` has in year `y` (Gregorian; February follows the leap rule).

### `fn month_name(m: Int) -> Str`

month name for `m` (1=January..12=December); out of range -> "".

### `fn iso8601(dt: DateTime) -> Str`

ISO-8601 UTC string "YYYY-MM-DDTHH:MM:SSZ".

### `fn date_string(dt: DateTime) -> Str`

the date part "YYYY-MM-DD".

### `fn time_string(dt: DateTime) -> Str`

the time part "HH:MM:SS".

### `fn format_dt(dt: DateTime, fmt: Str) -> Str`

strftime subset: %Y %m %d %H %M %S %y %A %a %B %b %%; any other char is copied literally.

### `fn parse_iso8601(s: Str) -> Int`

parse "YYYY-MM-DDTHH:MM:SSZ" (a space may replace 'T'; trailing 'Z' optional) -> Unix seconds; -1 if
it does not look well-formed.


