# std/tls

```nori
import "std/tls" as tls
```

### `struct Asn1`

A cursor over a DER buffer, bounded to `[pos, end)` and carrying its nesting depth.

### `struct Tlv`

One tag-length-value header, with the content it introduces located but not read.

### `fn asn1_max_depth() -> Int`

how deep a certificate may nest before this refuses to follow. Real ones use about six levels;
the limit is far above that and far below anything that could exhaust the stack.

### `fn tag_boolean() -> Int`

universal tag 1: DER admits two encodings, 0x00 and 0xFF.

### `fn tag_integer() -> Int`

universal tag 2

### `fn tag_bit_string() -> Int`

universal tag 3: its first content byte counts unused trailing bits.

### `fn tag_octet_string() -> Int`

universal tag 4

### `fn tag_null() -> Int`

universal tag 5

### `fn tag_oid() -> Int`

universal tag 6

### `fn tag_utf8string() -> Int`

universal tag 12

### `fn tag_sequence() -> Int`

universal tag 16, always constructed.

### `fn tag_set() -> Int`

universal tag 17

### `fn tag_printablestring() -> Int`

universal tag 19

### `fn tag_ia5string() -> Int`

universal tag 22, what a dNSName in a SAN is.

### `fn tag_utctime() -> Int`

universal tag 23: a two-digit year, windowed by RFC 5280.

### `fn tag_generalizedtime() -> Int`

universal tag 24: a four-digit year.

### `fn asn1_new(data: Str) -> Asn1`

A cursor over a whole buffer.

### `fn asn1_inner(a: Asn1, t: Tlv) -> Asn1`

A cursor over the content of `t`, one level deeper. Refuses past `asn1_max_depth`.

### `fn asn1_left(a: Asn1) -> Int`

how many bytes remain in this cursor's window

### `fn asn1_done(a: Asn1) -> Bool`

is this cursor exhausted, with nothing unread and nothing wrong?

### `fn asn1_next(inout a: Asn1) -> Tlv`

Read the next tag-length header, leaving the cursor after the value it introduces.

The length rules are where this is strict. An indefinite length (0x80) is BER and not DER, and
carries no end: a parser that accepts one is reading until it finds something that looks like
a terminator. A long form that could have been short, or one with leading zero bytes, is the
same number written two ways. Both are refused.

### `fn asn1_expect(inout a: Asn1, tag: Int) -> Tlv`

The next header, required to be a universal tag of a particular number. This is the form nearly
every caller wants: reading a SEQUENCE where a SEQUENCE must be is a check, and reading
"whatever is next" is not.

### `fn asn1_content(a: Asn1, t: Tlv) -> Str`

The raw content bytes a header introduced.

### `fn asn1_raw(a: Asn1, t: Tlv, hdr_start: Int) -> Str`

The header AND its content: the bytes as they appeared, which is what a signature is
computed over and so must never be reconstructed.

### `fn asn1_mark(a: Asn1) -> Int`

Where the header of the next value begins; capture it before `asn1_next` to recover the exact
encoded bytes afterwards.

### `fn asn1_uint(a: Asn1, t: Tlv) -> Str`

A DER INTEGER as an unsigned big-endian magnitude, with the sign byte removed.

"" for a negative integer rather than a magnitude: everything a certificate holds as an INTEGER
and this code cares about (a modulus, an exponent, an ECDSA r and s) is positive, and
silently taking the magnitude of a negative one would turn a malformed certificate into a
plausible key.

### `fn asn1_bits(a: Asn1, t: Tlv) -> Str`

The bytes of a BIT STRING, requiring it to be a whole number of octets.

The first content byte counts unused trailing bits. Every bit string this code reads (a public
key, a signature) is octet-aligned, so anything other than zero unused bits is malformed here
rather than something to shift.

### `fn asn1_oid_is(a: Asn1, t: Tlv, want: Str) -> Bool`

Compare an OID's content bytes against a dotted-decimal spelling encoded the same way. OIDs are
compared as encoded bytes rather than decoded numbers, because decoding is where a parser meets
arbitrary-length integers it does not need.


### `struct Conn`

