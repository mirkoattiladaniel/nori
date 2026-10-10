# 2. Lexical Structure

This chapter describes how source text is broken into tokens: comments,
identifiers, keywords, literals, and operators.

## Source encoding

Nori source files are UTF-8. Identifiers and keywords use ASCII letters, digits,
and underscores; string and character literals may contain any UTF-8 bytes. Line
endings may be LF or CRLF. Whitespace (spaces, tabs, newlines) separates tokens
but is otherwise insignificant; there are no semicolons and no layout rules.

## Comments

Nori has four comment forms:

```nori
// a line comment — runs to the end of the line

/* a block comment — does not nest; an unterminated one runs to EOF */

/// a documentation comment, attached to the item that follows it
fn area() -> Int { return 0 }

//! a module-level documentation comment, describing the whole file
```

`///` and `//!` are ordinary comments to the compiler, but `noric --doc` (and
`roll doc`) collect them into Markdown: `///` documents the next declaration,
`//!` documents the enclosing file. Doc comments are the source of the generated
API docs.

## Identifiers

An identifier starts with a letter or underscore and continues with letters,
digits, and underscores. Identifiers are case-sensitive.

```nori,excerpt
count   total_bytes   _tmp   Point   read_file   v2
```

By convention, functions, variables, and fields use `snake_case`; types
(`struct`, `enum`, `trait`) use `PascalCase`; global constants often use
`SCREAMING_CASE`. These are conventions, not rules.

## Keywords

### Reserved words

The following words are **reserved**; they are always keywords and can never be
used as identifiers:

```
as        break     continue  else      enum      false     fn        foreach
if        impl      import    in        inout     law       let       lock
match     over      reduce    region    return    schedule  set       sink
spawn     struct    trait     true      unsafe    var       while
```

That is the entire reserved set; the words the lexer itself knows. It is
intentionally small.

Two more words cannot be used as a name even so: `view` and `copy` begin an
expression (`view xs`, `copy s`), so `let view = 3` is read as a view of `3` and
goes wrong at the next token. Treat them as reserved in practice; they are listed
that way in the [Keyword Reference](21_keyword_reference.md).

### Contextual keywords

Many language features are spelled with words that are **not** reserved. They are
recognized only in the specific position where they are meaningful, and remain
ordinary identifiers everywhere else. These include:

```
pub        global     threadlocal frozen    extern     cfg       namespace
use        versioned  await      parallel   within     race      hedge
select     until      yield      never      shadow     with      or
naked      nori       for        durable    migrate    transient from
protocol   sends      streams    chooses    deprecated
```

For example, `within` introduces a deadline block *only* when it is immediately
followed by a duration and a `{`, otherwise `within` is just a name you could
bind with `let within = 3`. This keeps the surface expressive without a large
reserved vocabulary. Each contextual keyword is defined in its own chapter and
listed in the [Keyword Reference](21_keyword_reference.md).

### Built-ins are not keywords

Core operations such as `printl`, `push`, `pop`, `len`, `slen`, `to_str`,
`vec`, `map`, and `fill` are **built-in functions**, not keywords. They occupy
the ordinary function namespace and are called like any function. They are
documented alongside the types they operate on (e.g. `push`/`len` in
[Collections](09_collections.md), `slen`/`char_at` in
[Strings and Characters](06_strings_and_chars.md)).

### Names a program cannot define

A function, global or extern may take any name except:

| Name | Refused with |
|---|---|
| a built-in's (`push`, `len`, …) | `` `push` is a built-in — choose a different function name `` |
| a symbol the runtime links for the target: its `nori_*` entry points, the arena bases `g_s_base` `g_v_base` `g_o_base` `g_p_base` `g_m_base`, `g_sb_off`, `str_from_cstr`, the OpenMP entry points `__kmpc_*` | `` `nori_emit` is defined by the Nori runtime — choose a different function name `` |
| a function name starting `deepfree_`, `deepcopy_`, `copyclone_` or `freeclone_` (the per-type helpers the compiler generates) | `` … is named like the helpers the compiler generates per type `` |

The refusal is reported at the definition. The runtime's set follows the target's `cfg`: under
`--os bare` the runtime leaves `nori_czero`, `nori_rec_new` and the other floor entry points to the
floor, and a kernel may define them. An `extern fn` naming a runtime symbol only declares it and is
accepted.

Every other name the runtime uses internally (`cell`, `mrec`, `claim`, `str_hash`, `sys_read`, …) is
private to the runtime and free for a program.

## Literals

Literals are covered in detail in [Values and Types](03_values_and_types.md); in
brief:

```nori,excerpt
42            // decimal Int
0xFF          // hexadecimal Int (255)
0b1010        // binary Int (10)
3.14          // Float
true  false   // Bool
'z'           // Char (a single character; distinct from Int)
"text"        // Str
"x = ${n}"    // Str with interpolation — ${expr} splices a value
```

## Operators and punctuation

The operator and punctuation tokens are:

```
+   -   *   /   %              arithmetic
&   |   ^   ~   <<   >>        bitwise
==  !=  <   >   <=   >=        comparison
&&  ||  !                      logical
=                              assignment
->                             function return type
..                             range bounds (`0..n`, `foreach i in 0..len`)
=>                             match arm / lambda body
::                             namespaced path (module::name, Enum::Variant)
.                              field / method access
|…|                            closure parameter list
,   (   )   {   }   [   ]      grouping and separators
${ … }                         interpolation splice inside a string literal
```

`<<` and `>>` are each written as two adjacent `<`/`>` characters (with no space
between them); this lets nested generics like `Vec<Vec<Int>>` tokenize correctly.
Operator meanings and precedence are in [Operators](05_operators.md).

## Statements and line breaks

Nori has no statement terminator: a newline ends a statement. Whitespace is
otherwise insignificant, so an expression may be broken across lines freely —
with two exceptions, both about a suffix that opens a bracket.

**A `(` or `[` that continues an expression must be on the same line as what it
applies to.** On a new line it starts a new statement instead:

```nori,excerpt
let n = f(1)
(g())            // a separate statement — not a call of f(1)'s result

let v = xs
[0]              // a separate statement — not an index into xs

dbl
(g())            // two statements — not dbl(g())
```

Without this rule the two lines would fuse into one expression, and what that
did at run time would depend on what the first line's value happened to be. The
same rule already governs a loop label, which must sit on the same line as its
`break` or `continue` (see [Control Flow](07_control_flow.md)).

`.` and `@` are exempt, because no statement can begin with either; a leading
one is unambiguous, which is what lets a method chain break across lines:

```nori,excerpt
let n = xs
    .map(|x| x + 1)
    .len()
```

Arguments inside a call may of course span lines; it is only the opening `(`
that must stay with the callee.

---

Next: [Values and Types](03_values_and_types.md); the data every program works with.
