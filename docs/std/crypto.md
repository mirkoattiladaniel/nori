# std/crypto

```nori
import "std/crypto" as crypto
```

### `fn aes256_gcm_seal(key: Str, nonce: Str, aad: Str, plain: Str) -> Str`

Seal `plain` under a 32-byte `key` and a 12-byte `nonce`, authenticating `aad` alongside it.
Returns ciphertext ‖ 16-byte tag, or "" if the key or nonce is the wrong length.

A nonce must never repeat under one key. Reusing one in GCM does not degrade the cipher, it
breaks it: two messages sealed under the same nonce XOR to their plaintexts, and the
authentication key becomes recoverable. Derive nonces, do not pick them.

### `fn aes256_gcm_open(key: Str, nonce: Str, aad: Str, sealed: Str) -> Str`

Open what `aes256_gcm_seal` produced. Returns the plaintext, or "" if the tag does not
verify, meaning the input was altered, truncated, sealed under a different key or nonce, or
carried different `aad`.

"" is ambiguous: an authentic empty plaintext opens to "" as well, so this cannot distinguish
"the sender sent nothing" from "someone forged this". For an authenticated cipher that
distinction is the entire point, so use `try_aes256_gcm_open`, which returns Err only for a real
authentication failure.

### `fn try_aes256_gcm_open(key: Str, nonce: Str, aad: Str, sealed: Str) -> Result<Str>`

Open what `aes256_gcm_seal` produced, reporting authentication failure as Err.

Prefer this over `aes256_gcm_open`. An AEAD's job is to answer "is this genuine?", and a plain
Str cannot carry that answer when the plaintext may legitimately be empty.

### `fn aes128_gcm_seal(key: Str, nonce: Str, aad: Str, plain: Str) -> Str`

AES-128-GCM seal: a 16-byte `key`, a 12-byte `nonce`, ciphertext ‖ 16-byte tag.

Here because TLS 1.3 makes `TLS_AES_128_GCM_SHA256` mandatory to implement, so a client that
offers only AES-256 will meet servers it cannot talk to. The nonce rule from `aes256_gcm_seal`
applies unchanged and matters more, not less: a 128-bit key does not make a repeated nonce
any less fatal.

### `fn aes128_gcm_open(key: Str, nonce: Str, aad: Str, sealed: Str) -> Str`

Open what `aes128_gcm_seal` produced, or "" if the tag does not verify.


### `struct B3`

an in-progress BLAKE3 hash. Create with `b3_new` / `b3_new_keyed`, feed with `update`, read
with `digest` / `hex` / `xof`.

`stack` is the CVs of completed left subtrees, flattened 8 words per entry. `chunks` is how
many chunks have been absorbed, and its popcount is how many entries the stack holds.
That identity is the whole merging rule: after finishing chunk number `t`, merge the top
two entries once for every trailing zero bit of `t`, which is the same as saying a subtree is
closed as soon as it is full. `buf` holds the bytes of the chunk in progress, so that
`update` can be called with any chunking the caller likes and produce the same digest.

### `fn b3_new() -> B3`

a hasher for the plain, unkeyed hash.

### `fn b3_new_keyed(key32: Str) -> B3`

a hasher keyed with exactly 32 raw bytes. A wrong-length key returns an unkeyed hasher rather
than silently truncating or zero-extending, which would make two different keys agree.

## B3

### `fn update(inout self, data: Str)`

feed more input. Any split of the same bytes across calls gives the same digest.

### `fn xof(self, len: Int) -> Str`

`len` bytes of the extendable output, as raw bytes. `digest()` is `xof(32)`.

Not `inout`: reading the output never disturbs the hasher, so `update` may continue after
an `xof` and further output stays consistent with the longer input.

### `fn digest(self) -> Str`

the 32 raw digest bytes.

### `fn hex(self) -> Str`

the 32-byte digest as 64 lowercase hex characters.

### `fn blake3_raw(data: Str) -> Str`

BLAKE3 of `data`: 32 raw digest bytes.

### `fn blake3(data: Str) -> Str`

BLAKE3 of `data` as 64 lowercase hex characters, the form `b3sum` prints and the form the
rest of std/crypto returns a digest in.

### `fn blake3_xof(data: Str, len: Int) -> Str`

