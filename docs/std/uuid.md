# std/uuid

```nori
import "std/uuid" as uuid
```

std/uuid: RFC 4122 UUIDs, formatted canonically (8-4-4-4-12 lowercase hex). Two kinds:
  * v4: fully random (from a std/rand `Rng` you pass in).
  * v7: time-ordered: a 48-bit Unix-millisecond prefix (std/time) + random tail; sorts by creation time.
  import "std/uuid" as uuid
  import "std/rand" as rand
  var r = rand::new()
  uuid::v4(r)    // "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
### `fn v4(inout r: rand::Rng) -> Str`

a random (v4) UUID, drawn from `r`.

### `fn v7(inout r: rand::Rng) -> Str`

a time-ordered (v7) UUID: 48-bit Unix-ms prefix + random, drawn from `r`.


