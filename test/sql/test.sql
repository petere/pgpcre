SELECT pcre_text_eq('foo', 'fo+');
SELECT pcre_text_eq('bar', 'fo+');
SELECT pcre_text_eq('error', '+');

SELECT 'foo' =~ 'fo+';
SELECT 'bar' =~ 'fo+';
SELECT 'error' =~ '+';