A TLS connection. `err` is "" while it is healthy; once set, the connection is finished and
every further operation is a no-op.

### `fn handshake(fd: Int, host: Str) -> Conn`

Run a TLS 1.3 handshake over `fd`, presenting `host` as the server name.

Returns a Conn whose `err` says what went wrong; there is no success that leaves `err` set and
no failure that leaves it empty. Without `--cfg tls_insecure_test` this always fails, at the
certificate.

### `fn conn_send(inout c: Conn, data: Str) -> Int`

Send application data. Returns the number of bytes accepted, or 0 if the connection is done.
Data longer than one record's worth is split across records.

### `fn conn_recv(inout c: Conn, max: Int) -> Str`

Receive up to `max` bytes of application data. "" means the peer closed cleanly or the
connection has failed: `err` and `closed` say which.

Post-handshake messages arriving here are handled rather than delivered: a NewSessionTicket is
dropped (this client does not resume), and a KeyUpdate is answered by rekeying the read
direction, because ignoring one means every later record fails to decrypt.

### `fn conn_close(inout c: Conn)`

Close the connection, sending close_notify first so the peer can tell a clean end from a
truncated one. That distinction is the whole point of the alert: without it a caller cannot
know whether it received the whole response or the connection was cut.


### `struct Reader`

A bounds-checked cursor over a byte string. `err` latches: the first failure wins, and every
read after it is a no-op, so a caller can parse a whole structure and test once.

### `fn reader_new(data: Str) -> Reader`

A cursor over `data`, positioned at the start and with no error yet.

### `fn r_left(r: Reader) -> Int`

how many bytes remain unread

### `fn r_u8(inout r: Reader) -> Int`

One byte. Zero once the reader has failed, which is why `err` and not the value is what a caller tests.

### `fn r_u16(inout r: Reader) -> Int`

A 16-bit big-endian integer: the width of nearly every length in TLS.

### `fn r_u24(inout r: Reader) -> Int`

A 24-bit big-endian integer: a handshake message length, and a certificate list length.

### `fn r_bytes(inout r: Reader, n: Int) -> Str`

`n` bytes. Fails rather than truncating if fewer remain, and refuses a negative `n`.

### `fn r_vec8(inout r: Reader) -> Str`

a block introduced by an 8- or 16-bit length, which is how TLS spells nearly every list

### `fn r_vec16(inout r: Reader) -> Str`

A block introduced by a 16-bit length.

### `fn vec8(b: Str) -> Str`

`b` behind an 8-bit length

### `fn vec16(b: Str) -> Str`

`b` behind a 16-bit length

### `fn handshake_msg(htype: Int, body: Str) -> Str`

One handshake message: a one-byte type, a 24-bit length, and the body. Named `_msg` because
`handshake` is the client's entry point, and the two would otherwise collide: every root file
of a directory module shares one scope.

### `fn hs_client_hello() -> Int`

handshake type 1

### `fn hs_server_hello() -> Int`

handshake type 2, also a HelloRetryRequest, which reuses this type.

### `fn hs_new_session_ticket() -> Int`

handshake type 4, which arrives after the handshake, under application keys.

### `fn hs_encrypted_extensions() -> Int`

handshake type 8: the first message of the server's encrypted flight.

### `fn hs_certificate() -> Int`

handshake type 11

### `fn hs_certificate_request() -> Int`

handshake type 13, a request for a client certificate, which this client does not have.

### `fn hs_certificate_verify() -> Int`

handshake type 15

### `fn hs_finished() -> Int`

handshake type 20

### `fn hs_key_update() -> Int`

handshake type 24: post-handshake, and ignoring one makes every later record fail.

### `fn ext_server_name() -> Int`

extension 0 (RFC 6066): the host being asked for. Never an IP address.

### `fn ext_supported_groups() -> Int`

extension 10: the key-exchange groups offered.

### `fn ext_signature_algorithms() -> Int`

extension 13: what the client is willing to verify. Anything absent is refused by construction.

### `fn ext_supported_versions() -> Int`

extension 43: where TLS 1.3 is actually negotiated, `legacy_version` having always said 1.2.

### `fn ext_key_share() -> Int`

