SET client_min_messages = warning;

CREATE TYPE pcre;

CREATE FUNCTION pcre_in(cstring)
RETURNS pcre
AS '$libdir/pgpcre'
LANGUAGE C STRICT;

CREATE FUNCTION pcre_out(pcre)
RETURNS cstring
AS '$libdir/pgpcre'
LANGUAGE C STRICT;

CREATE TYPE pcre (
    INTERNALLENGTH = -1,
    INPUT = pcre_in,
    OUTPUT = pcre_out,
    STORAGE = extended
);

CREATE FUNCTION pcre_text_matches(subject text, pattern pcre) RETURNS boolean
IMMUTABLE
RETURNS NULL ON NULL INPUT
AS '$libdir/pgpcre'
LANGUAGE C;

CREATE FUNCTION pcre_matches_text(pattern pcre, subject text) RETURNS boolean
IMMUTABLE
RETURNS NULL ON NULL INPUT
AS '$libdir/pgpcre'
LANGUAGE C;

CREATE FUNCTION pcre_text_matches_not(subject text, pattern pcre) RETURNS boolean
IMMUTABLE
RETURNS NULL ON NULL INPUT
AS '$libdir/pgpcre'
LANGUAGE C;

CREATE FUNCTION pcre_matches_text_not(pattern pcre, subject text) RETURNS boolean
IMMUTABLE
RETURNS NULL ON NULL INPUT
AS '$libdir/pgpcre'
LANGUAGE C;

CREATE OPERATOR =~ (
    PROCEDURE = pcre_text_matches,
    LEFTARG = text,
    RIGHTARG = pcre
);

CREATE OPERATOR ~ (
    PROCEDURE = pcre_text_matches,
    LEFTARG = text,
    RIGHTARG = pcre
);

CREATE OPERATOR ~ (
    PROCEDURE = pcre_matches_text,
    LEFTARG = pcre,
    RIGHTARG = text,
    COMMUTATOR = ~
);

CREATE OPERATOR !~ (
    PROCEDURE = pcre_text_matches_not,
    LEFTARG = text,
    RIGHTARG = pcre,
    NEGATOR = ~
);

CREATE OPERATOR !~ (
    PROCEDURE = pcre_matches_text_not,
    LEFTARG = pcre,
    RIGHTARG = text,
    COMMUTATOR = !~,
    NEGATOR = ~
);
