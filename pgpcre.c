#include <postgres.h>
#include <fmgr.h>
#include <mb/pg_wchar.h>
#include <utils/builtins.h>

#include <pcre.h>

PG_MODULE_MAGIC;

Datum pcre_in(PG_FUNCTION_ARGS);
Datum pcre_out(PG_FUNCTION_ARGS);
Datum pcre_text_eq(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(pcre_in);
PG_FUNCTION_INFO_V1(pcre_out);
PG_FUNCTION_INFO_V1(pcre_text_eq);


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


Datum
pcre_text_eq(PG_FUNCTION_ARGS)
{
	text	   *subject = PG_GETARG_TEXT_PP(0);
	pgpcre	   *pattern = PG_GETARG_PCRE_P(1);
	pcre	   *pc;
	int			rc;

	pc = (pcre *) (pattern->data + pattern->pattern_strlen + 1);

	if (GetDatabaseEncoding() == PG_UTF8 || GetDatabaseEncoding() == PG_SQL_ASCII)
		rc = pcre_exec(pc, NULL, VARDATA_ANY(subject), VARSIZE_ANY_EXHDR(subject), 0, 0, NULL, 0);
	else
	{
		char *utf8string;

		utf8string = (char *) pg_do_encoding_conversion((unsigned char *) VARDATA_ANY(subject),
														VARSIZE_ANY_EXHDR(subject),
														GetDatabaseEncoding(),
														PG_UTF8);
		rc = pcre_exec(pc, NULL, utf8string, strlen(utf8string), 0, 0, NULL, 0);
		if (utf8string != VARDATA_ANY(subject))
			pfree(utf8string);
	}

	if (rc == PCRE_ERROR_NOMATCH)
		PG_RETURN_BOOL(false);
	else if (rc < 0)
		elog(ERROR, "PCRE exec error: %d", rc);

	PG_RETURN_BOOL(true);
}