extension 51: the offered public key; in a HelloRetryRequest, a bare group with no key.

### `fn group_x25519() -> Int`

the only key-exchange group this client offers

### `fn client_hello(host: Str, random: Str, session_id: Str, pubkey: Str) -> Str`

Build a ClientHello for `host`, offering `pubkey` as an x25519 key share.

`random` must be 32 bytes from a cryptographic source and `session_id` 32 bytes; the session id
is not used by TLS 1.3 at all, but echoing a non-empty one is what makes the exchange look like
a resumed TLS 1.2 session to middleboxes that would otherwise interfere.

"" if an argument is the wrong length, never a partly-formed hello.

### `struct HsMsg`

One handshake message lifted out of a buffer. `total` is how many bytes it occupied, so a
caller can advance; 0 means the buffer does not yet hold a whole message.

### `fn parse_handshake(buf: Str) -> HsMsg`

The first handshake message in `buf`, if it is complete.

Handshake messages do not line up with records: a server routinely packs several into one
record and may split one across two, so this works on a reassembled stream and reports how
much it used rather than assuming a record boundary means anything.

### `struct ServerHello`

What a ServerHello said. `err` is "" only if every field parsed; on failure nothing else is
meaningful.

### `fn parse_server_hello(body: Str) -> ServerHello`

Parse a ServerHello body (the message without its four-byte handshake header).

The negotiated version comes from the supported_versions extension and never from
`legacy_version`, which always says TLS 1.2. A server that omits the extension is not speaking
TLS 1.3, and is refused here rather than downgraded.

### `struct Alert`

An alert, as the peer sent it.

### `fn parse_alert(body: Str) -> Alert`

Read a two-byte alert body. A short one is reported as an internal_error(80) rather than
guessed at.

### `fn alert_is_close_notify(a: Alert) -> Bool`

Is this the peer closing cleanly? close_notify(0) at warning level, and nothing else.

### `fn alert_name(desc: Int) -> Str`

A readable name for an alert, for error messages. The number is what matters; the name is so a
failure says "handshake_failure" rather than "alert 40".

### `fn alert_fatal(desc: Int) -> Str`

The alert record body for a fatal alert of the given description.

### `fn alert_close_notify() -> Str`

close_notify, at warning level, the only clean way to end a connection.

### `fn parse_certificate(body: Str) -> Vec<Str>`

The certificates in a Certificate message, leaf first, each still DER-encoded.

The per-certificate extensions are parsed and discarded: they must be well-formed, but nothing
in TLS 1.3 that this client cares about is carried there. Certificates themselves are not
examined here: that is chain validation's job (see verify.nori), and doing any of it in this
file would spread the one part of a TLS client that must be airtight across two.

### `struct CertVerify`

The algorithm and signature from a CertificateVerify body.

### `fn parse_certificate_verify(body: Str) -> CertVerify`

The signature algorithm and signature from a CertificateVerify body. Reading them is not checking them.

### `fn certificate_verify_content(thash: Str, is_server: Bool) -> Str`

The bytes a CertificateVerify signature is computed over (RFC 8446 §4.4.3).

The 64 spaces and the context string are not padding. They exist so that a signature made by a
TLS server can never be mistaken for one made in another context, and so that a client
signature and a server signature over the same transcript differ.


### `fn oid_rsa_encryption() -> Str`

1.2.840.113549.1.1.1

### `fn oid_rsassa_pss() -> Str`

1.2.840.113549.1.1.10

### `fn oid_sha256_with_rsa() -> Str`

1.2.840.113549.1.1.11

### `fn oid_sha384_with_rsa() -> Str`

1.2.840.113549.1.1.12

### `fn oid_sha512_with_rsa() -> Str`

1.2.840.113549.1.1.13

### `fn oid_sha1_with_rsa() -> Str`

1.2.840.113549.1.1.5

### `fn oid_md5_with_rsa() -> Str`

1.2.840.113549.1.1.4

### `fn oid_ec_public_key() -> Str`

1.2.840.10045.2.1

### `fn oid_prime256v1() -> Str`

1.2.840.10045.3.1.7

### `fn oid_secp384r1() -> Str`

