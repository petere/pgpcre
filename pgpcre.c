#include <postgres.h>
#include <catalog/pg_type.h>
#include <fmgr.h>
#include <mb/pg_wchar.h>
#include <utils/array.h>
#include <utils/builtins.h>

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

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
	pcre2_code *pc;
	int         err;
	PCRE2_SIZE		erroffset;
	size_t		in_strlen, pcsize;
	int			rc, total_len;
	pgpcre	   *result;

	in_strlen = strlen(input_string);

	if (GetDatabaseEncoding() == PG_UTF8)
		pc = pcre2_compile((PCRE2_SPTR) input_string, in_strlen, PCRE2_UTF | PCRE2_UCP, &err, &erroffset, NULL);
	else if (GetDatabaseEncoding() == PG_SQL_ASCII)
		pc = pcre2_compile((PCRE2_SPTR) input_string, in_strlen, 0, &err, &erroffset, NULL);
	else
	{
		char *utf8string;

		utf8string = (char *) pg_do_encoding_conversion((unsigned char *) input_string,
														in_strlen,
														GetDatabaseEncoding(),
														PG_UTF8);
		pc = pcre2_compile((PCRE2_SPTR) utf8string, strlen(utf8string), PCRE2_UTF | PCRE2_UCP, &err, &erroffset, NULL);
		if (utf8string != input_string)
			pfree(utf8string);
	}
	if (!pc)
	{
		PCRE2_UCHAR buf[120];

		pcre2_get_error_message(err, buf, sizeof(buf));
		elog(ERROR, "PCRE compile error: %s", buf);
        }

	rc = pcre2_pattern_info(pc, PCRE2_INFO_SIZE, &pcsize);
	if (rc < 0)
		elog(ERROR, "pcre2_pattern_info/PCRE2_INFO_SIZE: %d", rc);

	total_len = offsetof(pgpcre, data) + in_strlen + 1 + pcsize;
	result = (pgpcre *) palloc0(total_len);
	SET_VARSIZE(result, total_len);
	result->pcre_major = PCRE2_MAJOR;
	result->pcre_minor = PCRE2_MINOR;
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
	pcre2_code	   *pc;
	pcre2_match_data   *md;
	int			rc;
	uint32_t		num_substrings = 0;
	PCRE2_SIZE	   *ovector;
	int			ovecsize;
	char	   *utf8string;
	static bool warned = false;

	if (!warned && (pattern->pcre_major != PCRE2_MAJOR || pattern->pcre_minor != PCRE2_MINOR))
	{
		ereport(WARNING,
				(errmsg("PCRE version mismatch"),
				 errdetail("The compiled pattern was created by PCRE version %d.%d, the current library is version %d.%d.  According to the PCRE documentation, \"compiling a regular expression with one version of PCRE for use with a different version is not guaranteed to work and may cause crashes.\"  This warning is shown only once per session.",
						   pattern->pcre_major, pattern->pcre_minor,
						   PCRE2_MAJOR, PCRE2_MINOR),
				 errhint("You might want to recompile the stored patterns by running something like UPDATE ... SET pcre_col = pcre_col::text::pcre.")));
		warned = true;
	}

	pc = (pcre2_code *) (pattern->data + pattern->pattern_strlen + 1);

	if (num_captured)
	{
		if ((rc = pcre2_pattern_info(pc, PCRE2_INFO_CAPTURECOUNT, &num_substrings)) != 0)
			elog(ERROR, "pcre2_pattern_info error: %d", rc);
	}

	if (return_matches)
	{
		ovecsize = (num_substrings + 1) * 3;
		md = pcre2_match_data_create(ovecsize, NULL);
	}
	else
	{
		md = pcre2_match_data_create_from_pattern(pc, NULL);
	}

	if (GetDatabaseEncoding() == PG_UTF8 || GetDatabaseEncoding() == PG_SQL_ASCII)
	{
		utf8string = VARDATA_ANY(subject);
		rc = pcre2_match(pc, (PCRE2_SPTR) VARDATA_ANY(subject), VARSIZE_ANY_EXHDR(subject), 0, 0, md, NULL);
	}
	else
	{
		utf8string = (char *) pg_do_encoding_conversion((unsigned char *) VARDATA_ANY(subject),
														VARSIZE_ANY_EXHDR(subject),
														GetDatabaseEncoding(),
														PG_UTF8);
		rc = pcre2_match(pc, (PCRE2_SPTR) utf8string, strlen(utf8string), 0, 0, md, NULL);
	}

	if (rc == PCRE2_ERROR_NOMATCH)
	{
		pcre2_match_data_free(md);
		return false;
	}
	else if (rc < 0)
		elog(ERROR, "PCRE match error: %d", rc);

	if (return_matches)
	{
		char **matches;

		if (num_captured)
		{
			int i;

			*num_captured = num_substrings;
			matches = palloc(num_substrings * sizeof(*matches));
			ovector = pcre2_get_ovector_pointer(md);

			for (i = 1; i <= num_substrings; i++)
			{
				if ((int) ovector[i * 2] < 0)
					matches[i - 1] = NULL;
				else
				{
					PCRE2_UCHAR *xmatch;
					PCRE2_SIZE l;

					pcre2_substring_get_bynumber(md, i, &xmatch, &l);
					matches[i - 1] = (char *) xmatch;
				}
			}
		}
		else
		{
			PCRE2_UCHAR *xmatch;
			PCRE2_SIZE l;

			matches = palloc(1 * sizeof(*matches));
			pcre2_substring_get_bynumber(md, 0, &xmatch, &l);
			matches[0] = (char *) xmatch;
		}

		*return_matches = matches;
	}

	pcre2_match_data_free(md);

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


void
_PG_init(void)
{
}
