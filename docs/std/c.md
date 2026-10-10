# std/c

```nori
import "std/c" as c
```

std/c: a C declarations front end: preprocessor, tokenizer, declaration parser, x86-64 System V
layout and constant evaluation, in Nori. It reads a header the way a GCC-compatible compiler on
x86-64 Linux does and answers with plain data: functions, records with their layout, enums,
typedefs, variables, `#define` constants, and a located note for every declaration it skipped.

  import "std/c" as c
  let u = c::parse_file("/usr/include/zlib.h", c::default_options())
  foreach f in u.functions { printl(c::prototype_str(u, f)) }
  let r = c::find_record(u, "z_stream_s")
  printl("z_stream is ${u.records[r].size} bytes")

## Types

A type is an `Int` index into `unit.types`. Each `Type` carries a `kind` (one of the `K_*`
constants), qualifier bits (`Q_CONST`, `Q_VOLATILE`, `Q_RESTRICT`, `Q_ATOMIC`), and by kind: the
pointee, element, return or underlying type in `base`; the array length in `len` (`-1` for a
flexible or unsized array); the record, enum or typedef index in `ref`; the parameters and the
variadic flag of a function type; and in `align` an alignment set on the type itself (a
typedef's `aligned` attribute, a `vector_size` type), 0 when the kind's own applies. The
unqualified primitive types sit at the index equal to their kind, so `K_INT` is also the type
`int`.

## Preprocessor

`#include "..."` searches the including file's directory, then `include_dirs`, then `system_dirs`;
`#include <...>` skips the first. `#include_next` resumes after the directory the current file was
found in, and `#include MACRO` expands first. Include guards and `#pragma once` stop a second
read. Macros follow the C standard's rescanning rules: `#`, `##`, `__VA_ARGS__`, `__VA_OPT__`,
GNU `, ## __VA_ARGS__` and named variadic parameters, and a macro never expands inside its own
expansion. `#if`/`#elif` evaluate full 64-bit expressions with `defined`, `__has_include`,
`__has_include_next`, `__has_attribute` and `__has_builtin` (true for the attributes and builtins
GCC has), `__has_feature` and the rest (false), and read an unknown name as 0. `#elifdef`,
`#elifndef`, `#pragma pack`, `__FILE__`, `__LINE__` and `__COUNTER__` are supported; `#line`,
`#warning` and `#ident` are accepted and ignored.

The predefined macros are those of GCC 13 compiling C17 for x86-64 Linux (`__x86_64__`,
`__linux__`, `__LP64__`, `__GNUC__`, `__STDC_VERSION__` 201710L, the `__SIZEOF_*__`, `*_MAX__`,
`*_TYPE__` and float-model families), so glibc takes its usual paths. With `target` "windows" in
the options they are those of the mingw-w64 GCC for x86_64-w64-mingw32 instead (`_WIN32`,
`_WIN64`, `__MINGW64__`, no `__linux__`/`__unix__`/`__LP64__`, `__SIZEOF_LONG__` 4,
`__SIZEOF_WCHAR_T__` 2, 64-bit types spelled `long long`).

## Declarations

typedefs; struct, union and enum definitions, forward declarations and tags used before their
definition; anonymous and nested records; bitfields; arrays with constant-expression lengths,
flexible members and parameters' variable lengths; declarators of any nesting; `const`,
`volatile`, `restrict` and `_Atomic` in their GNU spellings too; storage classes and function
specifiers; `__attribute__((...))` and `[[...]]` wherever they may stand (`packed`, `aligned`,
`mode` and `vector_size` take effect), `__asm__`, `__extension__`, `_Static_assert`, `_Alignas`;
`__typeof__` of a type or a declared name; `__int128`, `_Float128` and the `_FloatN` family,
`_Complex`, `__builtin_va_list` (an array of one 24-byte `struct __va_list_tag`). Function
definitions, old-style ones included, are recorded and their bodies skipped. Parameters of array
and function type decay to pointers. A redeclaration keeps the first declaration.

## What is noted and skipped

Nothing in a header stops the parse. A declaration the parser cannot read is skipped to its `;`
(or its closing `}`) and recorded in `notes` with its file, line and a short reason. `#error` in
a live branch, an unresolvable `#include`, `__typeof__` of an expression that is not a known name,
and `mode` attributes on non-integer types are noted the same way. An unresolvable include is
noted as `include not found: NAME`, with the name in quotes when the include was quoted.

## Layout

Sizes, alignments and field offsets follow the x86-64 System V ABI as GCC and clang implement it:
padding, unions, bitfields allocated in units of their declared type (a field that does not fit
starts a new unit, a zero-width field aligns to the next unit and does not raise the record's
alignment, an unnamed field does not either), flexible array members, `packed`, `aligned(N)` and
`#pragma pack`. Field offsets are in bits. The fields of an anonymous struct or union member are
listed after it with `promoted` set and their offsets taken from the enclosing record.

For the "windows" target the data model is LLP64: `long` and `unsigned long` are 4 bytes (`long
long`, pointers and `size_t` stay 8), an `L` suffix makes a 32-bit constant when the value fits,
`L'x'` is an unsigned 16-bit `wchar_t`, `__builtin_va_list` is a `char *`, and bitfields are laid
out as mingw-w64 GCC does by default (`-mms-bitfields`): a bitfield starts a new unit whenever the
previous member was not a bitfield of a type of the same size or its unit has too few bits left,
and unnamed bitfields count towards the record's alignment.
### `global K_VOID: Int = 0`

`void`

### `global K_BOOL: Int = 1`

`_Bool`

### `global K_CHAR: Int = 2`

plain `char` (signed on x86-64)

### `global K_SCHAR: Int = 3`

`signed char`

### `global K_UCHAR: Int = 4`

`unsigned char`

### `global K_SHORT: Int = 5`

`short`

### `global K_USHORT: Int = 6`

`unsigned short`

### `global K_INT: Int = 7`

`int`

### `global K_UINT: Int = 8`

`unsigned int`

### `global K_LONG: Int = 9`

`long`

### `global K_ULONG: Int = 10`

`unsigned long`

### `global K_LLONG: Int = 11`

`long long`

### `global K_ULLONG: Int = 12`

`unsigned long long`

### `global K_INT128: Int = 13`

`__int128`

### `global K_UINT128: Int = 14`

`unsigned __int128`

### `global K_FLOAT: Int = 15`

`float`

### `global K_DOUBLE: Int = 16`

`double`

### `global K_LDOUBLE: Int = 17`

`long double` (80-bit extended, 16 bytes)

### `global K_FLOAT128: Int = 18`

`_Float128` / `__float128`

### `global K_CFLOAT: Int = 19`

`float _Complex`

### `global K_CDOUBLE: Int = 20`

`double _Complex`

### `global K_CLDOUBLE: Int = 21`

`long double _Complex`

### `global K_FLOAT16: Int = 22`

`_Float16`

### `global K_POINTER: Int = 23`

a pointer; `base` is the pointee

### `global K_ARRAY: Int = 24`

an array; `base` is the element, `len` the length or -1

### `global K_FUNCTION: Int = 25`

a function; `base` is the return type, `params` and `variadic` the rest

### `global K_RECORD: Int = 26`

a struct or union; `ref` indexes `unit.records`

### `global K_ENUM: Int = 27`

an enum; `ref` indexes `unit.enums`

### `global K_TYPEDEF: Int = 28`

a typedef name; `ref` indexes `unit.typedefs`

### `global Q_CONST: Int = 1`

`const`

### `global Q_VOLATILE: Int = 2`

`volatile`

### `global Q_RESTRICT: Int = 4`

`restrict`

### `global Q_ATOMIC: Int = 8`

`_Atomic`

### `global CONST_INT: Int = 0`

a `Constant` holding an integer

### `global CONST_FLOAT: Int = 1`

a `Constant` holding a floating value

### `global CONST_STRING: Int = 2`

a `Constant` holding a string literal

### `struct Options`

where to look for headers and which macros to start from.

`include_dirs` are searched first (for both `"..."` and `<...>` includes, after the including
file's own directory for `"..."`), then `system_dirs`. `defines` are `NAME` or `NAME=VALUE`,
applied after the predefined macros; `undefines` are removed after that. `target` is "" (or
"linux") for x86-64 Linux and "windows" for x86_64-w64-mingw32: it picks the predefined macros
and the data model (LP64 or LLP64) sizes and layouts are computed in.

### `struct Param`

a parameter of a function type or declaration; `name` is "" when the declaration has none.

### `struct Type`

one C type; see the module overview for how the fields read per kind.

### `struct Field`

a struct or union member. `offset` is in bits from the start of the record; `width` is the
bitfield width or -1 for an ordinary member. An anonymous struct or union member has `name` "" and
is followed by its own fields with `promoted` set.

### `struct Record`

a struct or union. `name` is the tag, or a synthetic `__anon_<kind>_<n>` for an untagged one
(`anonymous` is set). An incomplete record was declared but never defined: size and align are 0.

### `struct EnumConst`

an enumerator and its value.

### `struct Enum`

an enum with its constants; `size` is 4 or 8 (1 or 2 when packed), `unsigned` says whether the
underlying type is unsigned.

### `struct Typedef`

a `typedef` name and the type it names.

### `struct Function`

a function declaration or definition (an inline body in a header is skipped, `definition` set).

### `struct Variable`

a file-scope object declaration (`extern int errno;`).

### `struct Constant`

an object-like `#define` from a header whose body is a constant: an integer expression
(`ivalue`, `unsigned`), a floating literal expression (`fvalue`) or a string literal (`svalue`, the
decoded bytes). `kind` is one of the `CONST_*` values.

### `struct Note`

a located message about something the front end skipped.

### `struct Unit`

everything read from one translation unit. `file` fields index `files`. `long_size` is the size
of `long` in the target's data model: 8 (LP64) or 4 (LLP64, the "windows" target).

### `global TOK_IDENT: Int = 1`

the kind of a token in a `Translation`: an identifier or keyword

### `global TOK_NUMBER: Int = 2`

a preprocessing number (`42`, `0x1fUL`, `1.5e-3f`)

### `global TOK_CHAR: Int = 3`

a character constant, prefix included (`'a'`, `L'x'`)

### `global TOK_STRING: Int = 4`

a string literal, prefix included (`"abc"`, `u8"x"`)

### `global TOK_PUNCT: Int = 5`

a punctuator (`->`, `...`, `<<=`)

### `global TOK_OTHER: Int = 6`

any other character the tokenizer met (`@`, a stray backslash)

### `struct Macro`

a macro defined at the end of a translation unit. `body` is its replacement list, spelled with one
space wherever the definition had whitespace; `params` names a function-like macro's parameters
(`__VA_ARGS__` for an unnamed `...`).

### `struct Translation`

a translation unit together with the preprocessed tokens it was parsed from. Token `i` is of kind
`kinds[i]` (a `TOK_*` value) and spelled `texts[i]`; it comes from line `lines[i]` of
`unit.files[files[i]]`, and a token a macro produced is placed where the macro's name was used.
`spaced[i]` says whitespace or a line break comes before it. Adjacent string literals stay
separate tokens. `macros` are the macros still defined at the end, in definition order.

### `fn default_options() -> Options`

options for this host: no user include directories, and the system search path `/usr/local/include`,
the compiler's own builtin include directory (GCC's under /usr/lib/gcc/x86_64-pc-linux-gnu or
/usr/lib/gcc/x86_64-linux-gnu, else clang's under /usr/lib/clang) when one exists, and /usr/include.

### `fn windows_options() -> Options`

options for reading headers as x86_64-w64-mingw32 GCC does (`target` "windows"): the system search
path is that compiler's builtin include directory (under /usr/lib/gcc/x86_64-w64-mingw32) when one
is installed, then the mingw-w64 headers in /usr/x86_64-w64-mingw32/include.

### `fn bare_options() -> Options`

options with no search path at all: only `#include "..."` next to the including file resolves.

### `fn parse_file(path: Str, opts: Options) -> Unit`

preprocess and parse the header at `path`. A file that cannot be read yields a unit whose only
content is a note saying so.

### `fn parse_text(text: Str, name: Str, opts: Options) -> Unit`

preprocess and parse `text` as if it were a file called `name`; quoted includes resolve against
the directory part of `name`.

### `fn translate_file(path: Str, opts: Options) -> Translation`

preprocess and parse the file at `path` as `parse_file` does, keeping the token stream and the
macros.

### `fn translate_text(text: Str, name: Str, opts: Options) -> Translation`

preprocess and parse `text` as `parse_text` does, keeping the token stream and the macros.

### `fn find_record(u: Unit, name: Str) -> Int`

the index of the record tagged `name` (or with that synthetic name), or -1.

### `fn find_function(u: Unit, name: Str) -> Int`

the index of the function `name`, or -1.

### `fn find_typedef(u: Unit, name: Str) -> Int`

the index of the typedef `name`, or -1.

### `fn find_enum(u: Unit, name: Str) -> Int`

the index of the enum tagged `name`, or -1.

### `fn find_constant(u: Unit, name: Str) -> Int`

the index of the `#define` constant `name`, or -1.

### `fn find_field(u: Unit, r: Int, name: Str) -> Int`

the index of the field `name` in record `r` (promoted fields included), or -1.

### `fn enum_value(u: Unit, name: Str, dflt: Int) -> Int`

the value of the enumerator `name` in any enum; `dflt` when there is none.

### `fn loc_str(u: Unit, file: Int, line: Int) -> Str`

"file:line" for a file index and line.

### `fn resolve(u: Unit, ty: Int) -> Int`

`ty` with typedefs followed to the type they finally name, as a type index. Qualifiers written on
the typedefs passed through are not part of the answer; `quals_of` collects them.

### `fn quals_of(u: Unit, ty: Int) -> Int`

the qualifiers of `ty` including those of every typedef it passes through.

### `fn sizeof(u: Unit, ty: Int) -> Int`

the size of `ty` in bytes: 0 for an incomplete type, 1 for `void` and function types (as GCC).

### `fn alignof(u: Unit, ty: Int) -> Int`

the alignment of `ty` in bytes, an explicit `aligned`/`_Alignas` on the type included.

### `fn is_integer(u: Unit, ty: Int) -> Bool`

is `ty` (after typedefs) an integer type, `_Bool` and enums included?

### `fn is_unsigned(u: Unit, ty: Int) -> Bool`

is `ty` (after typedefs) an unsigned integer type? Enums answer by their underlying type.

### `fn is_float(u: Unit, ty: Int) -> Bool`

is `ty` (after typedefs) a floating type?

### `fn type_str(u: Unit, ty: Int) -> Str`

`ty` in C syntax as an abstract declarator (`int (*)(void)`, `struct stat *const`).

### `fn decl_str(u: Unit, ty: Int, name: Str) -> Str`

a declaration of `name` with type `ty` in C syntax (`void (*(*f)(int))(void)`); `name` may be "".

### `fn prototype_str(u: Unit, f: Function) -> Str`

the function as a C prototype: `int deflate(z_streamp strm, int flush)`.


















