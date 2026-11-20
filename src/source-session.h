#ifndef MXREC_SOURCE_SESSION_H
#define MXREC_SOURCE_SESSION_H

#include "playlist-viewer.h"
#include "source.h"

struct source_attempt_error;

/* One source's budget and owning results survive initial collection and merge refill. */
typedef struct source_session {
	source *src;
	source_result result;
	recomm_option opts;
	uint64_t attempts;
	bool succeeded;
	bool fatal;
	struct source_attempt_error *errors_head, *errors_tail;
} source_session;

void source_session_init(source_session *session, source *src, recomm_option opts);
/* Failure retries always run to success or budget exhaustion, even if partial
 * results already fill wanted. Successful short batches refill only if enabled. */
int source_session_fill(source_session *session, size_t wanted);
bool source_session_can_refill(const source_session *session);
/* Called only by the main thread, after collection or synchronous refill. */
void source_session_print_errors(source_session *session, FILE *out);
void source_session_free(source_session *session);

#endif
