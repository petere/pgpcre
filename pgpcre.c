#include <postgres.h>
#include <catalog/pg_type.h>
#include <fmgr.h>
#include <mb/pg_wchar.h>
#include <utils/array.h>
#include <utils/builtins.h>

#include <pcre.h>

PG_MODULE_MAGIC;

Datum pcre_in(PG_FUNCTION_ARGS);
Datum pcre_out(PG_FUNCTION_ARGS);
Datum pcre_text_matches(PG_FUNCTION_ARGS);
Datum pcre_matches_text(PG_FUNCTION_ARGS);
Datum pcre_text_matches_not(PG_FUNCTION_ARGS);
Datum pcre_matches_text_not(PG_FUNCTION_ARGS);
Datum pcre_match(PG_FUNCTION_ARGS);
Datum pcre_captured_substrings(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(pcre_in);
PG_FUNCTION_INFO_V1(pcre_out);
PG_FUNCTION_INFO_V1(pcre_text_matches);
PG_FUNCTION_INFO_V1(pcre_matches_text);
PG_FUNCTION_INFO_V1(pcre_text_matches_not);
PG_FUNCTION_INFO_V1(pcre_matches_text_not);
PG_FUNCTION_INFO_V1(pcre_match);
PG_FUNCTION_INFO_V1(pcre_captured_substrings);

void _PG_init(void);

#ifndef FLEXIBLE_ARRAY_MEMBER
#define FLEXIBLE_ARRAY_MEMBER 1
#endif

typedef struct
{
	int32		vl_len_;
	int16		pcre_major;		/* new version might invalidate compiled pattern */
	int16		pcre_minor;
	int32		pattern_strlen;	/* used to compute offset to compiled pattern */
	char		data[FLEXIBLE_ARRAY_MEMBER];  /* original pattern string
											   * (null-terminated), followed by
											   * compiled pattern */
} pgpcre;


#define DatumGetPcreP(X)         ((pgpcre *) PG_DETOAST_DATUM(X))
#define DatumGetPcrePCopy(X)     ((pgpcre *) PG_DETOAST_DATUM_COPY(X))
#define PcrePGetDatum(X)         PointerGetDatum(X)
#define PG_GETARG_PCRE_P(n)      DatumGetPcreP(PG_GETARG_DATUM(n))
#define PG_GETARG_PCRE_P_COPY(n) DatumGetPcrePCopy(PG_GETARG_DATUM(n))
#define PG_RETURN_PCRE_P(x)      return PcrePGetDatum(x)



Datum
pcre_in(PG_FUNCTION_ARGS)
{
	char	   *input_string = PG_GETARG_CSTRING(0);
	pcre	   *pc;
	const char *err;
	int			erroffset;
	size_t		in_strlen;
	int			rc, total_len, pcsize;
	pgpcre	   *result;

	in_strlen = strlen(input_string);

	if (GetDatabaseEncoding() == PG_UTF8)
		pc = pcre_compile(input_string, PCRE_UTF8 | PCRE_UCP, &err, &erroffset, NULL);
	else if (GetDatabaseEncoding() == PG_SQL_ASCII)
		pc = pcre_compile(input_string, 0, &err, &erroffset, NULL);
	else
	{
		char *utf8string;

		utf8string = (char *) pg_do_encoding_conversion((unsigned char *) input_string,
														in_strlen,
														GetDatabaseEncoding(),
														PG_UTF8);
		pc = pcre_compile(utf8string, PCRE_UTF8 | PCRE_UCP, &err, &erroffset, NULL);
		if (utf8string != input_string)
			pfree(utf8string);
	}
	if (!pc)
		elog(ERROR, "PCRE compile error: %s", err);

	rc = pcre_fullinfo(pc, NULL, PCRE_INFO_SIZE, &pcsize);
	if (rc < 0)
		elog(ERROR, "pcre_fullinfo/PCRE_INFO_SIZE: %d", rc);

	total_len = offsetof(pgpcre, data) + in_strlen + 1 + pcsize;
	result = (pgpcre *) palloc0(total_len);
	SET_VARSIZE(result, total_len);
	result->pcre_major = PCRE_MAJOR;
	result->pcre_minor = PCRE_MINOR;
	result->pattern_strlen = in_strlen;
	strcpy(result->data, input_string);
	memcpy(result->data + in_strlen + 1, pc, pcsize);

	PG_RETURN_PCRE_P(result);
}


Datum
pcre_out(PG_FUNCTION_ARGS)
{
	pgpcre	   *p = PG_GETARG_PCRE_P(0);

	PG_RETURN_CSTRING(pstrdup(p->data));
}


static bool
matches_internal(text *subject, pgpcre *pattern, char ***return_matches, int *num_captured)
{
	pcre	   *pc;
	int			rc;
	int			num_substrings = 0;
	int		   *ovector;
	int			ovecsize;
	char	   *utf8string;
	static bool warned = false;

	if (!warned && (pattern->pcre_major != PCRE_MAJOR || pattern->pcre_minor != PCRE_MINOR))
	{
		ereport(WARNING,
				(errmsg("PCRE version mismatch"),
				 errdetail("The compiled pattern was created by PCRE version %d.%d, the current library is version %d.%d.  According to the PCRE documentation, \"compiling a regular expression with one version of PCRE for use with a different version is not guaranteed to work and may cause crashes.\"  This warning is shown only once per session.",
						   pattern->pcre_major, pattern->pcre_minor,
						   PCRE_MAJOR, PCRE_MINOR),
				 errhint("You might want to recompile the stored patterns by running something like UPDATE ... SET pcre_col = pcre_col::text::pcre.")));
		warned = true;
	}

	pc = (pcre *) (pattern->data + pattern->pattern_strlen + 1);

	if (num_captured)
	{
		int rc;

		if ((rc = pcre_fullinfo(pc, NULL, PCRE_INFO_CAPTURECOUNT, &num_substrings)) != 0)
			elog(ERROR, "pcre_fullinfo error: %d", rc);
	}

	if (return_matches)
	{
		ovecsize = (num_substrings + 1) * 3;
		ovector = palloc(ovecsize * sizeof(*ovector));
	}
	else
	{
		ovecsize = 0;
		ovector = NULL;
	}

	if (GetDatabaseEncoding() == PG_UTF8 || GetDatabaseEncoding() == PG_SQL_ASCII)
	{
		utf8string = VARDATA_ANY(subject);
		rc = pcre_exec(pc, NULL, VARDATA_ANY(subject), VARSIZE_ANY_EXHDR(subject), 0, 0, ovector, ovecsize);
	}
	else
	{
		utf8string = (char *) pg_do_encoding_conversion((unsigned char *) VARDATA_ANY(subject),
														VARSIZE_ANY_EXHDR(subject),
														GetDatabaseEncoding(),
														PG_UTF8);
		rc = pcre_exec(pc, NULL, utf8string, strlen(utf8string), 0, 0, ovector, ovecsize);
	}

	if (rc == PCRE_ERROR_NOMATCH)
		return false;
	else if (rc < 0)
		elog(ERROR, "PCRE exec error: %d", rc);

	if (return_matches)
	{
		char **matches;

		if (num_captured)
		{
			int i;

			*num_captured = num_substrings;
			matches = palloc(num_substrings * sizeof(*matches));

			for (i = 1; i <= num_substrings; i++)
			{
				if (ovector[i * 2] < 0)
					matches[i - 1] = NULL;
				else
				{
					const char *xmatch;

					pcre_get_substring(utf8string, ovector, rc, i, &xmatch);
					matches[i - 1] = (char *) xmatch;
				}
			}
		}
		else
		{
			const char *xmatch;

			matches = palloc(1 * sizeof(*matches));
			pcre_get_substring(utf8string, ovector, rc, 0, &xmatch);
			matches[0] = (char *) xmatch;
		}

		*return_matches = matches;
	}

	return true;
}


Datum
pcre_text_matches(PG_FUNCTION_ARGS)
{
	text	   *subject = PG_GETARG_TEXT_PP(0);
	pgpcre	   *pattern = PG_GETARG_PCRE_P(1);

	PG_RETURN_BOOL(matches_internal(subject, pattern, NULL, NULL));
}


Datum
pcre_matches_text(PG_FUNCTION_ARGS)
{
	pgpcre	   *pattern = PG_GETARG_PCRE_P(0);
	text	   *subject = PG_GETARG_TEXT_PP(1);

	PG_RETURN_BOOL(matches_internal(subject, pattern, NULL, NULL));
}


Datum
pcre_text_matches_not(PG_FUNCTION_ARGS)
{
	text	   *subject = PG_GETARG_TEXT_PP(0);
	pgpcre	   *pattern = PG_GETARG_PCRE_P(1);

	PG_RETURN_BOOL(!matches_internal(subject, pattern, NULL, NULL));
}


Datum
pcre_matches_text_not(PG_FUNCTION_ARGS)
{
	pgpcre	   *pattern = PG_GETARG_PCRE_P(0);
	text	   *subject = PG_GETARG_TEXT_PP(1);

	PG_RETURN_BOOL(!matches_internal(subject, pattern, NULL, NULL));
}


Datum
pcre_match(PG_FUNCTION_ARGS)
{
	pgpcre	   *pattern = PG_GETARG_PCRE_P(0);
	text	   *subject = PG_GETARG_TEXT_PP(1);
	char	  **matches;

	if (matches_internal(subject, pattern, &matches, NULL))
		PG_RETURN_TEXT_P(cstring_to_text(matches[0]));
	else
		PG_RETURN_NULL();
}


Datum
pcre_captured_substrings(PG_FUNCTION_ARGS)
{
	pgpcre	   *pattern = PG_GETARG_PCRE_P(0);
	text	   *subject = PG_GETARG_TEXT_PP(1);
	char	  **matches;
	int			num_captured;

	if (matches_internal(subject, pattern, &matches, &num_captured))
	{
		ArrayType  *result;
		int			dims[1];
		int			lbs[1];
		Datum	   *elems;
		bool	   *nulls;
		int i;

		dims[0] = num_captured;
		lbs[0] = 1;

		elems = palloc(num_captured * sizeof(*elems));
		nulls = palloc(num_captured * sizeof(*nulls));
		for (i = 0; i < num_captured; i++)
			if (matches[i])
			{
				elems[i] = PointerGetDatum(cstring_to_text(matches[i]));
				nulls[i] = false;
			}
			else
				nulls[i] = true;

		result = construct_md_array(elems, nulls, 1, dims, lbs, TEXTOID, -1, false, 'i');

		PG_RETURN_ARRAYTYPE_P(result);
	}
	else
		PG_RETURN_NULL();
}


static void *
pgpcre_malloc(size_t size)
{
	return palloc(size);
}


static void
pgpcre_free(void *ptr)
{
	pfree(ptr);
}


void
_PG_init(void)
{
	pcre_malloc = pgpcre_malloc;
	pcre_free = pgpcre_free;
}
