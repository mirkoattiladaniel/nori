# Nori Snake (raylib)

A small, playable Snake game in Nori, rendered with [raylib](https://www.raylib.com/), and an
end-to-end exercise of Nori's C interop. Arrow keys or WASD to steer, eat the red dots, `R` to restart.

## Build & run

```sh
./build-raylib.sh          # downloads raylib 5.5 and builds a shared libraylib.so into vendor/ (needs git + cmake)
roll run
```

raylib is [zlib-licensed](https://github.com/raysan5/raylib/blob/master/LICENSE) and is downloaded by the
script; it is not shipped here (`vendor/` is git-ignored).

A native build links C libraries dynamically. The `-Lvendor` in the link line is where the build finds
`libraylib.so`, and the program records that directory as its run path, relative to the program itself
(`$ORIGIN/../../vendor`), so it starts without `LD_LIBRARY_PATH`, also after the project is moved.
raylib pulls in the windowing and OpenGL libraries at run time, so the link line in `src/main.nori` is just
`link "-Lvendor raylib"`.

The `extern c { }` helpers are compiled with the system C compiler (clang) into a small shared object
beside the program (`build/dev/raylib-snake.inline-c.so`), which the program imports them from.

## What it exercises

The library goes through `extern raylib "vendor/raylib.h" link "..."` with no hand-written glue: the
compiler reads the header and generates the marshalling. The game's own helpers are C written inline.

| C-interop feature | Where in the game |
|---|---|
| **namespaced bindgen** (read the header, bind the whole API under `raylib::`) | `raylib::InitWindow`, `raylib::DrawRectangle`, `raylib::IsKeyPressed`, ... (they read distinctly from your own `body_hit`/`each_cell`) |
| **by-value structs** (params) | `raylib::Color` / `raylib::Vector2` passed to `ClearBackground`, `DrawRectangle`, `DrawCircleV` |
| **enums** | `raylib::KEY_RIGHT`, `raylib::KEY_W`, `raylib::KEY_R`, ... (raylib's `typedef enum`) |
| **floats** (`F32`) | `DrawCircleV(center, radius, color)` |
| **strings** (`Str` to `const char*`) | the HUD text and the game-over banner passed to `DrawText` |
| **inline C** | `extern c { ... }`: `body_hit`, `each_cell`, `hud_label`, compiled and linked by the build |
| **array marshalling** (`Vec<I32>` to `int*`) | the snake body passed to `body_hit` / `each_cell` |
| **callbacks** (C calls back into a Nori closure) | `each_cell(xs, ys, n, \|x, y\| raylib::DrawRectangle(...))` |
| **captured closure** | that draw closure captures the cell color (a `Color` struct) |
| **string return** (`const char*` to `Str`) | `hud_label(...)` builds the HUD text with `snprintf` |

It opens a window, so there is no output to compare; `tests/check_examples.sh` builds it and, under Xvfb
when `xvfb-run` is there, starts it for a few seconds (only when `vendor/libraylib.so` exists).
