# std/log

```nori
import "std/log" as log
```

std/log: leveled logging. Four levels (DEBUG < INFO < WARN < ERROR) gated by a global threshold
`g_log_level` (default INFO); messages below it are suppressed. An optional `HH:MM:SS` timestamp
prefix is toggled by `set_timestamps`. Output goes through `printl`.

  import "std/log" as log
  log::set_level(log::DEBUG())
  log::info("server up")        // -> [INFO] server up
  log::set_timestamps(true)
  log::warn("disk low")         // -> 14:03:09 [WARN] disk low
### `global g_log_level: Int = 1`

the active level threshold (DEBUG=0, INFO=1, WARN=2, ERROR=3); default INFO. Messages below it are dropped.

### `global g_log_time: Int = 0`

whether to prefix each line with an `HH:MM:SS` timestamp (0 = off, 1 = on); default off.

### `fn DEBUG() -> Int`

level constant: DEBUG (0), the most verbose level.

### `fn INFO() -> Int`

level constant: INFO (1), informational messages.

### `fn WARN() -> Int`

level constant: WARN (2), warnings.

### `fn ERROR() -> Int`

level constant: ERROR (3), errors.

### `fn set_level(l: Int) -> Int`

set the level threshold; messages below `l` are suppressed.

### `fn get_level() -> Int`

get the current level threshold.

### `fn set_timestamps(on: Bool) -> Int`

turn the `HH:MM:SS` timestamp prefix on or off.

### `fn debug(msg: Str) -> Int`

log a DEBUG-level message.

### `fn info(msg: Str) -> Int`

log an INFO-level message.

### `fn warn(msg: Str) -> Int`

log a WARN-level message.

### `fn error(msg: Str) -> Int`

log an ERROR-level message.


