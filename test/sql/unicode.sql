-- check that it counts characters, not bytes

SELECT 'ätsch' =~ '^.....$';
SELECT 'ätsch' =~ '^......$';


-- check Unicode properties

SELECT 'ätsch' =~ '^\w+$';


-- check Unicode in pattern

SELECT 'ätsch' =~ '^ätsch$';
