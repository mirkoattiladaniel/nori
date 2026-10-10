# std/http

```nori
import "std/http" as http
```

### `struct Url`

A parsed URL. `port` is filled in from the scheme when the URL omits it.

### `struct HttpResponse`

A completed response. `err` is "" on success; `final_url` is where redirects landed.

### `fn default_max_body() -> Int`

The largest response this client will hold, unless a caller says otherwise (16 MiB).

A cap is not optional. Both Content-Length and chunked framing let a server announce a size,
and a client that believes one allocates whatever it is told to.

### `fn max_redirects() -> Int`

How many redirects to follow before giving up: enough for real sites, few enough that a
redirect loop ends.

### `fn parse_url(url: Str) -> Url`

Split a URL into its parts. Only http and https; anything else is refused by name rather than
guessed at.

### `fn decode_chunked(body: Str, cap: Int) -> HttpResponse`

Decode a chunked body (RFC 9112 §7.1). "" with `err` set if it is malformed.

Each chunk announces its own size in hex, and the sizes are a running total the server chooses,
so `cap` bounds the decoded result and a body that exceeds it is refused mid-stream rather than
after it has all been held.

### `fn post(url: Str, token: Str, ctype: Str, body: Str) -> Str`

POST `body` to `url` with an optional `X-Nori-Token`. "" on success, otherwise the reason.

Does not follow redirects. A redirect on a POST is where a body gets replayed to
somewhere the caller never named, and where, historically, clients have quietly turned it into
a GET. If a report endpoint moves, the caller should be told rather than have its body forwarded.

### `fn post_report(url: Str, token: Str, report: Str) -> Str`

POST a plain-text report.

### `fn fetch_once(url: Str, method: Str, extra_headers: Str, cap: Int) -> HttpResponse`

One request, with no redirect following.

### `fn fetch_with_body(url: Str, method: Str, extra_headers: Str, body: Str, cap: Int) -> HttpResponse`

One request that may carry a body. `fetch_once` is this with an empty one.

### `fn fetch(url: Str, cap: Int) -> HttpResponse`

Fetch a URL, following redirects. The response is the one that finally answered.

### `fn fetch_hdr(url: Str, cap: Int, extra: Str) -> HttpResponse`

Fetch a URL with extra request headers (CRLF-terminated lines), following redirects.

What a site sends is decided before any of it is rendered: plenty of servers choose their mobile
or desktop markup from the User-Agent alone, so a client naming itself honestly and unfamiliarly
is handed the desktop page, or an "unsupported browser" notice, however well it draws. A
`User-Agent:` line here replaces the default rather than joining it. The headers ride every hop,
because a redirect that lands on the mobile host must be asked the same question as the first.


std/http: HTTP(S) client + a minimal HTTP/1.1 server.
  import "std/http" as http

Client (std/tls for the handshake, std/net/dns.nori for resolution):
  http::http_get("https://example.com/x", "/tmp/x")   // 0 = ok

Server (over std/net, no extra native deps):
  struct Site {}
  impl http::Handler for Site {
      fn handle(self, req: Request) -> Response {
          if req.path == "/" { return http::html("<h1>hi</h1>") }
          return http::not_found()
      }
  }
  fn main() -> Int { return http::serve(http::loopback(), 8080, Site {}) }

The handler is a `Handler` trait object (not a closure) because struct types do not carry
through an `Fn(...)` parameter, while a trait carries them cleanly. Each connection is
read, handled and answered in its own task, and every task gets a `copy` of your handler, so the
handler struct's fields must be scalars and Strs (see 16_concurrency, "What may cross into a task").
For full control (streaming, custom status lines) use listen/accept/recv_request + send_response.
`serve` answers a HEAD with the head its handler's response would have and no body; `serve_limited`
takes the limits a request must keep to (`Limits`: head and body sizes, and how long the whole
request may take to arrive), and what breaks one is answered 431, 413, 501, 400 or 408.

Routing, cookies, forms and static files:

  let r = http::route(req, "GET", "/videos/:id")     // also `*rest`; "" method = any
  if r.ok { let id = http::param(r, "id") }
  let sid = http::req_cookie(req, "session")
  resp = http::set_cookie(resp, http::Cookie { name: "session", value: sid, max_age: 2592000 })
  let who = http::form_get(req, "username")          // urlencoded body, decoded
  return http::file_response("web", rel)             // content-type + traversal refused
### `struct Fetched`

a completed fetch: `rc` (0 = ok) and the URL the client landed on after redirects.

### `fn http_fetch(url: Str, dest: Str) -> Fetched`

