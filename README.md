# Huginn programming language

## Language

**Huginn** is a computer programming language with following traits:

- interpreted (multiplatform, what you type is what you get)
- imperative (use statements to change a program's state)
- functional style (functions, lambdas and closures are first class citizens)
- object oriented (user defined types, composition, inheritance, exceptions)
- introspective, non-reflective type system (user defined types are fixed and immutable at runtime, no monkey patching)
- strongly typed (absolutely no implicit value conversions and no implicit type coercion)
- dynamically typed (final type consistency checks are done at runtime)
- duck typing (a set of members determine the valid semantics of a type)
- no quirks guarantee (semantics is consistent across types in all contexts)
- support arbitrary precision calculations per built-in type (precision is restricted only by the amount of hardware resources)
- interpreter/executor trivially embeddable in C++ code

Full language documentation can be found at [Huginn home page][1].

## Executor

The `huginn` program is an executor for Huginn programming language.
It allows execution of Huginn scripts,
it can work as a stream editor,
it also provides an interactive `REPL` interface for the language,
additionally it works as **Jupyter**'s kernel core.

## Implementation

The language implementation itself is provided by [**yaal**][2] library.

This project provides only executor implementation.

## Installation

Huginn can be [installed][3] from sources or from prebuild binary packages.

Following platforms are supported via prebuild binary packages downloadable
with default package manager after subscribing to [Huginn package repository][4]:

- Ubuntu
- Debian (Stable and Sid)
- Fedora
- Centos
- FreeBSD

## Try it online

One can experiment with Huginn without necessity for installation of any software
via [Try It Onlilne][5] service.

[1]: https://huginn.org/
[2]: https://codestation.org/yaal
[3]: https://huginn.org/?h-action=bar-huginn&huginn=huginn-installation&menu=submenu-project&page=&project=huginn&
[4]: https://codestation.org/?h-action=menu-page&page=download
[5]: https://tio.run/#huginn