`len` bytes of BLAKE3's extendable output over `data`, as raw bytes. `len` may exceed 32:
BLAKE3 is an XOF, and the first 32 bytes of any longer output are `blake3_raw`.

### `fn blake3_keyed_raw(key32: Str, data: Str) -> Str`

keyed BLAKE3 (a MAC) under a 32-byte `key`: 32 raw bytes. A key of any other length is
rejected with "" rather than being stretched into a different key.

### `fn blake3_keyed(key32: Str, data: Str) -> Str`

keyed BLAKE3 as lowercase hex, or "" if the key is not 32 bytes.

### `fn blake3_derive_key(context: Str, key_material: Str, len: Int) -> Str`

derive `len` bytes of key material from `key_material` under the application `context`.

`context` must be a hardcoded, globally unique string; the spec is emphatic that it is not a
runtime value, not a salt and not a secret. Its whole job is to make two applications' keys
unrelated even from the same input material.

### `fn blake3_file(path: Str) -> Str`

BLAKE3 of the file at `path` as lowercase hex, or "" if it cannot be read.

The compressor is fed in 64 KiB pieces, but `read_file` loads the file whole first, so this
is not a streaming hash and memory use grows with the file size.


std/crypto: hashes, MAC, authenticated encryption, password hashing and secure randomness:
SHA-256, SHA-384, SHA-512, SHA-1, BLAKE2b, BLAKE3, HMAC over any of the SHA-2 hashes, CRC-32,
AES-128/256-GCM, Argon2id and PBKDF2 password hashes, the kernel CSPRNG, and signed tokens.
  import "std/crypto" as crypto
  crypto::sha256("abc")            // -> "ba7816bf...20015ad"  (lowercase hex)
  crypto::sha256_raw(data)         // 32 raw bytes (for HMAC / binary protocols)
  crypto::hmac_sha256(key, msg)    // RFC 2104, lowercase hex
  crypto::sha512(data) / crypto::sha384(data)      // and _raw for the bytes
  crypto::hmac_sha512(key, msg) / crypto::hmac_sha384(key, msg)
  crypto::sha1("abc") / crypto::crc32_hex(data)
  crypto::md5(data)                // RFC 1321, hex: only where a format names it (thumbnail paths)
  crypto::rand_bytes(32) / crypto::rand_hex(32)   // the kernel CSPRNG, never std/rand
  crypto::ct_eq(given, expected)                  // compare secrets without a timing leak
  crypto::password_hash(pw) / password_verify(h, pw)   // Argon2id, PHC format
  crypto::argon2id_raw(...) / crypto::blake2b(data, 32)
  crypto::blake3(data)                            // BLAKE3-256, lowercase hex
  crypto::blake3_raw(data) / crypto::blake3_file(path)
  crypto::blake3_keyed(key32, data)               // BLAKE3 as a MAC
  crypto::blake3_xof(data, n) / crypto::blake3_derive_key(context, material, n)
  var h = crypto::b3_new()  h.update(part)  h.hex()   // incremental, for large inputs
  crypto::aes256_gcm_seal(key32, nonce12, aad, plain)   // -> ciphertext ‖ 16-byte tag
  crypto::aes256_gcm_open(key32, nonce12, aad, sealed)  // -> plaintext, or "" if it does not verify
  crypto::aes128_gcm_seal(key16, nonce12, aad, plain) / aes128_gcm_open(...)
  crypto::token_new(key, payload, 3600)           // a signed, expiring token
  crypto::token_read(key, token)                  // -> ok / bad_signature / expired
32-bit words are held in i64 and masked to 32 bits after each op (FIPS 180-4); the SHA-512
family uses the full 64 bits, where no masking is needed. SHA-1 is for git-style/legacy
hashing, not new security uses.

Anything comparing a secret must use `ct_eq` and not `==`.
### `fn sha256(data: Str) -> Str`

SHA-256 of an arbitrary byte string, as lowercase hex (64 chars).

### `fn sha256_raw(data: Str) -> Str`

SHA-256 as 32 raw bytes (a byte Str), the form HMAC and binary protocols need.

### `fn hmac_sha256(key: Str, msg: Str) -> Str`

