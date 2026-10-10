# 16. Concurrency

Concurrency is safe by default: the compiler proves the absence of data races
rather than trusting you to. Values are handles that alias within a task (see
[The Memory Model](15_memory_model.md)), so a mutable by-reference value simply
**cannot cross into a task**: the checker rejects it, and concurrent code
communicates by moving values, channels, and `Shared<T>` instead. This
chapter covers tasks, data parallelism, channels, locks, and deadline-driven
control flow.

## Tasks: `spawn` and `await` {#tasks}

`spawn` starts a concurrent task and returns a `Task<T>`; `await` blocks until it
finishes and yields its value. You can spawn a function call or an arbitrary
block.

```nori
fn work(x: Int) -> Int { return x * x }

fn main() -> Int {
    let t  = spawn work(6)                       // Task<Int>
    let t2 = spawn { var s = 0  var i = 0  while i < 100 { s = s + i  i = i + 1 }  s }
    printl(to_str(await t))                      // 36
    printl(to_str(await t2))                     // 4950
    return 0
}
```

Tasks run on a work-stealing thread pool. `sync()` waits for all outstanding
tasks to complete.

## How tasks execute: stackless, and uncolored

A task does not get its own call stack. The compiler finds every function that can
reach a pause point — `await`, `co_sleep`, a channel op, a lock, a socket read — and
rewrites it into a state machine over a heap-allocated frame. Pausing stores the
live locals into the frame and returns; resuming re-enters at the saved state.

This is whole-program and automatic, so there is **no function coloring**: no `async`
keyword, no separate async/sync versions of a function, and no color to thread
through generics, closures or trait methods. An ordinary function becomes suspendable
by calling something that suspends, and callers follow. A call through a closure value
counts as a call to every lambda the program holds as a value; a `spawn { }` block or a
`select`/`within` branch is a lambda only the scheduler enters, so it is not among them.
`noric --suspendable` reports which functions those are, and why:

```
$ noric --suspendable app.nori
SUSPENDABLE: 9 of 69 functions
  ...
  fetch_one: fetch_one -> net__tcp_recv [nori_net_recv]
  fetch_all: fetch_all -> fetch_one -> net__tcp_recv [nori_net_recv]
```

Each line gives the function, the call path that makes it suspendable, and the
primitive it bottoms out in.

Two consequences worth knowing. Recursion depth in a suspendable function is bound by
the heap rather than a fixed stack size. And because a paused task is just a chain of
heap frames — ordinary serializable data — a task that is still in flight can be
written into a program image and resumed in a later process; see
[Program images](04_variables.md#program-images-snapshot--resume).

## Data parallelism {#auto_par}

### `parallel { … }`, fork/join

Run several independent `let` bindings concurrently and join at the closing brace;
the bindings are available afterward:

```nori,excerpt
fn main() -> Int {
    parallel {
        let a = expensive(1)
        let b = expensive(2)
        let c = expensive(3)
    }
    printl(to_str(a + b + c))     // a, b, c computed in parallel
    return 0
}
```

### `parallel foreach`, parallel loops

Run a loop's iterations in parallel. The iterations must be independent (the
compiler rejects a racy body).

```nori
var a = fill(1000, 0)
parallel foreach i in 0..1000 { a[i] = i * i }
```

### `parallel reduce`, parallel reduction

Combine a range in parallel with an associative operator and an identity, via
`parallel reduce(OP, INIT) over VAR in LO..HI { BODY }`: `BODY` produces each
element and `OP`/`INIT` fold them:

```nori
let s = parallel reduce(+, 0) over i in 0..1000 { i }      // 499500
let p = parallel reduce(*, 1) over i in 1..6 { i }         // 120
var a = fill(100, 0)
let t = parallel reduce(+, 0) over i in 0..100 { a[i] }    // sum of a
```

These lower onto the same pool; a parallel map form (producing a `[T]`) exists
too.

### Automatic parallelism

