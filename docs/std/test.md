# std/test

```nori
import "std/test" as test
```

std/test: assertions for native `.nori` tests. Write tests as `fn test_*()` functions that call
these asserts; run them with `noric --test FILE` (or `roll test`), which finds every `fn test_*`,
runs it, and reports pass/fail. Asserts don't abort on failure; they record it (so a test reports
all its failures) and the runner fails the test if its assert-failure count went up.

  import "std/test"
  import "std/iter" as it
  fn test_vsum() {
    assert_eq(it::vsum(it::vmap(mkv(), |x| x * 2)), 12)
    assert_true(it::vany(mkv(), |x| x == 3))
  }
### `fn assert_file(f: Str) -> Int`

the file the tests are in. Set once by the generated harness.

Separate from `assert_at` so that what gets injected in front of an assert is as short as it can
be: the harness prefixes the line, which shifts every column on it, and a compiler error on that
line shows the injected call inside the source it quotes. `assert_at(12)` is short; a full path
would not be.

### `fn assert_at(l: Int) -> Int`

record which line the next assert is on. Called by the generated harness, not by hand.

### `fn assert_failures() -> Int`

the number of assert failures recorded so far (the runner brackets each test with this).

### `fn assert_reset() -> Int`

reset the recorded assert-failure count to zero.

### `fn assert_true(c: Bool) -> Int`

assert `c` is true; records a failure and prints "got false" otherwise.

### `fn assert_false(c: Bool) -> Int`

assert `c` is false; records a failure and prints "got true" otherwise.

### `fn assert_eq(got: Int, want: Int) -> Int`

assert `got == want`; records a failure and prints "want X, got Y" otherwise.

### `fn assert_ne(got: Int, bad: Int) -> Int`

assert `got != bad`; records a failure and prints the offending value otherwise.

### `fn assert_streq(got: Str, want: Str) -> Int`

assert strings `got` and `want` are equal; records a failure and prints "want X, got Y" otherwise.

### `fn assert_feq(got: Float, want: Float, eps: Float) -> Int`

approximate float equality: fails if |got - want| > eps.