1.3.132.0.34

### `fn oid_ecdsa_with_sha256() -> Str`

1.2.840.10045.4.3.2

### `fn oid_ecdsa_with_sha384() -> Str`

1.2.840.10045.4.3.3

### `fn oid_ecdsa_with_sha1() -> Str`

1.2.840.10045.4.1

### `fn oid_ed25519() -> Str`

1.3.101.112

### `fn oid_sha256() -> Str`

2.16.840.1.101.3.4.2.1

### `fn oid_sha384() -> Str`

2.16.840.1.101.3.4.2.2

### `fn oid_sha512() -> Str`

2.16.840.1.101.3.4.2.3

### `fn oid_mgf1() -> Str`

1.2.840.113549.1.1.8

### `fn oid_ext_basic_constraints() -> Str`

2.5.29.19

### `fn oid_ext_key_usage_bits() -> Str`

2.5.29.15

### `fn oid_ext_subject_alt_name() -> Str`

2.5.29.17

### `fn oid_ext_extended_key_usage() -> Str`

2.5.29.37

### `fn oid_ext_authority_key_id() -> Str`

2.5.29.35

### `fn oid_ext_subject_key_id() -> Str`

2.5.29.14

### `fn oid_ext_certificate_policies() -> Str`

2.5.29.32

### `fn oid_ext_crl_distribution() -> Str`

2.5.29.31

### `fn oid_ext_authority_info_access() -> Str`

1.3.6.1.5.5.7.1.1

### `fn oid_ext_sct() -> Str`

1.3.6.1.4.1.11129.2.4.2

### `fn oid_ext_name_constraints() -> Str`

2.5.29.30

### `fn oid_eku_server_auth() -> Str`

1.3.6.1.5.5.7.3.1

### `fn oid_eku_client_auth() -> Str`

1.3.6.1.5.5.7.3.2

### `fn oid_eku_any() -> Str`

2.5.29.37.0

### `fn oid_at_common_name() -> Str`

2.5.4.3


### `struct Keys`

One direction's record-protection state: the key, the IV, and the count of records sent or
received under them. A rekey replaces the whole thing.

### `struct Record`

A record after decryption: its true content type, its contents, and why it could not be read.
`err` is "" on success, and on failure `body` is empty; there is no partial result.

### `fn ct_change_cipher_spec() -> Int`

content type 20: meaningless in TLS 1.3, sent only so middleboxes see a familiar shape.

### `fn ct_alert() -> Int`

content type 21

### `fn ct_handshake() -> Int`

content type 22

### `fn ct_application_data() -> Int`

content type 23, which every protected record claims on the outside, whatever it carries.

### `fn max_plaintext() -> Int`

The largest plaintext a record may carry, 2^14 (RFC 8446 §5.1).

### `fn max_ciphertext() -> Int`

The largest protected fragment a record may carry: the plaintext limit, plus one byte of
content type, plus up to 255 of padding, plus the 16-byte tag (RFC 8446 §5.2).

This bound is the reason a hostile server cannot make a client allocate without limit. The
length field is 16 bits, so a peer can always ask for 64KiB; refusing at this cap is what keeps
the answer bounded, and it must be checked before anything is read, not after.

### `fn suite_aes128_gcm_sha256() -> Int`

The cipher suites this client offers. Both are AEAD_AES_GCM; they differ in key length and in
the hash the whole key schedule runs on.

### `fn suite_aes256_gcm_sha384() -> Int`

TLS_AES_256_GCM_SHA384 (0x1302): a 32-byte key, and a key schedule over SHA-384.

### `fn suite_key_len(suite: Int) -> Int`

The AEAD key length a suite uses, or 0 if the suite is not one we speak.

### `fn suite_hash_len(suite: Int) -> Int`

The hash length a suite's key schedule runs on, or 0 if the suite is not one we speak.

### `fn keys_new(sink key: Str, sink iv: Str) -> Keys`

Record-protection state for one direction, with the sequence number at zero.

### `fn version_tls12() -> Int`

The legacy record version every record after the first must carry: 0x0303, "TLS 1.2".

### `fn version_tls10() -> Int`

