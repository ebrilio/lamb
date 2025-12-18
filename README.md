# Lamb

Smallest Pure Functional Programming Language in C. Based on [Untype Lambda Calculus](https://en.wikipedia.org/wiki/Lambda_calculus) with Normal Order reduction.

## Quick Start

```console
$ cc -std=c99 -o lamb lamb.c
$ ./lamb
,---@>
 W-W'
Enter :help for more info
@> (\f.(\x.f (x x)) (\x.f (x x))) g
(\f.(\x.f (x x)) (\x.f (x x))) g
(\x.g (x x)) (\x.g (x x))
g ((\x.g (x x)) (\x.g (x x)))
g (g ((\x.g (x x)) (\x.g (x x))))
g (g (g ((\x.g (x x)) (\x.g (x x)))))
g (g (g (g ((\x.g (x x)) (\x.g (x x))))))
g (g (g (g (g ((\x.g (x x)) (\x.g (x x)))))))
g (g (g (g (g (g ((\x.g (x x)) (\x.g (x x))))))))
g (g (g (g (g (g (g ((\x.g (x x)) (\x.g (x x)))))))))
g (g (g (g (g (g (g (g ((\x.g (x x)) (\x.g (x x))))))))))
...
```

It's recommended to use Lamb with [rlwrap](https://github.com/hanslub42/rlwrap). Just do

```console
$ rlwrap ./lamb
```

and you get Bash-style history and command line navigation.

## Syntax

The syntax is based on the [Notation of Untype Lambda Calculus](https://en.wikipedia.org/wiki/Lambda_calculus_definition#Notation) and provides few improvements over it.

### Variables

Variables are any alphanumeric names:

```
@> x
x
@> hello69
hello69
@> 69420
69420
@>
```

Yes, fully numeric sequences of characters are also considered names, because they are alphanumeric. This may change in the future.

### Functions

To denote functions instead of small greek lambda `λ` we use backslash `\` (this may change in the future, we are considering allowing `λ` as an alternative):

```
@> \x.x
\x.x
@> \x.\y.x
\x.\y.x
@>
```

The body of the function extends as far right as possible. Use parenthesis to denote the boundaries of the function:

```
@> \x.x x
\x.x x
@> (\x.x) x
(\x.x) x
x
@>
```

Since the variable names can be longer than 1 character we can't use that silly mathematician trick of stitching their names together by saying that `\xy.x` means `\x.\y.x`, because in Lamb it just means it's a single function with a parameter `xy`:

```
@> \xy.x
\xy.x
@> (\xy.x) z
(\xy.x) z
x
@>
```

Instead we allow you to drop consequent backslashes turning the dot `.` into a parameter separator:

```
@> \x.y.x
\x.\y.x
@> (\x.y.x) z
(\x.\y.x) z
\y.z
@>
```

## Applications

Just separate two lambda expressions with a space:

```
@> (\x.x) x
(\x.x) x
x
@>
```

You can drop parenthesis if the expression is unambiguous:

```
@> f \x.x
f (\x.x)
@>
```

Applications are left-associative:

```
@> f a b c d
(((f a) b) c) d
@>
```
