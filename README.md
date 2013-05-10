# pgpcre

This is a module for PostgreSQL that exposes Perl-compatible regular expressions (PCRE) functionality as functions and operators.  It is based on the popular [PCRE library](http://www.pcre.org/).

A regular expression is a separate data type, named `pcre`.  (This is different from how the built-in regular expressions in PostgreSQL work, which are simply values of type `text`.)

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

The supported regular expressions are documented on the [pcrepattern(3)](http://linux.die.net/man/3/pcrepattern) man page.

To get case-insensitive matching, set the appropriate option in the pattern, for example:

    SELECT 'FOO' ~ pcre '(?i)fo+';

Some possible advantages over the regular expression support built into PostgreSQL:

- richer pattern language, more familiar to Perl and Python programmers
- complete Unicode support

Some disadvantages:

- no index optimization

You can workaround the lack of index optimization by manually augmenting queries like

    column =~ '^foo'

with

    AND column ~>=~ 'foo' AND column ~<~ 'fop'

and creating the appropriate `text_pattern_ops` index as you would for the built-in pattern matching.

## Installation

    CREATE EXTENSION pgpcre;


[![Build Status](https://secure.travis-ci.org/petere/pgpcre.png)](http://travis-ci.org/petere/pgpcre)
