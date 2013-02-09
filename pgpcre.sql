CREATE FUNCTION pcre_text_eq(subject text, pattern text) RETURNS boolean
IMMUTABLE
RETURNS NULL ON NULL INPUT
AS '$libdir/pgpcre'
LANGUAGE C;
