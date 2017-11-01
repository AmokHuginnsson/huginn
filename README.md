# Huginn programming language

The language implementation itself is provided by [**yaal**][1] library.
[This][2] project provides executor interface.

## Language

**Huginn** is a computer programming language with following traits:

- interpreted
- object oriented
- static type system
- strongly typed
- dynamically typed
- no quirks guarantee
- support arbitrary precision calculations per built-in type
- interpreter/executor trivially embeddable in C++ code

## Executor

The `huginn` program is an executor for Huginn programming language.
It allows execution of Huginn scripts,
it also provides an interactive `REPL` interface for the language,
additionally it works as **Jupyter**'s kernel core.

[1]: https://codestation.org/?h-action=menu-project&menu=submenu-project&page=&project=yaal
[2]: https://huginn.org/

