# std/net

```nori
import "std/net" as net
```

### `struct DnsAddr`

One resolved address: 4 bytes for IPv4, 16 for IPv6.

### `struct DnsResult`

What a lookup produced. `err` is "" on success; `addrs` is empty when it is not.

### `fn dns_type_a() -> Int`

DNS record type A: a 4-byte IPv4 address.

### `fn dns_type_aaaa() -> Int`

DNS record type AAAA: a 16-byte IPv6 address.

### `fn dns_timeout_ms() -> Int`

How long to wait for one server before trying the next.

### `fn dns_encode_name(host: Str) -> Str`

Encode a hostname as a DNS QNAME: each label length-prefixed, terminated by a zero byte.
"" if any label is empty or over 63 bytes, or the name is over 255: the wire format cannot
represent those, and a resolver that truncates instead would query a different name.

### `fn dns_build_query(id: Int, host: Str, qtype: Int) -> Str`

Build a query for `host` of type `qtype`, with the given 16-bit id.

### `fn dns_parse_response(msg: Str, id: Int, qtype: Int) -> DnsResult`

Read the addresses out of a DNS response, checking it answers `id`.

Records of a type that was not asked for are skipped rather than refused: a response legitimately
carries CNAMEs alongside the addresses, and refusing the whole answer because of one would mean
failing on most of the web.

### `fn dns_servers() -> Vec<Str>`

The nameservers from /etc/resolv.conf, in order. Empty when there are none.

### `fn dns_query_server(server: Str, host: Str, qtype: Int, id: Int) -> DnsResult`

Ask one server for `host`, returning what it said.

The query id is drawn from the clock and the port is whatever the OS gives an unbound socket.
Neither is a security measure (an attacker on the path sees both), and the real defence against
a forged answer is that the address is only used to open a connection whose certificate is then
checked. Matching the id at least rejects a stale datagram from a previous query.

### `fn dns_resolve4(host: Str) -> DnsResult`

Resolve `host` to IPv4 addresses, trying each configured nameserver in turn.

A dotted-quad is returned as itself without a query: asking a nameserver to resolve an address
is both pointless and, on a network with a hostile resolver, an opportunity.

### `fn resolve_ip4(host: Str) -> Ip4`

Resolve `host` and return the first IPv4 address, or a bad Ip4 if it could not be resolved.

### `fn tcp_connect_host(host: Str, port: Int) -> Int`

Resolve and connect in one step, the operation nearly every caller actually wants.
Returns a connected socket fd, or -1.


std/net: IPv4/IPv6 address types (parse/format/convert/classify) and TCP & UDP sockets.
No C shim: the socket layer calls the POSIX socket API directly via `extern fn` to libc
(linked automatically) with per-OS constants behind `cfg`. Fallible parsers return a struct with an
`ok` field (1/0); a recv returns `Recv { n, data }` (n>0 bytes, 0 = closed, <0 = error).

**Platform support.** Socket calls suspend the calling task rather than blocking its thread, so they
need a readiness backend for the stackless scheduler: Linux uses epoll, Windows uses WSAPoll. Both
are one-shot readiness, so the same code runs on either: a handler parks on each op instead of
occupying an OS thread. macOS is unported (kqueue fits the same shape); a socket call there aborts
with a clear message rather than misbehaving. A listening/bound `Socket` in a global re-establishes
itself across a program image on Linux and Windows.
### `struct Ip4`

An IPv4 address. `ok` is 0 when it did not parse.

### `fn ipv4(a: Int, b: Int, c: Int, d: Int) -> Ip4`

build a valid Ip4 from four octets a.b.c.d (ok=1).

### `fn parse_ipv4(s: Str) -> Ip4`

parse "a.b.c.d" (decimal octets, each 0..255, 1..3 digits). ok=0 on any malformation.

### `fn ipv4_str(ip: Ip4) -> Str`

"a.b.c.d"

### `fn ipv4_u32(ip: Ip4) -> Int`

32-bit big-endian integer form, and back