download `url` into file `dest`, returning the result code and the effective URL after redirects
(so callers can resolve relative links against the final host, e.g. google.com -> www.google.com).

Nori's own HTTP and TLS all the way down: client.nori frames the request, std/tls does the
handshake and validates the chain, std/net/dns.nori resolves the name. No libcurl, no C TLS
stack, and no separate certificate-store logic: the trust store is loaded by the same code on
every platform.

### `fn http_fetch_hdr(url: Str, dest: Str, extra: Str) -> Fetched`

Same, with extra request headers (CRLF-terminated lines), e.g. a `User-Agent:` of your own.

### `fn http_get(url: Str, dest: Str) -> Int`

download `url` into file `dest`. 0 on success, non-zero on any transport/HTTP error (HTTP >= 400 fails).
Follows redirects; supports https://, http:// and file://. The TLS handshake is std/tls's.

### `fn http_get_hdr(url: Str, dest: Str, extra: Str) -> Int`

Same, with extra request headers (CRLF-terminated lines).

### `struct Request`

A parsed HTTP request. `query` and `headers` are lowercase-keyed maps (use req_query / req_header).

### `struct Response`

An HTTP response: a status, a Content-Type, any extra header lines, and a (possibly binary) body.

### `trait Handler`

Implement `Handler` and pass it to `serve`; `handle` is called once per request.

### `fn handle(self, req: Request) -> Response`

turn a request into a response.

### `fn url_encode(s: Str) -> Str`

percent-encode a string for use inside a URL query value (space -> %20).

### `fn url_decode(s: Str) -> Str`

percent-decode a URL query value ('+' -> space, %XX -> byte).

### `fn html_escape(s: Str) -> Str`

escape a string for safe inclusion in HTML text / attribute values.

### `fn parse_query(rq: Str) -> Map<Str, Str>`

parse a URL query string (`a=1&b=two`, no leading `?`) into a map, percent-decoding both
halves. A key with no `=` maps to the empty string; a repeated key keeps the last value.

### `fn parse_request(raw: Str) -> Request`

parse a raw HTTP request (request line + headers + optional body) into a Request.

### `fn req_query(req: Request, key: Str) -> Str`

a query parameter (percent-decoded), or "" if absent.

### `fn req_header(req: Request, key: Str) -> Str`

a request header by (case-insensitive) name, or "" if absent.

### `fn response(status: Int, ctype: Str, body: Str) -> Response`

build a Response with an explicit status, Content-Type, and body.

### `fn html(body: Str) -> Response`

200 text/html.

### `fn text(body: Str) -> Response`

200 text/plain.

### `fn json(body: Str) -> Response`

200 application/json.

### `fn not_found() -> Response`

404 Not Found with a tiny HTML body.

### `fn redirect(loc: Str) -> Response`

303 See Other, redirecting to `loc`.

### `fn with_header(inout r: Response, name: Str, value: Str) -> Response`

add a raw header line ("Name: Value") to a Response, returning it.

### `fn response_bytes(r: Response) -> Str`

the full wire form of a Response (head + body) as one Str; a 304, 204 or 1xx never carries a body.

### `fn resp_status(r: Response) -> Int`

a Response's status code.

### `fn resp_body(r: Response) -> Str`

a Response's body.

### `fn req_cookie(req: Request, name: Str) -> Str`

a cookie sent by the browser, or "" if absent

### `struct Cookie`

A cookie to send. The defaults are the safe ones: readable only by the server, not
sent on cross-site requests (which is also the CSRF defence for form posts), and
scoped to the whole site. `max_age` of -1 means a session cookie (no Max-Age).
Turn on `secure` once you are on https: a Secure cookie is silently dropped over
plain http, which makes a local test look like a broken login.

### `fn cookie_header(c: Cookie) -> Str`

the Set-Cookie header value for `c`

### `fn set_cookie(inout r: Response, c: Cookie) -> Response`

attach a Set-Cookie to a response

### `fn clear_cookie(inout r: Response, name: Str, secure: Bool) -> Response`

expire a cookie in the browser (same name, empty value, Max-Age 0)

### `fn is_form(req: Request) -> Bool`

is this a urlencoded form post?

### `fn req_form(req: Request) -> Map<Str, Str>`

every field of a urlencoded body, percent-decoded (`+` is a space)

### `fn form_get(req: Request, key: Str) -> Str`

one form field, or "" if absent

### `struct Route`

the outcome of matching one route: whether it matched, and what it captured

### `fn route(req: Request, method: Str, pattern: Str) -> Route`

match `req` against a method and a path pattern ("" method = any)