HMAC-SHA256(key, msg) -> lowercase hex (RFC 2104). Keys longer than the 64-byte block are hashed first.

### `fn hmac_sha256_raw(key: Str, msg: Str) -> Str`

HMAC-SHA256(key, msg) -> the 32 raw bytes. What PBKDF2 and any binary protocol
want; `hmac_sha256` is this, hex-encoded.

### `fn crc32(data: Str) -> Int`

CRC-32 (IEEE 802.3) of a byte string, as a 32-bit integer.

### `fn crc32_hex(data: Str) -> Str`

CRC-32 as 8 lowercase hex digits.

### `fn sha1(data: Str) -> Str`

SHA-1 of a byte string, as lowercase hex (40 chars).

### `fn rand_bytes(n: Int) -> Str`

`n` bytes from the kernel's CSPRNG, or "" if they could not be read.

"" is a hard failure and must be treated as one: a caller that shrugs and carries
on with a shorter or empty token has produced something guessable. There is no
fallback to std/rand here: a weak token that looks like a strong one is
worse than no token.

Reads /dev/urandom rather than calling getrandom(2), which keeps std/crypto free of
FFI and portable to any POSIX system. getrandom would avoid needing a file
descriptor (and works in a chroot with no /dev); worth adding behind `cfg linux` if
that ever matters.

Windows has no /dev/urandom; there the bytes come from advapi32's system CSPRNG instead.
A caller that treats an empty answer as bytes would be building a key out of nothing.

### `fn rand_hex(n: Int) -> Str`

`n` random bytes, hex-encoded (so the result is 2n characters).

The unit is bytes, not characters, because that is the unit security is measured
in: `rand_hex(32)` is a 256-bit token. "" on failure, as `rand_bytes`.

### `fn ct_eq(a: Str, b: Str) -> Bool`

Compare two strings in time that does not depend on where they differ.

`a == b` stops at the first differing byte, so how long it takes reveals how much
of a secret a guess got right, enough to recover a token or a MAC one byte at a
time over enough attempts. Use this for anything an attacker supplies and the
server compares against a secret: session ids, password-reset tokens, HMACs,
webhook signatures.

The length difference is folded into the result rather than short-circuiting, and
the loop always runs the length of `a`.

### `struct TokenCheck`

what a token turned out to be. `why` is "" when ok, else "bad_format",
"bad_signature" or "expired".

### `fn token_new(key: Str, payload: Str, ttl: Int) -> Str`

Mint a token carrying `payload`, valid for `ttl` seconds.

`ttl <= 0` means it never expires, which is occasionally what you want (a signed cookie
whose lifetime the cookie itself controls), and a liability otherwise: a token
that leaks is then valid forever, and the only way to revoke it is to change the
key, which invalidates every other token too.

`key` is the one secret this rests on. Generate it with `rand_bytes(32)`, keep it
out of the repository, and give each purpose its own: a key shared between the
reset-password tokens and the newsletter unsubscribe tokens lets one be used as
the other.

### `fn token_read(key: Str, token: Str) -> TokenCheck`

Verify and open a token.

The signature is checked before the expiry, and with `ct_eq`, so neither the
answer nor the time taken tells an attacker how close a forgery came.

### `fn pbkdf2_sha256(password: Str, salt: Str, iterations: Int, dklen: Int) -> Str`

PBKDF2-HMAC-SHA256: derive `dklen` raw bytes from `password` and `salt`.

`iterations` is the cost, and it is the whole point: see `password_iterations`
for what to pass. "" if any argument is nonsense, which callers must treat as a
failure rather than deriving from nothing.

### `fn password_iterations() -> Int`

Iterations for a new password hash, and the number you should think hardest about.

A login pays the cost once, and registration once more. OWASP recommends 600,000 for
PBKDF2-HMAC-SHA256; this default is lower, chosen to fit a quarter-second login budget. If
you need better, the answer is not a bigger number but a memory-hard KDF.
Argon2id blunts GPUs in a way no iteration count can.

Raise it with `password_hash_with`; `password_needs_rehash` will then upgrade
existing hashes on their next successful login.

### `fn pbkdf2_hash(password: Str) -> Str`

Hash a password for storage. Returns a self-describing string:

    $pbkdf2-sha256$i=210000$<b64url salt>$<b64url dk>

