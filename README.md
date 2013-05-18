# pgpcre

[![Build Status](https://secure.travis-ci.org/petere/pgpcre.png)](http://travis-ci.org/petere/pgpcre)

This is a module for PostgreSQL that exposes Perl-compatible regular expressions (PCRE) functionality as functions and operators.  It is based on the popular [PCRE library](http://www.pcre.org/).

## Installation

You need to have libpcre installed.  pkg-config will be used to find it.

To build and install this module:

    make
    make install

or selecting a specific PostgreSQL installation:

    make PG_CONFIG=/some/where/bin/pg_config
    make PG_CONFIG=/some/where/bin/pg_config install

And finally inside the database:

    CREATE EXTENSION pgpcre;

## Using

A regular expression is a separate data type, named `pcre`.  (This is different from how the built-in regular expressions in PostgreSQL work, which are simply values of type `text`.)

The supported regular expressions are documented on the [pcrepattern(3)](http://linux.die.net/man/3/pcrepattern) man page.

### Basic matching

Boolean operators are available for checking whether a pattern matches a string.  These operators return true or false, respectively.  They only return null when one of the operands is null.

Examples:

    SELECT 'foo' ~ pcre 'fo+';
    SELECT 'bar' !~ pcre 'fo+';

You can also write it the other way around:

    SELECT pcre 'fo+' ~ 'foo';
    SELECT pcre 'fo+' !~ 'bar';

This can be handy for writing things like

    SELECT pcre 'fo+' ~ ANY(ARRAY['foo', 'bar']);

For Perl nostalgia, you can also use this operator:

    SELECT 'foo' =~ pcre 'fo+';

And if this operator is unique (which it should be, unless you have
something else installed that uses it), you can also write:

    SELECT 'foo' =~ 'fo+';

(The `~` operator, by contrast, is not unique, of course, because it is used by the built-in regular expressions.)

To get case-insensitive matching, set the appropriate option in the pattern, for example:

    SELECT 'FOO' ~ pcre '(?i)fo+';

### Extracting the matched string

To extract the substring that was matched by the pattern, use the
function `pcre_match`.  It returns either a value of type text, or
null if the pattern did not match.  Examples:

    SELECT pcre_match('fo+', 'foobar');  --> 'foo'
    SELECT pcre_match('fo+', 'barbar');  --> NULL

There is no support for extracting multiple matches of a pattern in a
string, because PCRE does not (easily) support that.

### Extracting captured substrings

Captured substrings (parenthesized subexpressions) are extracted using
the function `pcre_captured_substrings`.  It returns either an array
of text, or null if the pattern did not match.  Examples:

    SELECT pcre_captured_substrings('(fo+)(b..)', 'foobar');  --> ARRAY['foo','bar']
    SELECT pcre_captured_substrings('(fo+)(b..)', 'abcdef');  --> NULL

Note that elements of the array can be null if a substring was not used, for example:

    SELECT pcre_captured_substrings('(a|(z))(bc)', 'abc');  --> ARRAY['a',NULL,'bc']

## Discussion

Some possible advantages over the regular expression support built into PostgreSQL:

- richer pattern language, more familiar to Perl and Python programmers
- complete Unicode support
- saner operators and functions

Some disadvantages:

- no repeated matching (`'g'` flag)
- no index optimization

You can workaround the lack of index optimization by manually augmenting queries like

    column =~ '^foo'

with

    AND column ~>=~ 'foo' AND column ~<~ 'fop'

and creating the appropriate `text_pattern_ops` index as you would for the built-in pattern matching.