With `--auto-par` (on by default in `roll`'s dev and release profiles), the
compiler **proves** an ordinary `foreach` has independent iterations and runs it
in parallel with **no annotation**; the source is unchanged and the result is
bit-identical to the serial run. `parallel foreach` is the explicit form for when
you want to state it yourself.

Independence is necessary but not sufficient: the loop also has to be **worth**
parallelizing, and what a fork/join costs depends on the machine the binary is
*running* on, not the one it was built on. So every eligible loop is compiled with
*both* forms behind a trip guard, and the guard's threshold is **calibrated at run
time**: at the first guarded loop the runtime measures one fork/join on the actual
hardware (once, ~1 ms) and scales its profitability bars from that measurement plus
the machine's core count. One shipped binary makes the right serial/parallel call on
a 4-core laptop and a 32-core server. The guard declines two cases that look
tempting but lose:

- **The loop is small.** A short `v` in `foreach x in 0..len(v)` runs serially, a
  long one in parallel, decided per execution, against the measured fork cost. Only
  a loop that is provably too small for *any* machine (a constant handful of
  iterations) skips the guard and is settled serial at build time.
- **The serial form would vectorize.** An elementwise loop over a `fill` array is
  the vectorizer's best case, and *scalar* parallel bodies would race SIMD for the
  same memory bandwidth. Such a loop stays serial until it is very large (larger
  still on machines with few cores, which is why that bar scales with the pool
  width), and a loop over narrow floats (`[F32]`/`[F64]`) is left to the vectorizer
  outright.

So `--auto-par` never makes a loop slower than the serial build, which is the
property that matters; you should not have to think about whether to turn it off.
`NORI_AP_DEBUG=1` makes any `--auto-par` binary print what its calibration measured. (Deterministic reductions
are the one case still decided at build time, with the reference thresholds.)

A loop body may declare a scalar `let`/`var`, and may contain a nested loop —
`foreach` or `while`. Those locals are **privatized**: each iteration gets its own
cell, so a temporary (or a `while` counter) is not shared between threads. This
matters because they are the loops that actually scale; a bare elementwise map is
limited by memory bandwidth, while a body doing real per-element work with a
temporary keeps every core busy.

```nori,excerpt
foreach k in 0..n {          // parallelizes: `v` is per-iteration
    var v = a[k] + k
    v = v * v + 7
    out[k] = v
}

foreach i in 0..rows {       // parallelizes the outer loop; `j` is per-iteration
    foreach j in 0..cols { g[i, j] = i * j }
}

foreach k in 0..n {          // a `while` over privatized locals parallelizes too
    var v = a[k]
    var j = 0
    while j < 200 { v = v * v + 7  v = v % 1000003  j = j + 1 }
    out[k] = v
}
```

A local the compiler cannot make per-iteration (a `Vec`, `Str`, struct or other
heap value, because those are reclaimed per function rather than per iteration) is
still shared, so it is refused by name rather than silently raced:

```text
error: `s` cannot be privatized, so it would be shared across threads
  note: only scalar locals get a per-iteration cell — a container, Str or struct
        local is reclaimed per function, not per iteration
  fix:  hoist it out of the loop, index into a pre-sized array, or use a serial foreach
```

`match`, `break`/`continue`, `return`, `spawn` and `lock` are still not allowed
inside a parallel body (`break`/`continue` are rejected even inside a nested
`while` there).

### Seeing the verdicts

A loop that silently stays serial can cost an order of magnitude, so the verdicts
are inspectable. `noric --build app.nori app --par-report` prints one line per
analyzable `foreach` to stderr: either that it got the run-time guard, or exactly
why it stays serial:

```text
app.nori:7:  ⟦par⟧ parallel — run-time trip guard (the machine it runs on decides)
app.nori:12: ⟦par⟧ serial — a read races the loop's writes (another index of a
             written array), or a call is not proven pure (I/O, global write, inout, extern)
```

`roll xray` shows the same verdicts inline as per-line `⟦par⟧` annotations,
alongside the lifeline and bounds-check-erasure report.

## Channels

`channel()` creates a `Channel<T>`. `chan_send` puts a value; `chan_recv` takes
one (blocking until available); `foreach v in ch` drains a channel until it
closes.

```nori
fn main() -> Int {
    let ch = channel()
    let producer = spawn {
        var i = 0
        while i < 3 { chan_send(ch, i)  i = i + 1 }
        chan_close(ch)
        0
    }
    foreach v in ch { printl(to_str(v)) }   // 0 1 2
    return 0
}
```

## Mutexes and `lock`

`mutex(v)` creates a `Mutex` guarding an initial value `v`. `lock m as x { … }`
enters the critical section, binding the guarded value as `x`; assignments to `x`
inside are written back on release.

```nori
fn worker(m: Mutex, n: Int) {
    foreach i in 0..n { lock m as x { x = x + 1 } }   // atomic increment
}

fn main() -> Int {
    var c = mutex(0)
    let a = spawn { worker(c, 1000)  0 }
    let b = spawn { worker(c, 1000)  0 }
    sync()
    lock c as x { printl(to_str(x)) }                 // 2000 — race-free
    return 0
}
```

### Lock order is checked

Holding two locks is fine; holding them in two different **orders** is a deadlock waiting for the two
paths to interleave. Every `lock` on a global — a `durable var` or a global mutex — is visible to the
compiler, so taking the same pair both ways is a **build error**:

```
◆ these two locks can deadlock: `a` then `b` in `one`, but `b` then `a` here
  ▸ fix · lock them in one order everywhere (e.g. `a` before `b`), or hold only one at a time
```

The check is whole-program: a lock taken by a function *called* inside a `lock` body counts, which is
where an inversion usually hides; the two `lock`s are rarely next to each other in the source. A lock
on a **local** `Mutex` value takes part in no ordering: it has no whole-program identity, so a helper
that locks its own mutex is never reported.

## Session types: `protocol` {#protocol}

A `Channel<T>` says *what* can travel through it. A **protocol** says *what order, in which direction,
and when the conversation ends*, and the compiler checks it:

```nori,setup
protocol Fetch {
    client sends Str        // the url
    server sends Str        // the header
    server sends Str        // the body
}
```

Two roles, named by you; the steps run top to bottom. A channel typed by the protocol has two ends,
`Fetch<client>` and `Fetch<server>`, and each carries its **position** in the conversation:

```nori
fn serve(s: Fetch<server>) -> Int {
    let url = chan_recv(s)              // step 1: the client speaks
    chan_send(s, "200 OK")              // step 2
    chan_send(s, "<html>" + url)        // step 3 — and the protocol is complete
    return 0
}

fn main() -> Int {
    let c: Fetch<client> = session()    // a fresh session; c is the client end
    let s: Fetch<server> = peer(c)      // …and s is the other end
    let t = spawn serve(s)              // hand the server end to a task
    chan_send(c, "/index.html")
    printl(chan_recv(c))                // 200 OK
    printl(chan_recv(c))                // <html>/index.html
    let _r = await t
    return 0
}
```

`session()` creates a session and gives you the end your annotation names; `peer(e)` gives you the
other. `chan_send`/`chan_recv` are the ordinary channel operations; on an endpoint they follow the
protocol instead of taking anything at any time.

Each of these is a **build error**, not a hang:

| you wrote | the compiler says |
|---|---|
| send when the peer's step is next | `` `s` cannot send here: step 1 of `Fetch` is `client sends Str` `` |
| return with steps left | `` `s` is left mid-conversation … step 3 … never happens `` |
| an operation after the last step | `` `s` has finished its `Fetch` conversation `` |
| a payload the step doesn't declare | ``step 1 of `Fetch` sends `Str`, but this value is `Int` `` |

The second one is the interesting one: it is the classic "server forgot the last message, client waits
forever" bug, found before the program runs.

An endpoint may be handed to a function or a task, at the **start** of its protocol; the callee then
owns the rest of the conversation. That is why `spawn serve(s)` is allowed even though a handle
normally cannot cross into a task: an endpoint is *moved*, not shared, so exactly one task holds it.

### Repetition: `streams`

A step that happens zero or more times is a **stream**. The sender writes as many messages as it likes
and then closes it; the receiver drains it with `foreach`, and the conversation carries on afterwards:

```nori
protocol Download {
    client sends Str        // the path
    server sends Str        // a header
    server streams Str      // zero or more chunks
    server sends Int        // how many there were
}

fn serve(s: Download<server>) -> Int {
    let path = chan_recv(s)
    chan_send(s, "200 " + path)
    var i = 0
    while i < 3 { chan_send(s, "chunk" + to_str(i))  i = i + 1 }   // as many as you want
    chan_close(s)                                                   // …and that's the stream
    chan_send(s, i)
    return 0
}
```

```nori,excerpt
foreach part in c { printl(part) }    // drains until the sender closes it
printl(chan_recv(c))                  // the step after the stream
```

A `streams` step is the only one a loop may repeat; any other step in a loop is still refused. Only
the sending role may `chan_close`; the receiving side drains. An empty stream is fine.

### Branching: `chooses`

One role picks a branch; the other must handle **every** one, and each branch has its own
continuation:

```nori
protocol Login {
    client sends Str
    server chooses {
        ok:     server sends Str      // the token
        denied: server sends Str      // the reason
    }
}
```

```nori,excerpt
// the side that decides
if good { let _c = chan_choose(s, "ok")      chan_send(s, "token-for-" + user) }
else    { let _c = chan_choose(s, "denied")  chan_send(s, "no such user") }

// the side that must cope with either
match chan_offer(c) {
    "ok"     => { printl("welcome: " + chan_recv(c)) }
    "denied" => { printl("rejected: " + chan_recv(c)) }
}
```

Leaving out the `denied` arm is a build error (`branch `denied` is not handled`), and so is an arm that
doesn't finish its own branch; the peer would still be waiting for the rest. A `_` arm is refused,
because each branch continues differently and a wildcard cannot say which.

### Why these sessions do not deadlock

The two ends of **one** session cannot deadlock by construction: the roles are dual, so one side sends
exactly where the other receives, and the rules above already prove each side follows its role. That
leaves three ways a program could still wait forever, and each is a build error:

- **The other end never exists.** `session()` whose `peer` is never taken, or taken and then dropped:
  the first blocking step waits for a partner that will never speak.
- **Both ends driven by one task, in an impossible order**: a receive whose message this same task
  only sends later. There is nobody else to make progress.
- **Two sessions interleaved in opposite orders.** One task steps `P` before `Q` while another does the
  reverse: each can hold what the other is waiting for. This is lock-order inversion in a different
  costume, and it is refused the same way.

```
◆ these two sessions can deadlock: `P` is stepped before `Q` in `t1`, and the other way round here
  ▸ fix · interleave them in one order everywhere, or finish one conversation before starting the other
```

The first two are exact. The third is conservative in the safe direction: a refused program might have
been fine, but an accepted one has no cycle among its sessions. **What is not covered:** a partner
blocked on something outside the session system (a mutex, a socket read, an `await` on a task that
never finishes). Sessions make the *conversation* deadlock-free, not the whole program.

### Branches nest

A branch may hold anything a protocol may: more sends, a stream, another choice. Braces make a
multi-step branch readable:

```nori
protocol Api {
    client sends Str
    server chooses {
        ok: {
            server sends Str        // content type
            server streams Str      // the body, in chunks
            server sends Int        // how many chunks
        }
        error: { server sends Str }
    }
}
```

The conversation resumes at the right place however deep it went, and every rule above applies at every
depth: an unhandled label inside an inner choice is the same build error as one at the top.

**One restriction:** all `streams` steps of a protocol carry the same element type, because `foreach`
binds its loop variable before the checker knows which stream it is draining. Two different types in one
protocol is refused at the declaration, with that reason.

At runtime a session between tasks is two ordinary channels, one per direction; a stream is framed (a
marker before each element, and one to end it) and a choice sends its branch index.

### A session over a socket

The same protocol can carry a conversation between two **programs**. Put the declaration in a module
both import, and open the endpoint on a socket instead of in memory:

```nori
// deal.nori — the contract, imported by both binaries
pub protocol Deal {
    client sends Str
    server chooses {
        ok: { server sends Str  server streams Str  server sends Int }
        no: { server sends Str }
    }
}
```

```nori,excerpt
// the server, its own program
import "std/net" as net
import "deal.nori" as deal

fn main() -> Int {
    let srv = net::tcp_listen4(net::ipv4(127, 0, 0, 1), 8080, 4)
    let conn = net::tcp_accept(srv)
    let s: deal::Deal<server> = session_over(conn)      // ← the endpoint rides the socket
    let want = chan_recv(s)
    let _c = chan_choose(s, "no")
    chan_send(s, "404 " + want)
    net::tcp_close(conn)
    return 0
}
```

```nori,excerpt
// the client, a different program, compiled separately
import "std/net" as net
import "deal.nori" as deal

fn main() -> Int {
    let sock = net::tcp_connect4(net::ipv4(127, 0, 0, 1), 8080)
    let c: deal::Deal<client> = session_over(sock)
    chan_send(c, "/big")
    match chan_offer(c) {
        "ok" => { printl(chan_recv(c))  foreach part in c { printl(part) }  printl(chan_recv(c)) }
        "no" => { printl(chan_recv(c)) }
    }
    net::tcp_close(sock)
    return 0
}
```

Neither compiler sees the other's code; both check their side against the same declaration, so the
protocol *is* the contract. The step sequence, the rules, and the diagnostics are identical to the
in-process case; only the transport differs. On the wire each message is its type's serialization
(the same one `durable var` uses) inside a length frame, so the wire format comes from the protocol
rather than being designed separately.

Two things to know:

- **A socket step parks the task**, exactly like `std/net`'s recv: a step that has to wait suspends the
  conversation and hands the worker thread back, so one worker can carry as many sessions as you spawn.
  Nothing in the code says so — `chan_recv` looks the same either way — and there is no colouring to
  propagate. (Outside a task, in a plain non-suspendable function, the same step just waits: there is no
  task to park.)
- **The peer can vanish.** If the connection drops mid-conversation the step traps with
  `the session's peer closed the connection mid-conversation` rather than decoding whatever arrives
  next. A protocol makes the conversation well-formed; it cannot make the network reliable.

An accept loop is the natural server shape, and the rule about steps in loops accounts for it: a
session **created inside** the loop is a fresh conversation each iteration, so its steps are fine
there. Only an endpoint made *outside* a loop may not repeat its steps inside one.

## Race safety {#race_safety}

Sharing a mutable value across tasks without synchronization is a **compile
error**, not a runtime hazard: the same analysis that erases memory overhead also
proves your parallelism is race-free. If it compiles, it does not have a data
race. Mutexes and channels are how you share mutable state on purpose.

### What may cross into a task

A task receives caller data only through its arguments (there are no closures), so
the rule lives there. Safe on their own: scalars (copied by value), `Str`
(immutable), `Channel`, `Shared`, a `Mutex` handle, and a session endpoint (which
is *moved*, not shared). A `frozen global` is readable from any task, because it is
written only before the first `spawn`.

A **struct** is a mutable handle, so passing one directly is refused: two tasks
would hold one object:

```nori,error
let cfg = Cfg { name: "site", n: 1 }
spawn worker(cfg)        // ERROR: a `Cfg` can't be shared with a task
spawn worker(copy cfg)   // fine: the task gets its own Cfg
```

`copy` is what makes it safe, and it is a real copy: later writes to `cfg` are
invisible to the task. It works when every field is itself share-safe (a scalar or
a `Str`). A field that is a `Vec`, `Map`, or another struct is still a shared
handle after the copy, so that is refused too, naming the field:

```
a `Cfg` can't be shared with a task even copied — its field `items` is a `Vec<Int>`
```

Put mutable state behind a `mutex`, or send it over a `Channel`; a struct may
travel by channel, because a channel hands it over rather than sharing it.

This is what lets a library take a caller's struct and run it in a task per unit of
work: `std/http`'s `serve<H: Handler>` copies your handler into each connection's
task, which is why a handler is a struct of scalars and Strs.

## Deadline-driven control flow

Nori has first-class control flow for time and cancellation, built on the
coroutine scheduler. A timed-out task is **truly cancelled**: blocked I/O is
interrupted and the work stops, not merely ignored.

### `within … else`, deadline with a fallback

Run a block with a deadline; take its value if it finishes in time, else the
**lazy** `else` fallback (evaluated only on timeout). The timed-out task is
cancelled.

```nori,excerpt
let layout = within 8ms { full_solve(doc) } else { cached_layout }
```

### `within …` (no `else`) → `Timed<T>`

Without an `else`, `within` yields a `Timed<T>` you match:

```nori,excerpt
match within 8ms { solve() } {
    Timed::Done(v)  => { use(v) }
    Timed::TimedOut => { fallback() }
}
```

### Anytime `yield`, harvest a best-so-far

Inside a `within … else` block, `yield best` publishes a partial result; on
timeout, `within` returns the **last** value yielded (or the `else` fallback if
none was), instead of discarding the work.

```nori,excerpt
let est = within 20ms {
    var best = rough(doc)  yield best
    foreach pass in passes { best = refine(best, pass)  yield best }
    best
} else { fallback }        // times out -> the last `yield best`
```

`yield` publishes a **copy** of a heap value, so the block keeps using and
rebinding `best` freely; the caller owns what `within` hands back, and a best that
was published but never needed is freed with the block. A scalar is copied by
value as always.

### `race { A } or { B }` and `hedge N { A } or { B }`

`race` runs both branches, takes the first to finish, and cancels the loser.
`hedge` runs `A` and only starts the backup `B` if `A` is still going after the
delay (the standard tail-latency hedge).

```nori,excerpt
let ans = race { solver_a(q) } or { solver_b(q) }
let hot = hedge 5ms { replica_a() } or { replica_b() }
```

### `select { recv … after … }`

Wait on several channels and/or a timeout; the first ready branch wins, the rest
are cancelled.

```nori,excerpt
let ev = select {
    recv m = inbox => handle(m)
    recv c = ctrl  => control(c)
    after 100ms    => tick()
}
```

### `until` → `Deferred<T>` (stale-while-revalidate)

`{ B } until { P }` returns a `Deferred<T>` immediately: `.now()` reads the
placeholder `P` until the still-running `B` completes, then the real value.
`within N { B } until { P } else { E }` adds a deadline: show `P` for `N`, then
`E`, and the real `B` wins whenever it lands. Unlike `else`, `until` never cancels
the task.

```nori,excerpt
let title = { fetch_title() } until { "Loading…" }
render(title.now())        // "Loading…" until fetch lands, then the title
```

### Cooperative cancellation

A CPU-bound loop inside a cancellable block can call `cancel_point()` to let a
timeout stop it at a safe spot. `co_sleep(ms)` is the async sleep (it parks the
task without blocking a thread).

### Platform support

The scheduler that drives those heap frames is itself written in Nori, with no
platform-specific code, so `spawn`/`await`/`sync`, `Mutex`, `Shared`, channels,
`within`/`race`/`select`/`hedge`, `co_sleep` and `cancel_point` behave the same on
Linux, Windows and macOS.

Async socket IO needs an OS readiness backend on top of that: Linux uses epoll and
Windows uses WSAPoll, so socket handlers park rather than holding a thread on both.
macOS has no backend yet and a socket call there aborts with a clear message.
See [std/net](../std/net.md).

macOS is stubbed rather than supported: the `cfg(macos)` paths exist and compile,
but nothing there has been run on real hardware.

---

Next: [Unsafe and FFI](17_unsafe_and_ffi.md).
