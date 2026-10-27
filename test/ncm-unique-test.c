/* Parse response-shaped JSON directly, without starting NCM or making requests. */
#include "dedup-helpers.h"
#include "source/ncm.c"

static const char fixture[] = "{\"success\":true,\"data\":{\"body\":{\"data\":{\"dailySongs\":["
			      "{\"name\":\"A\",\"al\":{\"name\":\"First\"},\"ar\":[{\"name\":\"Artist\"}]},"
			      "{\"name\":\"a\",\"al\":{\"name\":\"Compilation\"},\"ar\":[{\"name\":\"ARTIST\"}]},"
			      "{\"name\":\"B\",\"ar\":[{\"name\":\"Artist\"}]},"
			      "{\"name\":\"C\",\"ar\":[{\"name\":\"Artist\"}]},"
			      "{\"name\":\"D\",\"ar\":[{\"name\":\"Artist\"}]}"
			      "]}}}}";

static int fixture_recomm(source *s, size_t num, playlist *out, recomm_option opts)
{
	(void)num;
	struct json_err err = {0};
	curlbuf buf;
	curlbuf_init(&buf, sizeof(fixture));
	curlbuf_append(&buf, fixture, strlen(fixture));
	int ret = ncm_curlbuf2playlist(s, &buf, out, &err, opts.progress_bar);
	curlbuf_free(&buf);
	return ret;
}

int main(void)
{
	source s = {.rmp = fixture_recomm};
	playlist out = NULL;
	CHECK(recomm_multi(&s, 3, &out, .strict = true) == 3);
	test_unique(out);
	CHECK(strcmp((const char *)out[0].tr->title, "A") == 0);
	CHECK(strcmp((const char *)out[0].tr->album, "First") == 0);
	CHECK(strcmp((const char *)out[1].tr->title, "B") == 0);
	CHECK(strcmp((const char *)out[2].tr->title, "C") == 0);
	playlist_free(out);
	CHECK(recomm_multi(&s, 10, &out, .strict = true) == 4);
	test_unique(out);
	playlist_free(out);

	struct json_err err = {0};
	curlbuf buf;
	const char empty[] = "{\"success\":true,\"data\":{\"body\":{\"data\":{\"dailySongs\":[]}}}}";
	curlbuf_init(&buf, sizeof(empty));
	curlbuf_append(&buf, empty, strlen(empty));
	CHECK(ncm_curlbuf2playlist(&s, &buf, &out, &err, false) == 0);
	playlist_free(out);
	curlbuf_free(&buf);
	puts("ncm-unique-test passed");
	return 0;
}
