#include <postgres.h>
#include <fmgr.h>
#include <mb/pg_wchar.h>
#include <utils/builtins.h>

#include <pcre.h>

PG_MODULE_MAGIC;

Datum pcre_text_eq(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(pcre_text_eq);


/*
 * RE caching taken from src/backend/utils/adt/regexp.c
 */

/* this is the maximum number of cached regular expressions */
#ifndef MAX_CACHED_RES
#define MAX_CACHED_RES	32
#endif

/* this structure describes one cached regular expression */
typedef struct cached_re_str
{
	char	   *cre_pat;		/* original RE (not null terminated!) */
	int			cre_pat_len;	/* length of original RE, in bytes */
	pcre	   *cre_re;			/* the compiled regular expression */
} cached_re_str;

static int	num_res = 0;		/* # of cached re's */
static cached_re_str re_array[MAX_CACHED_RES];	/* cached re's */


static pcre *
pcre_compile_and_cache(text *pattern)
{
	int			text_re_len = VARSIZE_ANY_EXHDR(pattern);
	char	   *text_re_val = VARDATA_ANY(pattern);
	pcre	   *pc;
	int			i;
	cached_re_str re_temp;
	const char *err;
	int erroffset;

	for (i = 0; i < num_res; i++)
	{
		if (re_array[i].cre_pat_len == text_re_len &&
			memcmp(re_array[i].cre_pat, text_re_val, text_re_len) == 0)
		{
			/* Found a match; move it to front if not there already. */
			if (i > 0)
			{
				re_temp = re_array[i];
				memmove(&re_array[1], &re_array[0], i * sizeof(cached_re_str));
				re_array[0] = re_temp;
			}

			return re_array[0].cre_re;
		}
	}

	if (GetDatabaseEncoding() == PG_UTF8)
		pc = pcre_compile(text_to_cstring(pattern), PCRE_UTF8 | PCRE_UCP, &err, &erroffset, NULL);
	else if (GetDatabaseEncoding() == PG_SQL_ASCII)
		pc = pcre_compile(text_to_cstring(pattern), 0, &err, &erroffset, NULL);
	else
	{
		char *utf8string;

		utf8string = (char *) pg_do_encoding_conversion((unsigned char *) text_re_val,
								text_re_len,
								GetDatabaseEncoding(),
								PG_UTF8);
		pc = pcre_compile(utf8string, PCRE_UTF8 | PCRE_UCP, &err, &erroffset, NULL);
		if (utf8string != text_re_val)
			pfree(utf8string);
	}
	if (!pc)
		elog(ERROR, "PCRE compile error: %s", err);

	re_temp.cre_re = pc;

	/*
	 * We use malloc/free for the cre_pat field because the storage has to
	 * persist across transactions, and because we want to get control back on
	 * out-of-memory.  The Max() is because some malloc implementations return
	 * NULL for malloc(0).
	 */
	re_temp.cre_pat = malloc(Max(text_re_len, 1));
	if (re_temp.cre_pat == NULL)
	{
		pcre_free(&re_temp.cre_re);
		ereport(ERROR,
				(errcode(ERRCODE_OUT_OF_MEMORY),
				 errmsg("out of memory")));
	}
	memcpy(re_temp.cre_pat, text_re_val, text_re_len);
	re_temp.cre_pat_len = text_re_len;

	/*
	 * Okay, we have a valid new item in re_temp; insert it into the storage
	 * array.  Discard last entry if needed.
	 */
	if (num_res >= MAX_CACHED_RES)
	{
		--num_res;
		Assert(num_res < MAX_CACHED_RES);
		pcre_free(re_array[num_res].cre_re);
		free(re_array[num_res].cre_pat);
	}

	if (num_res > 0)
		memmove(&re_array[1], &re_array[0], num_res * sizeof(cached_re_str));

	re_array[0] = re_temp;
	num_res++;

	return re_array[0].cre_re;
}


Datum
pcre_text_eq(PG_FUNCTION_ARGS)
{
	text *subject = PG_GETARG_TEXT_PP(0);
	text *pattern = PG_GETARG_TEXT_PP(1);
	pcre *pc;
	int rc;

	pc = pcre_compile_and_cache(pattern);

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