0x0301, "TLS 1.0", permitted on the initial ClientHello only (RFC 8446 §5.1), because some
middleboxes reject a first record that claims anything newer.

### `fn record_plain(ctype: Int, version: Int, body: Str) -> Str`

An unprotected record, as the ClientHello and the compatibility ChangeCipherSpec are sent.
"" if the fragment is over the 2^14 limit.

`version` is a parameter and not a constant because the initial ClientHello is allowed to
differ, and it is the one record where interoperability actually depends on the choice.

### `fn record_seal(inout k: Keys, ctype: Int, body: Str) -> Str`

Seal one record and advance the sequence number. "" if the body is too long or the keys are
not a length we can use.

The outer header always says `application_data` and version 0x0303 whatever is inside: a
protected record does not advertise what it carries, and a middlebox watching the outside sees
the same thing for a Finished as for a byte of application data.

### `fn record_len(buf: Str) -> Int`

How many bytes the record starting at the front of `buf` occupies, or 0 if `buf` does not yet
hold a complete one, or -1 if its declared length is over the limit.

Separated from reading so that a caller can decide it has enough bytes without having to buffer
whatever length a peer claims. A peer that announces 64KiB is refused here, before the read.

### `fn record_open(inout k: Keys, wire: Str) -> Record`

Open one complete protected record and advance the sequence number.

Every failure is the same failure. A record that will not authenticate is indistinguishable
from one that was altered, reordered, replayed or sent under another key, and reporting which
is which would hand an attacker the oracle the AEAD exists to deny, so the reasons
below describe the framing, which is public, and never the outcome of the tag check.


std/tls: the TLS 1.3 key schedule (RFC 8446 §7.1) and the handshake transcript.

  import "std/tls" as tls
  var t = tls::transcript_new(32)              // 32 = SHA-256 suite, 48 = SHA-384
  tls::transcript_add(t, client_hello)
  tls::transcript_add(t, server_hello)
  var s = tls::schedule_start(32, "")          // no PSK
  tls::schedule_handshake(s, ecdhe_shared)
  let shs = tls::server_handshake_traffic(s, tls::transcript_hash(t))
  let k = tls::traffic_keys(32, shs, 16)       // k.key, k.iv

Everything here is a pure function of its inputs (no sockets, no state beyond what a caller
holds), which is what lets the whole schedule be checked against RFC 8448's published traces
with no network and no server.

The hash is named by its output length, 32 or 48, because that is what the construction uses.
A cipher suite chooses it; nothing here guesses.
### `struct TrafficKeys`

the key and IV a traffic secret produces. `iv` is always 12 bytes: it is XORed with the record
sequence number to make each record's AEAD nonce, so its length is the nonce's, not the key's.

### `struct Transcript`

the running handshake transcript, and the hash it is hashed with.

### `struct Schedule`

the three extracted secrets, in the order the schedule produces them. `handshake` and `master`
are "" until the corresponding step has run; a caller that derives from them too early gets
nothing rather than something plausible.

### `fn hkdf_label(label: Str, ctx: Str, n: Int) -> Str`

The `HkdfLabel` structure, serialised: the `info` that HKDF-Expand is called with.

    uint16 length; opaque label<7..255>; opaque context<0..255>

The `"tls13 "` prefix is not decoration. It is what keeps a TLS 1.3 key from colliding with a
key some other protocol derives from the same secret under the same name, and it is included in
every label's length byte.

"" if the label or context is too long to describe in the single length byte each gets.

### `fn expand_label(hlen: Int, secret: Str, label: Str, ctx: Str, n: Int) -> Str`

HKDF-Expand-Label: `n` bytes from `secret`, bound to a TLS label and a context.

### `fn derive_secret(hlen: Int, secret: Str, label: Str, thash: Str) -> Str`

Derive-Secret: one hash-length secret from another, bound to a label and a transcript hash.

The context is a hash of messages, never the messages themselves, which is why a whole
handshake of any length costs 32 or 48 bytes here.

### `fn schedule_start(hlen: Int, psk: Str) -> Schedule`

Start a schedule: the early secret, from a PSK or from nothing.

