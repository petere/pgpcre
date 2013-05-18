SELECT pcre 'fo+';
SELECT pcre '+';

SELECT 'foo' =~ 'fo+';
SELECT 'bar' =~ 'fo+';
SELECT 'error' =~ '+';

SELECT 'foo' ~ pcre 'fo+';
SELECT 'bar' ~ pcre 'fo+';

SELECT pcre 'fo+' ~ 'foo';
SELECT pcre 'fo+' ~ 'bar';

SELECT 'foo' !~ pcre 'fo+';
SELECT 'bar' !~ pcre 'fo+';

SELECT pcre 'fo+' !~ 'foo';
SELECT pcre 'fo+' !~ 'bar';

SELECT pcre 'fo+' ~ ANY(ARRAY['foo', 'bar']);

SELECT 'FOO' ~ pcre 'fo+';
SELECT 'FOO' ~ pcre '(?i)fo+';

SELECT pcre_match('fo+', 'foobar');
SELECT pcre_match('fo+', 'barbar');

SELECT pcre_captured_substrings('(fo+)(b..)', 'foobar');
SELECT pcre_captured_substrings('(fo+)(b..)', 'abcdef');
SELECT pcre_captured_substrings('(a|(z))(bc)', 'abc');
SELECT pcre_captured_substrings('(abc)(x(yz)?)?', 'abc');
