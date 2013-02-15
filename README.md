# pgpcre

This is a module for PostgreSQL that exposes Perl-compatible regular expressions (PCRE) functionality as functions and operators.  It is based on the popular [PCRE library](http://www.pcre.org/).

Examples:

    SELECT 'foo' =~ 'fo+';

The supported regular expressions are documented on the [pcrepattern(3)](http://linux.die.net/man/3/pcrepattern) man page.

[![Build Status](https://secure.travis-ci.org/petere/pgpcre.png)](http://travis-ci.org/petere/pgpcre)