An empty `psk` means a block of zeros, which is the ordinary case: a full handshake has no
pre-shared key, and the schedule still runs with a well-defined early secret rather than a
special case.

### `fn schedule_handshake(inout s: Schedule, ecdhe: Str)`

Mix the (EC)DHE shared secret in, producing the handshake secret.

### `fn schedule_master(inout s: Schedule)`

Close the schedule, producing the master secret. There is nothing left to mix in, so the input
keying material is zeros; the entropy is already all in the salt.

### `fn client_handshake_traffic(s: Schedule, thash: Str) -> Str`

client_handshake_traffic_secret: `thash` covers ClientHello..ServerHello.

### `fn server_handshake_traffic(s: Schedule, thash: Str) -> Str`

server_handshake_traffic_secret: `thash` covers ClientHello..ServerHello.

### `fn client_app_traffic(s: Schedule, thash: Str) -> Str`

client_application_traffic_secret_0: `thash` covers ClientHello..server Finished.

### `fn server_app_traffic(s: Schedule, thash: Str) -> Str`

server_application_traffic_secret_0: `thash` covers ClientHello..server Finished.

### `fn exporter_master(s: Schedule, thash: Str) -> Str`

exporter_master_secret: `thash` covers ClientHello..server Finished.

### `fn resumption_master(s: Schedule, thash: Str) -> Str`

resumption_master_secret: `thash` covers ClientHello..client Finished, which is a different
point in the transcript from every other secret above.

### `fn traffic_keys(hlen: Int, secret: Str, keylen: Int) -> TrafficKeys`

The record-protection key and IV for a traffic secret. `keylen` comes from the cipher suite:
16 for AES-128-GCM, 32 for AES-256-GCM.

### `fn finished_key(hlen: Int, secret: Str) -> Str`

The key a Finished message is MACed with. Derived from the traffic secret with an empty
context; the transcript enters through the MAC's message, not through this derivation.

### `fn verify_data(hlen: Int, secret: Str, thash: Str) -> Str`

The `verify_data` of a Finished message: HMAC(finished_key, transcript_hash).

This is what makes the handshake tamper-evident. Both sides compute it over everything said so
far, so a modified message anywhere earlier changes it, which is why an implementation must
compare it with a constant-time equality and refuse on any difference.

### `fn key_update(hlen: Int, secret: Str) -> Str`

The next application traffic secret after a KeyUpdate. The old one cannot be recovered from it,
which is the whole point: a key compromised later does not open records sent earlier.

### `fn transcript_new(hlen: Int) -> Transcript`

A fresh, empty transcript for the hash named by `hlen`.

### `fn transcript_add(inout t: Transcript, msg: Str)`

Append one complete handshake message, with its four-byte header, as it appears on the
wire, and never the record framing around it.

### `fn transcript_hash(t: Transcript) -> Str`

The transcript hash as it stands.

### `fn hrr_random() -> Str`

The special Random that marks a ServerHello as a HelloRetryRequest: the SHA-256 of the string
"HelloRetryRequest", fixed by RFC 8446 §4.1.3. HelloRetryRequest reuses the ServerHello message
type, so this value is the only thing distinguishing them.

### `fn is_hello_retry_request(server_hello: Str) -> Bool`

Is this ServerHello actually a HelloRetryRequest? The Random sits at offset 6: four bytes of
handshake header, then two of legacy_version.

### `fn transcript_hrr(inout t: Transcript)`

Apply the HelloRetryRequest transcript substitution (RFC 8446 §4.4.1).

This replaces what the transcript holds rather than adding to it. Everything accumulated so far
(the first ClientHello) becomes a single synthetic message:

    message_hash(254) ‖ 00 00 Hash.length ‖ Hash(ClientHello1)

Call it after the first ClientHello and before adding the HelloRetryRequest itself. Getting
this wrong is not a visible failure on the client's side: the handshake simply ends in a
decrypt error much later, with nothing pointing back here.


### `struct TrustStore`

The roots a chain may terminate at. Nothing is trusted that is not in here.

### `fn max_chain_depth() -> Int`

How many certificates a path may contain, leaf and anchor included.

