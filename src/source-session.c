#include "source-session.h"
#include <string.h>

typedef struct source_attempt_error {
	mxrec_error error;
	uint64_t attempt;
	int status;
	bool partial;
	struct source_attempt_error *next;
} source_attempt_error;

void source_session_init(source_session *session, source *src, recomm_option opts)
{
	memset(session, 0, sizeof(*session));
	session->src = src;
	session->opts = opts;
	/* Each call below supplies its own error object. */
	session->opts.error = NULL;
}

static void save_error(source_session *session, const mxrec_error *error, int status, bool partial)
{
	if (!error->count)
		return;
	source_attempt_error *record = xcalloc(1, sizeof(*record));
	record->error = *error;
	record->attempt = session->attempts;
	record->status = status;
	record->partial = partial;
	if (session->errors_tail)
		session->errors_tail->next = record;
	else
		session->errors_head = record;
	session->errors_tail = record;
}

/* Returns 1 on success, 0 when the budget is exhausted, -1 for invalid data. */
static int fetch_with_failure_retry(source_session *session, size_t requested)
{
	while (session->attempts < session->src->max_try) {
		mxrec_error error = {0};
		recomm_option opts = session->opts;
		opts.error = &error;
		playlist batch = NULL;
		session->attempts++;
		int status = source_fetch_batch(session->src, requested, &batch, opts);
		size_t before = session->result.viewer.len;
		int appended = source_result_append_error(&session->result, batch, opts.strict, &error);
		bool partial = status < 0 && appended >= 0 && session->result.viewer.len > before;
		save_error(session, &error, appended < 0 ? -1 : status, partial);
		if (appended < 0) {
			session->fatal = true;
			return -1;
		}
		if (status >= 0) {
			session->succeeded = true;
			return 1;
		}
	}
	return 0;
}

bool source_session_can_refill(const source_session *session)
{
	return !session->fatal && session->src->refill_on_shortage && session->attempts < session->src->max_try;
}

int source_session_fill(source_session *session, size_t wanted)
{
	if (!session->src || !session->src->max_try || wanted > INT_MAX)
		return -1;
	if (session->fatal)
		return -1;
	if (!wanted || (!session->src->refill_on_shortage && session->succeeded))
		return 0;
	/* Shortage refill and failure retry share attempts; neither loop resets it. */
	while (session->result.viewer.len < wanted && session->attempts < session->src->max_try) {
		int fetched = fetch_with_failure_retry(session, wanted - session->result.viewer.len);
		if (fetched < 0)
			return -1;
		if (!fetched || !session->src->refill_on_shortage)
			break;
	}
	return 0;
}

void source_session_print_errors(source_session *session, FILE *out)
{
	while (session->errors_head) {
		source_attempt_error *record = session->errors_head;
		mxrec_error_print(out, &record->error, session->src->name, record->attempt, session->src->max_try,
				  record->status, record->partial);
		session->errors_head = record->next;
		xfree(record);
	}
	session->errors_tail = NULL;
}

void source_session_free(source_session *session)
{
	while (session->errors_head) {
		source_attempt_error *next = session->errors_head->next;
		xfree(session->errors_head);
		session->errors_head = next;
	}
	source_result_free(&session->result);
	memset(session, 0, sizeof(*session));
}
