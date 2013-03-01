# pgpcre

This is a module for PostgreSQL that exposes Perl-compatible regular expressions (PCRE) functionality as functions and operators.  It is based on the popular [PCRE library](http://www.pcre.org/).

Examples:

    SELECT 'foo' =~ 'fo+';

The supported regular expressions are documented on the [pcrepattern(3)](http://linux.die.net/man/3/pcrepattern) man page.

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

    psql -f pgpcre.sql

Extension support will be added later.


[![Build Status](https://secure.travis-ci.org/petere/pgpcre.png)](http://travis-ci.org/petere/pgpcre)
