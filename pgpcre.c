#include <postgres.h>
#include <fmgr.h>
#include <utils/builtins.h>

#include <pcre.h>

PG_MODULE_MAGIC;

Datum pcre_text_eq(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(pcre_text_eq);

Datum
pcre_text_eq(PG_FUNCTION_ARGS)
{
	char *subject = text_to_cstring(PG_GETARG_TEXT_P(0));
	char *pattern = text_to_cstring(PG_GETARG_TEXT_P(1));
	pcre *pc;
	const char *err;
	int erroffset;
	int rc;

	pc = pcre_compile(pattern, 0, &err, &erroffset, NULL);
	if (!pc)
		elog(ERROR, "PCRE compile error: %s", err);

	rc = pcre_exec(pc, NULL, subject, strlen(subject), 0, 0, NULL, 0);
	pcre_free(pc);

	if (rc == PCRE_ERROR_NOMATCH)
		PG_RETURN_BOOL(false);
	else if (rc < 0)
		elog(ERROR, "PCRE exec error: %d", rc);

	PG_RETURN_BOOL(true);
}