The salt is 16 random bytes, generated here: the same password hashed twice gives
different strings, so two users sharing one are not visibly alike and precomputed
tables are useless. The parameters travel with the hash, so raising the cost later
leaves every existing hash verifiable. "" if the CSPRNG failed, which must be
treated as a failure: a hash with a predictable salt is a weaker thing wearing the
same shape.

### `fn pbkdf2_hash_with(password: Str, salt: Str, iterations: Int) -> Str`

`pbkdf2_hash` with an explicit salt and cost, for test vectors, and for
re-hashing at a chosen cost.

### `fn pbkdf2_verify(stored: Str, password: Str) -> Bool`

Check a password against a stored hash. Constant-time in the comparison, and false
for anything it cannot parse; a malformed record must never verify.

### `fn blake2b(data: Str, outlen: Int) -> Str`

BLAKE2b of `data` with an `outlen`-byte digest (1..64), returned as raw bytes.

### `fn blake2b_hex(data: Str, outlen: Int) -> Str`

BLAKE2b as lowercase hex

### `fn argon2id_raw(password: Str, salt: Str, secret: Str, ad: Str,`

Argon2id, returning `taglen` raw bytes. `m_kib` is memory in KiB, `t` passes,
`p` lanes. `secret` and `ad` may be "".

### `fn argon2id_memory() -> Int`

Default cost for a new password hash: OWASP's Argon2id recommendation.

### `fn argon2id_time() -> Int`

