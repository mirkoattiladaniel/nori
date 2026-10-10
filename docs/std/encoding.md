# std/encoding

```nori
import "std/encoding" as encoding
```

std/encoding: byte <-> text codecs: lowercase hex and standard Base64 (RFC 4648).
A "byte string" is an ordinary Nori `Str` (each `char_at` is one 0..255 byte). Encoders take bytes and
return ASCII text; decoders take that text and return the original bytes.
  import "std/encoding" as enc
  enc::hex_encode("Hi")     // "4869"
  enc::b64_encode("Hi")     // "SGk="
  enc::b64_decode("SGk=")   // "Hi"
(For a single integer in hex, std/fmt::to_hex already exists; this is for byte arrays + decoding.)
### `fn hex_encode(s: Str) -> Str`

encode bytes as lowercase hex ("Hi" -> "4869").

### `fn is_hex(s: Str) -> Bool`

is every non-whitespace character of `s` a hex digit, and is there an even number of them?

### `fn try_hex_decode(s: Str) -> Result<Str>`

decode a hex string back to bytes, or Err when it is not one.

Prefer this one. Hex is how keys, hashes and digests are written down, and `hex_decode` maps an
unrecognised character to the digit 0, so a mistyped key does not fail, it quietly becomes a
different key. Anything that decodes secret or identifying material wants the strict form.

### `fn hex_decode(s: Str) -> Str`

decode a hex string back to bytes, leniently: whitespace is ignored, an odd trailing digit is
dropped, and any character that is not a hex digit is read as 0.

That last one is silent and lossy: "zz" decodes to a NUL byte rather than failing, so invalid
input produces plausible bytes instead of an error. Use `try_hex_decode` unless you specifically
want the lenient behaviour.

### `fn b64_encode(s: Str) -> Str`

encode bytes as standard Base64 with `=` padding ("Hi" -> "SGk=").

### `fn b64url_encode(s: Str) -> Str`

decode standard Base64 back to bytes (padding/whitespace ignored).
encode bytes as base64url (RFC 4648 §5) with no padding.

The URL-safe alphabet swaps `+/` for `-_`, and padding is dropped because `=` has
to be escaped in a query string and carries no information (the length of the
input is recoverable without it). This is the encoding tokens, JWTs and webhook
signatures use; standard base64 in a URL breaks as soon as a token contains a `/`.

### `fn b64url_decode(s: Str) -> Str`

decode base64url back to bytes. Accepts input with or without `=` padding, and
accepts the standard alphabet too, since being strict about which one arrived buys
nothing when the mapping is unambiguous.

### `fn b64_decode(s: Str) -> Str`

decode standard base64. Characters outside the alphabet, including newlines and padding,
are skipped rather than rejected, so wrapped MIME input decodes as-is.