Not a performance limit. Without one, a set of certificates that issue each other in a cycle
makes path building run forever, and a server chooses what it sends.

### `fn trust_store_new() -> TrustStore`

An empty trust store. Nothing is trusted until roots are added, and a chain checked
against an empty store is refused rather than accepted.

### `fn trust_count(ts: TrustStore) -> Int`

how many roots the store holds

### `fn trust_add_pem(inout ts: TrustStore, pem: Str) -> Int`

Add every certificate in a PEM bundle. Returns how many parsed.

Entries that do not parse are skipped rather than fatal: a system bundle is a large file
maintained by someone else, and refusing to start because one of two hundred roots uses
something unusual would be worse than proceeding with the rest.

### `fn trust_add_der(inout ts: TrustStore, der: Str) -> Int`

Add one DER-encoded certificate (the raw bytes, not base64). Returns 1 when it parsed, else 0.
This is what a platform store hands out, where a PEM file is what a Unix system keeps on disk.

### `fn trust_add_file(inout ts: TrustStore, path: Str) -> Int`

Load a PEM bundle from a file.

### `fn trust_add_dir(inout ts: TrustStore, dir: Str) -> Int`

Load a directory of PEM files, as Android's `/system/etc/security/cacerts` is arranged: one
certificate per file, named by a hash of its subject.

### `fn trust_load_system(inout ts: TrustStore) -> Int`

Load the system trust store from wherever this platform keeps it. Returns how many roots.

`SSL_CERT_FILE` and `SSL_CERT_DIR` are honoured first because that is how every other TLS
client on the machine can be pointed at a different set, and a client that ignores them is one
that cannot be tested against a private CA.

### `fn name_matches(presented: Str, host: Str) -> Bool`

Does a presented name from a certificate match the host asked for?

A wildcard is allowed only as an entire leftmost label: `*.example.com`, never `w*.example.com`
and never `*.com`. The partial forms are permitted by RFC 6125 and rejected by every browser,
because `w*.example.com` matching `www.example.com` also matches names the issuer never
considered. A bare `*` matches nothing at all.

A wildcard matches exactly one label. `*.example.com` covers `a.example.com` and must not cover
`a.b.example.com`: the classic mistake, and the one that turns a certificate for a subdomain
into a certificate for everything under it.

### `fn check_hostname(c: Cert, host: Str) -> Str`

Is this certificate valid for `host`? "" means yes, otherwise the reason.

The subjectAltName extension is required. The Common Name fallback was deprecated in 2011 and
removed from browsers years ago, and honouring it here would mean accepting certificates that
no other modern client accepts, including ones where the CN says something the CA never
validated as a hostname.

### `fn verify_chain(chain: Vec<Str>, ts: TrustStore, host: Str, now: Int) -> Str`

Validate `chain` (leaf first, as TLS sends it) against `ts`, for `host`, at `now`.

"" means the chain is good. Anything else names the first reason it is not, and it names it,
because a validator that answers only yes or no cannot be debugged and cannot be trusted to be
failing for the reason you think.

### `fn preload_system_trust() -> Int`

Read the system trust store into memory, so that every later verification uses that copy instead
of parsing a hundred-odd certificates again. Returns how many roots it holds (0 = none found).

Call it once, at startup, before spawning anything that fetches: it is the only writer of the
shared store, and its being the only one is what makes concurrent handshakes race-free. Calling
it is optional (`verify_chain_system` reads its own copy when the store is empty), but then
every connection pays for the store again.

### `fn verify_chain_system(chain: Vec<Str>, host: Str, now: Int) -> Str`

Validate a chain against the system trust store at the current time. The form a caller wants.


### `struct Cert`

A parsed certificate. `err` is "" only if every field below was read successfully.

### `fn parse_time(s: Str, generalized: Bool) -> Int`

A UTCTime or GeneralizedTime as unix seconds, or -1 if it is not one this accepts.

Only the `Z` forms are accepted. RFC 5280 requires them, and a local-time offset would mean a
validity window whose meaning depends on where the reader is standing.

UTCTime's two-digit year is the sliding window RFC 5280 fixes: 50 and above is 19xx, below is
20xx. It is not a heuristic to be improved on: a certificate issuer and a verifier that read
the same two digits differently disagree about when a certificate expires.

