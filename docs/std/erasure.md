# std/erasure

```nori
import "std/erasure" as erasure
```

Reed–Solomon erasure coding over GF(2^8).

k data shards in, m parity shards out. Any k of the k+m reconstruct the whole set, so m losses
of any kind are survivable. Against keeping whole copies that is a large difference in what
storage buys: two replicas cost 2x and survive one loss, while RS(8,4) costs 1.5x and survives
four.

Systematic, which matters more than it sounds. The k data shards come out unchanged (the code
is appended, not applied), so a reader that has the data reads it as before and never
touches parity or does any arithmetic at all. Only a degraded read, where something is missing,
pays anything.

Cauchy, not Vandermonde. The generator is the identity stacked on a Cauchy matrix
C[i][j] = 1/(x_i + y_j) over disjoint x and y. Every square submatrix of a Cauchy matrix is
invertible, which is the property recovery depends on: any k surviving rows must give a
solvable system. A Vandermonde matrix does not guarantee that, and the failure is not a wrong
answer but a set of k shards that cannot be inverted at all, discovered only when something
has already been lost and it is too late to choose differently.

The field is GF(2^8) modulo 0x11d with generator 2. Multiplication goes through a full 64KB
product table built once: the inner loop is one lookup and one XOR per byte, which is what
makes coding megabyte shards in this language reasonable.
### `fn gf_mul_table() -> Vec<Int>`

The 256x256 product table. Built once and passed around: without it every byte of every shard
costs two log lookups, an add and an exp lookup, and the inner loop is where all the time goes.

### `fn rs_matrix(k: Int, m: Int, mt: Vec<Int>) -> Vec<Int>`

The m x k Cauchy matrix, row-major. x_i = k+i and y_j = j are disjoint by construction, which
is what makes every square submatrix invertible.

### `fn rs_encode(data: Vec<Str>, m: Int) -> Vec<Str>`

k data shards -> m parity shards, each the same length. All shards must be equal length; a
caller that pads to a size class already has that.

### `fn rs_recover(shards: Vec<Str>, k: Int) -> Vec<Str>`

Rebuild the k data shards from any k surviving shards.

`shards` is the full n = k+m set in order (data first, then parity), with "" wherever a shard
is missing. Returns the k data shards, or an empty Vec when fewer than k survive; that is not
a failure to handle so much as the point at which the data is gone.