### `fn route_path(path: Str, pattern: Str) -> Route`

match a path against a pattern, ignoring the method

### `fn param(r: Route, name: Str) -> Str`

a captured path parameter, or "" if this route did not capture it

### `fn path_is(req: Request, method: Str, path: Str) -> Bool`

method + exact path, for the routes that capture nothing

### `fn content_type_for(path: Str) -> Str`

a Content-Type guessed from the extension; text/plain when unrecognised

### `fn file_response(dir: Str, rel: Str) -> Response`

Serve `rel` from under `dir`.

The path comes off the network, so it is never resolved as given: a `..` anywhere,
a leading `/`, or a backslash is refused rather than normalised; normalising is
where directory-traversal bugs live. 404 for anything that is not an existing file.

### `fn loopback() -> net::Ip4`

the loopback address 127.0.0.1.

### `fn tcp_listen(ip: net::Ip4, port: Int) -> Int`

bind + listen on ip:port; returns a listening fd or -1.

### `fn tcp_accept(lfd: Int) -> Int`

accept one connection; returns a client fd or -1.

### `fn tcp_close(fd: Int) -> Int`

close a socket fd.

### `fn max_head_bytes() -> Int`

most bytes of request head (request line + headers) that will be accepted

### `fn max_body_bytes() -> Int`

most bytes of request body that will be read; larger requests are refused

### `fn read_timeout_ms() -> Int`

how long a whole request may take to arrive, in ms

### `struct Limits`

What `serve_limited` accepts from a client. A request over a limit is answered with
the status that names it (431 for the head, 413 for the body, 408 for the time)
and the connection closed.

### `fn default_limits() -> Limits`

the limits `serve` uses

### `struct Received`

How reading one request ended: its raw bytes (status 0), the status to refuse it with
(400, 408, 413, 431, 501), or -1 when the client went away and there is no one to tell.

### `fn recv_limited(fd: Int, lim: Limits) -> Received`

Read one complete request (the head, then exactly Content-Length bytes of body)
within `lim.timeout_ms` of starting.

For one read on a socket of your own (`serve` has a watchdog for all its connections instead).
The socket stays this task's. The read runs in a task of its own and says how it ended on a
channel; this task waits for that or the deadline. At the deadline it shuts the socket's
reading side, which ends the reader's parked recv, and waits for the reader to let go before
returning, so the caller can still answer 408 and close the socket, once. (Cancelling the
reader instead would have the scheduler close the socket under its owner: a cancelled task's
parked socket is closed, and the owner's later close would hit a number already reused.)

### `fn recv_request(fd: Int) -> Request`

Read one complete request: the head, then exactly Content-Length bytes of body.

Returns a Request whose `method` is "" if the client went away, sent more than the
caps allow, or stopped talking mid-request; the caller should close the socket.

### `fn recv_raw(fd: Int) -> Str`

The raw bytes of one complete request, or "" if it never arrived. Kept separate
from parsing because a Str can cross into a task and a parsed `Request` cannot:
it holds Maps.

### `fn recv_request_capped(fd: Int, hmax: Int, bmax: Int, timeout_ms: Int) -> Request`

`recv_request` with explicit limits, for a server that wants different ones.

### `fn recv_raw_capped(fd: Int, hmax: Int, bmax: Int, timeout_ms: Int) -> Str`

`recv_raw` with explicit limits; `timeout_ms` bounds the whole request.

### `fn response_bytes_for(method: Str, r: Response) -> Str`

The wire form of `r` as the answer to a request made with `method`: a HEAD gets the
head a GET would have got (the same Content-Length) and no body.

### `fn send_response(fd: Int, r: Response) -> Int`

write a Response to a client fd (head and body are sent separately so binary bodies aren't copied).

### `fn serve<H: Handler>(ip: net::Ip4, port: Int, h: H) -> Int`

Run the accept loop forever, dispatching each request to `h.handle`. Returns 1 if
the bind failed. Generic over the handler (monomorphized), so importing std/http
for the client alone costs nothing.

Every connection is read, handled and answered in its own task, so one slow
client, or one slow handler, delays nobody else. Each task gets a copy of the
handler (`copy h`), which is why `H` must be a struct of scalars and Strs: copying
is what makes it safe to hand to a task, and a Vec or Map field would still be
shared. Put anything mutable behind a `mutex`, or keep it in the tasks the handler
spawns itself.

A HEAD request is handled as a GET and answered without the body.

### `fn serve_limited<H: Handler>(ip: net::Ip4, port: Int, h: H, lim: Limits) -> Int`

`serve` with the limits a request must stay within (see `Limits`).