### `fn ipv4_from_u32(x: Int) -> Ip4`

build an Ip4 from a 32-bit big-endian integer.

### `fn ipv4_eq(p: Ip4, q: Ip4) -> Bool`

true if the two addresses have equal octets (ignores the ok flag).

### `fn ipv4_is_loopback(ip: Ip4) -> Bool`

true if loopback (127.0.0.0/8).

### `fn ipv4_is_unspecified(ip: Ip4) -> Bool`

true if the unspecified address 0.0.0.0.

### `fn ipv4_is_broadcast(ip: Ip4) -> Bool`

true if the limited broadcast address 255.255.255.255.

### `fn ipv4_is_multicast(ip: Ip4) -> Bool`

true if multicast (224.0.0.0/4).

### `fn ipv4_is_private(ip: Ip4) -> Bool`

true if in a private RFC1918 range (10/8, 172.16/12, 192.168/16).

### `struct Ip6`

An IPv6 address. `ok` is 0 when it did not parse.

### `fn ip6_of(g: Vec<Int>) -> Ip6`

build an Ip6 from a Vec of exactly 8 group values

### `fn ip6_group(ip: Ip6, i: Int) -> Int`

the i-th 16-bit group (0..7) of the address.

### `fn parse_groups(seg: Str, inout out: Vec<Int>) -> Bool`

parse one ':'-delimited run of groups into `out` (empty `seg` = no groups). Each group is 1..4 hex
digits, or a trailing embedded IPv4 dotted-quad (e.g. ::ffff:1.2.3.4) which expands to two groups.

### `fn find_dcolon(s: Str) -> Int`

the byte index of the "::" in `s`, or -1.

### `fn parse_ipv6(s: Str) -> Ip6`

parse an IPv6 textual address: optional one "::" zero-compression + optional trailing embedded IPv4.

### `fn ipv6_str(ip: Ip6) -> Str`

canonical text form, with "::" over the longest run (>=2) of zero groups.

### `fn ipv6_is_loopback(ip: Ip6) -> Bool`

true if the loopback address ::1.

### `fn ipv6_is_unspecified(ip: Ip6) -> Bool`

true if the unspecified address ::.

### `fn sock_close(fd: Int) -> Int`

close a socket fd; returns 0 on success, -1 on error.

### `fn wsa_init()`

init the socket subsystem (no-op on POSIX; WSAStartup on Windows).

### `fn set_nonblocking(fd: Int, on: Bool) -> Int`

set/clear O_NONBLOCK on fd; returns 0 on success, -1 on error.

### `fn poll_fd(fd: Int, events: Int, ms: Int) -> Int`

poll fd for `events` up to ms; returns revents bitmask, 0 on timeout, <=0 from poll on error.

### `fn af_inet6() -> Int`

AF_INET6 address-family constant for this OS.

### `fn sol_socket() -> Int`

SOL_SOCKET option-level constant for this OS.

### `fn so_reuseaddr() -> Int`

SO_REUSEADDR option-name constant for this OS.

### `fn put_sa_head(b: Int, salen: Int, fam: Int)`

write the sockaddr header (family) at the start of buffer b for this OS layout.

### `struct Recv`

One receive: `n` bytes read (0 = empty, <0 = error) and the bytes themselves.

### `fn tcp_connect4(ip: Ip4, port: Int) -> Int`

connect to a peer; returns a socket fd (>=0) or -1.

### `fn tcp_connect6(ip: Ip6, port: Int) -> Int`

connect (TCP/IPv6) to a peer; returns a socket fd (>=0) or -1.

### `fn tcp_listen4(ip: Ip4, port: Int, backlog: Int) -> Int`

bind + listen; returns a listening fd or -1.

### `fn tcp_accept(fd: Int) -> Int`

accept one pending connection on listening fd; returns a client fd or -1.

### `fn peer_ip4(fd: Int) -> Ip4`

Who is at the other end of an accepted socket (IPv4). ok=0 when it cannot be had: the socket
is closed, or the peer is not AF_INET.