Default pass count for a new password hash (OWASP's Argon2id recommendation).

### `fn argon2id_lanes() -> Int`

Default lane (parallelism) count for a new password hash.

### `fn argon2id_hash(password: Str) -> Str`

Hash a password with Argon2id, in the PHC string format everyone else uses:

    $argon2id$v=19$m=19456,t=2,p=1$<b64 salt>$<b64 tag>

Which means these verify under libsodium, and libsodium's verify here. 16 random
bytes of salt, generated fresh, "" if the CSPRNG failed, and that must be treated
as a failure rather than stored.

### `fn argon2id_hash_at(password: Str, m: Int, t: Int, p: Int) -> Str`

`argon2id_hash` at a cost you choose, with a fresh random salt.

The default is OWASP's recommendation and suits most sites. Raise `m` if your
login budget allows it: memory is the axis an attacker cannot buy their way out
of, so it is worth more than extra passes. Remember that every concurrent login
holds `m` KiB for the duration, which is a denial-of-service lever as well as a
security control.

### `fn argon2id_hash_with(password: Str, salt: Str, m: Int, t: Int, p: Int) -> Str`

`argon2id_hash` with the salt supplied rather than generated, for reproducing a known
vector or re-hashing under a recorded salt. Everything else (the PHC framing, the 32-byte
tag) matches `argon2id_hash`.

Passing a salt is how a hash stops being unique per password, so use `argon2id_hash` or
`argon2id_hash_at` for anything you store. An empty password returns "".

### `fn argon2id_verify(stored: Str, password: Str) -> Bool`

Verify a password against a PHC-format Argon2id hash. Constant-time, and false for
anything it cannot parse; a record it does not understand must never verify.

### `fn argon2id_needs_rehash(stored: Str) -> Bool`

Was this hash written with something weaker than we use now (a different
algorithm, or lower parameters)? If so, re-hash on the next successful login.

### `fn password_hash(password: Str) -> Str`

Hash a password for storage: Argon2id, in PHC format. "" on failure, which must
be treated as a failure rather than stored.

### `fn password_verify(stored: Str, password: Str) -> Bool`

Verify against a stored hash of any scheme this library writes: Argon2id today,
PBKDF2-HMAC-SHA256 from before it existed. False for anything unrecognised.

### `fn password_needs_rehash(stored: Str) -> Bool`

Should this hash be rewritten? True for a weaker algorithm or lower parameters,
so do it on the next successful login, the one moment the password is in hand.

### `fn sha256_file(path: Str) -> Str`

SHA-256 of file `path`, lowercase hex (64 chars), or "" if the file can't be read.


### `fn x25519(scalar: Str, upoint: Str) -> Str`

X25519: multiply the u-coordinate `upoint` by `scalar`. Both are 32 bytes; the answer is 32.
Returns "" on a wrong length.

An all-zero result means the input point had small order and the shared secret is worthless.
RFC 7748 leaves rejecting that to the caller, so `x25519_shared` below does it and this does
not; the raw primitive stays raw.

### `fn x25519_base(scalar: Str) -> Str`

the public key for a private scalar: the scalar times the base point, u = 9.

### `fn x25519_shared(mine: Str, theirs: Str) -> Str`

A shared secret, with the check RFC 7748 says the caller owes: an all-zero output means the
peer sent a small-order point and every party would agree the same worthless value. Returning
"" makes that a refusal rather than a secret nobody had to guess.


### `struct EcCurve`

A curve, with everything the Montgomery arithmetic needs precomputed.

### `fn ec_p256() -> EcCurve`

secp256r1 / prime256v1.

### `fn ec_p384() -> EcCurve`

secp384r1.

### `fn ecdsa_verify(c: EcCurve, pubkey: Str, digest: Str, r: Str, s: Str) -> Bool`

Verify an ECDSA signature.

`pubkey` is an uncompressed point (0x04 ‖ X ‖ Y), `digest` the message hash, and `r` and `s` the
signature halves, each as many bytes as the curve is wide.

Every precondition below is a rejection that matters. A public key off the curve is not a key.
An r or s of zero, or one at or above the group order, is a signature no signer produces and
several verifiers have accepted, which is what a good share of the Wycheproof vectors test.

### `fn ecdsa_public(c: EcCurve, priv: Str) -> Str`

The public key for a private scalar, as the uncompressed 04‖X‖Y a verifier expects.

### `fn ecdsa_valid_private(c: EcCurve, priv: Str) -> Bool`

A private scalar from raw bytes; rejected unless it lands in [1, n-1], which is what makes a
key generated from any random source usable without the caller having to know the order.

### `fn ecdsa_sign(c: EcCurve, priv: Str, digest: Str) -> Str`

Sign `digest` (already hashed) with `priv`. Returns r‖s, each field-width, or "" if the private
key is not a scalar on this curve.

### `fn ec_field_mul(c: EcCurve, a: Str, b: Str) -> Str`

a * b mod p, operands and result as field-width big-endian bytes.

### `fn ec_field_add(c: EcCurve, a: Str, b: Str) -> Str`

a + b mod p

### `fn ec_field_sub(c: EcCurve, a: Str, b: Str) -> Str`

a - b mod p

### `fn ec_field_inv(c: EcCurve, a: Str) -> Str`

a^-1 mod p


### `fn hkdf_extract_h(hlen: Int, salt: Str, ikm: Str) -> Str`

HKDF-Extract over the hash named by `hlen` (32 = SHA-256, 48 = SHA-384). An empty salt means a
block of `hlen` zero bytes, which is not the same thing as an empty HMAC key.

### `fn hkdf_expand_h(hlen: Int, prk: Str, info: Str, n: Int) -> Str`

HKDF-Expand over the hash named by `hlen`. "" if `n` is negative or exceeds 255 blocks, the
limit the single-byte counter imposes.

### `fn hkdf_extract(salt: Str, ikm: Str) -> Str`

HKDF-Extract (RFC 5869): concentrate the entropy of `ikm` into a uniform 32-byte pseudorandom
key. An empty salt is legal and means a block of zeros, which is not the same thing as an
empty HMAC key.

This step is not optional. A raw Diffie-Hellman output is not uniformly distributed
and must not be used as a key directly; extract is what makes it one.

### `fn hkdf_expand(prk: Str, info: Str, n: Int) -> Str`

`n` bytes derived from a pseudorandom key, bound to the label `info`. Two outputs taken under
different labels are independent, so one handshake secret can key a send direction, a receive
direction and a rekey chain without any of them revealing the others.

Returns "" if `n` is negative or exceeds 255 hash blocks (8160 bytes), which is the limit the
construction allows. A shorter request is a prefix of a longer one; this is a stream, not a
per-length function.

### `fn hkdf(salt: Str, ikm: Str, info: Str, n: Int) -> Str`

extract then expand, which is how HKDF is almost always used.


### `fn md5(data: Str) -> Str`

MD5 of `data` as 32 lowercase hex digits (RFC 1321's digest bytes, in order). Not for security.

### `fn md5_raw(data: Str) -> Str`

MD5 of `data` as its 16 raw digest bytes.


### `fn mont_limbs_of(be: Str) -> Vec<Int>`

A big-endian byte string as little-endian 32-bit limbs.

### `fn mont_bytes_of(v: Vec<Int>, n: Int) -> Str`

Limbs back to exactly `n` big-endian bytes.

### `fn mont_zeros(n: Int) -> Vec<Int>`

`n` zero limbs.

### `fn mont_ge(a: Vec<Int>, b: Vec<Int>) -> Bool`

a >= b, as unsigned little-endian limb vectors of equal length.

### `fn mont_sub_in_place(inout a: Vec<Int>, b: Vec<Int>)`

a -= b, wrapping.

### `fn mont_n0inv(n0: Int) -> Int`

-n^-1 mod 2^32, for an odd n.

### `fn mont_mul(a: Vec<Int>, b: Vec<Int>, n: Vec<Int>, n0inv: Int) -> Vec<Int>`

(a * b * R^-1) mod n: a Montgomery product, CIOS form.

### `fn mont_r_squared(n: Vec<Int>) -> Vec<Int>`

R^2 mod n, so a value can be scaled into Montgomery form.

### `fn mont_modexp(base: Vec<Int>, exp: Vec<Int>, n: Vec<Int>) -> Vec<Int>`

base^exp mod n, for an odd n. Inputs and output are ordinary (not Montgomery) values.

### `fn mont_one(len: Int) -> Vec<Int>`

the value 1 in `len` limbs.


### `struct RsaPub`

An RSA public key: modulus and exponent, big-endian, as they came out of a certificate.

### `fn rsa_bits(k: RsaPub) -> Int`

The modulus size in bits, counted from the top set bit, which is what "a 2048-bit key" means,
and not the byte length times eight.

### `fn rsa_pkcs1_verify(k: RsaPub, digest: Str, hash_len: Int, sig: Str) -> Bool`

Verify an RSASSA-PKCS1-v1_5 signature over `digest`, which must be the hash of the message and
`hash_len` bytes long (32, 48 or 64; SHA-1 is not offered, as a matter of policy).

The whole expected block is constructed and compared. That is the difference between this and
the implementations that have been broken: there is no scan for a 0x00 separator, no "skip the
padding and look at what follows", and so nothing after the digest can be smuggled anywhere.

### `fn rsa_pss_verify(k: RsaPub, digest: Str, hash_len: Int, salt_len: Int, sig: Str) -> Bool`

Verify an RSASSA-PSS signature over `digest`. `salt_len` is the expected salt length; TLS 1.3
requires it to equal the hash length, and a verifier that accepts any length accepts forgeries
that a fixed-length one would not.

PSS is what TLS 1.3 mandates for CertificateVerify. The steps below are RFC 8017 §9.1.2 in
order, and every one of them is a rejection point: an implementation that skips the leftmost
bit check, or the zero-padding check, still verifies genuine signatures perfectly.


### `fn sha512_raw(data: Str) -> Str`

SHA-512 as 64 raw bytes.

### `fn sha512(data: Str) -> Str`

SHA-512 of an arbitrary byte string, as lowercase hex (128 chars).

### `fn sha384_raw(data: Str) -> Str`

SHA-384 as 48 raw bytes.

### `fn sha384(data: Str) -> Str`

SHA-384 of an arbitrary byte string, as lowercase hex (96 chars).

### `fn hmac_sha512_raw(key: Str, msg: Str) -> Str`

HMAC-SHA512(key, msg) -> the 64 raw bytes.

### `fn hmac_sha512(key: Str, msg: Str) -> Str`

HMAC-SHA512(key, msg) -> lowercase hex.

### `fn hmac_sha384_raw(key: Str, msg: Str) -> Str`

HMAC-SHA384(key, msg) -> the 48 raw bytes. TLS 1.3's SHA-384 cipher suites key their whole
schedule with this, so it is a protocol primitive and not only a convenience.

### `fn hmac_sha384(key: Str, msg: Str) -> Str`

HMAC-SHA384(key, msg) -> lowercase hex.