### `struct AlgId`

An AlgorithmIdentifier: an OID and whatever parameters followed it, kept as raw DER because
RSA-PSS carries its whole configuration there and it must be read as written.

### `fn parse_cert(der: Str) -> Cert`

Parse a DER certificate. `err` says why not, and every other field is meaningless when it is set.

### `fn key_usage_digital_signature() -> Int`

keyUsage bit 0, digitalSignature: 0x80.

### `fn key_usage_cert_sign() -> Int`

keyUsage bit 5, keyCertSign: 0x04. The bit that says a key may sign other certificates.

### `fn key_usage_crl_sign() -> Int`

keyUsage bit 6, cRLSign: 0x02.

### `fn cert_time_valid(c: Cert, now: Int) -> Bool`

Is the certificate within its validity window at `now` (unix seconds)?

### `fn rsa_pub_from_der(spki_key: Str) -> crypto::RsaPub`

The RSA modulus and exponent from a SubjectPublicKeyInfo's key bits.

Here rather than in `std/crypto/rsa.nori` because it is DER parsing, and the DER reader lives
in this module (`std/tls` imports `std/crypto`, not the other way round). The split is a
reasonable one anyway: the arithmetic of RSA and the encoding of an X.509 key are different
problems, and only one of them reads hostile bytes.

RSAPublicKey ::= SEQUENCE { modulus INTEGER, publicExponent INTEGER }. An empty modulus means
it did not parse, and a caller must check: a key with no modulus verifies nothing, which is
indistinguishable from a key that simply rejects every signature.

### `fn ecdsa_verify_der(curve: crypto::EcCurve, pubkey: Str, digest: Str, der: Str) -> Bool`

Verify an ECDSA signature whose r and s arrive DER-encoded, as they do in a certificate and in
a TLS CertificateVerify: SEQUENCE { r INTEGER, s INTEGER }.

Here rather than beside the curve arithmetic for the same reason as `rsa_pub_from_der`: this is
DER parsing, and `std/crypto` cannot see the DER reader.

The encoding is checked strictly. A signature with a non-minimal integer, trailing bytes, or a
negative value is refused rather than repaired; those are the malleable re-encodings
that let one signature be presented as several.

### `fn curve_of(oid: Str) -> crypto::EcCurve`

The curve a SubjectPublicKeyInfo named, or an empty one (len 0) if it is not one we implement.

### `fn sig_hash_len(oid: Str) -> Int`

The hash a signature algorithm OID names, as its output length. 0 means this client will not
verify signatures of that kind.

SHA-1 and MD5 are absent on purpose, not by omission. Both are collision-broken, and a
collision is the attack a certificate signature must resist: an attacker who can find
one gets a signature over a certificate the CA never saw. A verifier that still accepts them
hands back every guarantee the chain was supposed to provide.

### `fn rsa_min_bits() -> Int`

The smallest RSA modulus this client will verify with.

1024-bit RSA is within reach of a well-resourced attacker and has been off the public web for
years; accepting one means accepting a chain anchored in a key someone may already have
factored. Certificates using them still exist, so this is a refusal that will occasionally cost
a connection, and that is the intended trade.

### `fn verify_signed_by(cert: Cert, issuer: Cert) -> Str`

Verify that `cert` was signed by the key in `issuer`. "" means the signature is good.

This is one link. It says nothing about whether the issuer is trusted, whether either
certificate is in date, or whether the issuer was allowed to sign; those are chain validation,
and keeping them apart is what stops a passing signature from being mistaken for a passing
chain.

### `fn verify_transcript_signature(leaf: Cert, cv: CertVerify, content: Str) -> Str`

Verify a TLS 1.3 CertificateVerify: `cv`'s signature over `content`, under the leaf's key.

TLS 1.3's algorithm set is narrower than X.509's, and the difference is the point. PKCS#1 v1.5
is forbidden here (RFC 8446 §4.4.3 requires PSS) even though certificates in the same chain
are routinely signed with v1.5. A verifier that reused its certificate policy for this message
would accept a signature scheme the protocol removed.