A server usually does not care. It matters when the answer it is about to give depends on where
the asker is: handing 127.0.0.1 to something across the internet is not a smaller answer, it is
a wrong one, and the failure lands somewhere else entirely: on a phone, as a connection that
times out against itself.

### `fn is_loopback4(ip: Ip4) -> Bool`

Is this address on this machine? 127.0.0.0/8, which is useful to anything local and to nothing else.

### `struct Socket`

A re-establishable listening/bound socket. Unlike a bare listening fd (which
snapshot() refuses over), a `Socket` carries its bind address, so snapshot()/resume() re-listen on
the same ip:port on resume. Field order is fixed (runtime nori_sock_ser/de): fd@0, kind@1
(1=tcp-listen, 2=udp-bind), ip@2 (u32 host form), port@3. Accepted/connected client sockets are not
re-establishable and stay bare fds under the refuse rule.

### `fn socket_listen4(ip: Ip4, port: Int, backlog: Int) -> Socket`

bind + listen as a re-establishable Socket (re-binds itself on resume).

### `fn socket_fd(s: Socket) -> Int`

the raw listening fd (for accept loops / interop).

### `fn socket_accept(s: Socket) -> Int`

accept the next client connection; returns a client fd (>=0) or -1.

### `fn socket_close(s: Socket) -> Int`

close the listening socket.

### `fn tcp_send(fd: Int, data: Str) -> Int`

send all bytes of `data`; returns bytes sent or -1.

### `fn tcp_recv(fd: Int, max: Int) -> Recv`

receive up to `max` bytes.

### `fn tcp_close(fd: Int) -> Int`

close a TCP socket fd; returns 0 on success, -1 on error.

### `fn tcp_shutdown(fd: Int, how: Int) -> Int`

shut down part of a TCP connection (how: 0=read, 1=write, 2=both); returns 0 or -1.

### `fn udp_bind4(ip: Ip4, port: Int) -> Int`

open a UDP/IPv4 socket bound to ip:port; returns the fd or -1.

### `fn udp_open4() -> Int`

open an unbound UDP/IPv4 socket (for sending); returns the fd or -1.

### `fn udp_send_to4(fd: Int, ip: Ip4, port: Int, data: Str) -> Int`

send `data` as one UDP datagram to ip:port; returns bytes sent or -1.

### `fn udp_recv(fd: Int, max: Int) -> Recv`

receive one UDP datagram, up to `max` bytes; Recv.n>0 bytes, 0=empty, <0=error.

### `fn udp_close(fd: Int) -> Int`

close a UDP socket fd; returns 0 on success, -1 on error.

### `fn so_rcvtimeo() -> Int`

SO_RCVTIMEO option-name constant for this OS.

### `fn so_sndtimeo() -> Int`

SO_SNDTIMEO option-name constant for this OS.

### `fn so_error() -> Int`

SO_ERROR option-name constant for this OS.

### `fn set_timeo(fd: Int, opt: Int, ms: Int) -> Int`

per-socket recv/send timeout in milliseconds (0 = block forever). After it elapses, recv/send return <0.

### `fn set_recv_timeout(fd: Int, ms: Int) -> Int`

set the receive timeout (ms; 0 = block forever) on fd; returns 0 or -1.

### `fn set_send_timeout(fd: Int, ms: Int) -> Int`

set the send timeout (ms; 0 = block forever) on fd; returns 0 or -1.

### `fn poll_read(fd: Int, timeout_ms: Int) -> Int`

wait until `fd` is readable / writable: 1 = ready, 0 = timeout, -1 = error.
wait until fd is readable: 1 = ready, 0 = timeout, -1 = error.

### `fn poll_write(fd: Int, timeout_ms: Int) -> Int`

wait until fd is writable: 1 = ready, 0 = timeout, -1 = error.

### `fn tcp_connect_timeout4(ip: Ip4, port: Int, ms: Int) -> Int`

connect with a timeout (non-blocking connect + poll for writable + SO_ERROR check). fd or -1.


