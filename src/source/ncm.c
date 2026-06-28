#include "ncm.h"
#include "artist.h"
#include "assert.h"
#include "bb.h"
#include "comm.h"
#include "config.h"
#include "da.h"
#include "json.h"
#include "monotonic.h"
#include "playlist.h"
#include "source.h"
#include "source/comm-source.h"
#include "track.h"
#include "u8string.h"
#include "utils/file.h"
#include "utils/image.h"
#include "utils/string.h"
#include "xmalloc.h"
#include "yyjson/src/yyjson.h"
#include <curl/curl.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define NCM_MODULE_ENTRYPOINT "server.js"

#define NCM_DECL static

// curlbuf
#define CURLBUF_DEFAULT_CAP (1024)
BUFFERBUILDER_INIT(NCM_DECL, curlbuf, curlbuf, void);

NCM_DECL
size_t ncm_curl_write_callback(char *ptr, const size_t size, const size_t nmemb, curlbuf *cb)
{
	const size_t real = size * nmemb;
	curlbuf_append(cb, ptr, real);
	return real;
}

typedef enum ncm_address_type {
	NCM_NONE_ADDRESS,
	NCM_HTTP_REQUEST,
	NCM_LOCAL_SOCKET,
} ncm_address_type;

NCM_DECL
ncm_address_type str2addr_type(const char *str)
{
	const char *err_strategy;
	if (str == NULL)
		mxrec_cleanup(err, err_strategy, "NULL");
	if (strcmp(str, "http") == 0)
		return NCM_HTTP_REQUEST;
	else if (strcmp(str, "socket") == 0)
		return NCM_LOCAL_SOCKET;
	else
		mxrec_cleanup(err, err_strategy, str);
err:
	error("ncm failed to convert %s into diffusion_strategy", err_strategy);
	return NCM_NONE_ADDRESS;
}

#define NCM_HTTP_REQUEST_STR "http"
#define NCM_LOCAL_SOCKET_STR "socket"

typedef struct ncm_module {
	pid_t pid;
	// TODO
	char *status;
} ncm_module;

NCM_DECL
bool ncm_module_is_running(ncm_module *M) { return M->pid != -1; }

NCM_DECL
int ncm_wait_ready(pid_t pid, int fd, int timeout)
{
	char buf[1024];
	struct timeval tv = {.tv_sec = timeout, .tv_usec = 0};
	fd_set fds;

	while (1) {
		FD_ZERO(&fds);
		FD_SET(fd, &fds);

		int ret = select(fd + 1, &fds, NULL, NULL, &tv);
		if (ret <= 0) {
			error("timeout waiting for ncm module to start");
			return -1;
		}

		ssize_t n = read(fd, buf, sizeof(buf) - 1);
		if (n <= 0) {
			error("pipe closed before ncm module started");
			return -1;
		}
		buf[n] = '\0';

		if (strstr(buf, "listening") != NULL) {
			return 0;
		}
	}
}

NCM_DECL
int ncm_module_init(ncm_module *M, ncm_address_type type, const char *address, int port, const char *work_dir)
{
	int pipefd[2];
	M->pid = -1;
	M->status = NULL;

	char port_str[LONG_STR_SIZE];
	ull2string(port_str, LONG_STR_SIZE, port);

	if (pipe(pipefd) < 0)
		return -1;

	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}

	if (pid == 0) {
		prctl(PR_SET_PDEATHSIG, SIGTERM);
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);

		chdir(work_dir);

		if (type == NCM_LOCAL_SOCKET) {
			execlp("node", "node", "--optimize-for-size", "--max-old-space-size=64",
			       "--max-semi-space-size=1", NCM_MODULE_ENTRYPOINT, "--mode", NCM_LOCAL_SOCKET_STR,
			       "--address", address, NULL);
		} else {
			execlp("node", "node", "--optimize-for-size", "--max-old-space-size=64",
			       "--max-semi-space-size=1", NCM_MODULE_ENTRYPOINT, "--mode", NCM_HTTP_REQUEST_STR,
			       "--address", address, "--port", port_str, NULL);
		}
		_exit(127);
	}

	close(pipefd[1]);
	M->pid = pid;
	if (ncm_wait_ready(pid, pipefd[0], 5) < 0)
		goto fail;

	close(pipefd[0]);
	return 0;
fail:
	close(pipefd[0]);
	kill(pid, SIGTERM);
	waitpid(pid, NULL, 0);
	M->pid = -1;
	return -1;
}

NCM_DECL
void ncm_module_free(ncm_module *M)
{
	if (M == NULL)
		return;
	if (M->pid > 0) {
		kill(M->pid, SIGTERM);
		waitpid(M->pid, NULL, 0);
	}
}

typedef struct ncm_security {

} ncm_security;

static ncm_security __security;

#define FUNCTION_FIELD(retype, name, ...) NCM_DECL retype ncm_##name(__VA_ARGS__);

FUNCTION_FIELD_LIST

#undef FUNCTION_FIELD

static source __ncm_source = {
	.name = "ncm",
	.destroy = ncm_source_destroy,
	.rsp = ncm_recomm_single,
	.rmp = ncm_recomm_multi,
	.sh = ncm_security_handle,
	.security = &__security,
	.cc = ncm_config_check,
};

struct ncm_source {
	source src;
	int port;
	u8s username;
	char *address;
	ncm_address_type addr_type;
	char *cookie;
	ncm_module M;

	CURL *curl;
};

NCM_DECL
int ncm_source_init(void *sp, config_t *cfg)
{
	globalCurlInit();
	ncm_source *s = (ncm_source *)sp;
	s->src = __ncm_source;

	ncm_address_type type = str2addr_type(cfg->ncm_bind_method);
	const char *address = cfg->ncm_bind_address;
	if (address == NULL) {
		address = type == NCM_HTTP_REQUEST ? NCM_DEFAULT_HTTP_ADDRESS : NCM_DEFAULT_SOCKET_ADDRESS;
	}
	int port = (int)cfg->ncm_port;
	if (ncm_module_init(&s->M, type, address, port, cfg->ncm_work_dir) < 0)
		return -1;

	s->curl = curl_easy_init();
	if (s->curl == NULL)
		return -1;

	s->username = u8sdup(cfg->ncm_username);
	s->port = port;
	s->address = xstrdup(address);
	s->addr_type = type;
	s->cookie = slurp(cfg->ncm_cookie_file);
	return source_check(s) ? 0 : -1;
}

NCM_DECL
inline void ncm_source_destroy(void *sp)
{
	ncm_source *s = (ncm_source *)sp;
	u8sfree(s->username);
	xfree(s->address);
	xfree(s->cookie);
	curl_easy_cleanup(s->curl);
	ncm_module_free(&s->M);
}

// ncm module routing
#define NCM_MODULE_ENTRY_LIST                                                                                          \
	NCM_MODULE_ENTRY(LOGIN_QR_KEY, "/login/qr/key", 0)                                                             \
	NCM_MODULE_ENTRY(LOGIN_QR_CREATE, "/login/qr/create", 1)                                                       \
	NCM_MODULE_ENTRY(LOGIN_QR_CHECK, "/login/qr/check", 1)                                                         \
	NCM_MODULE_ENTRY(LOGIN_STATUS, "/login/status", 1)                                                             \
	NCM_MODULE_ENTRY(RECOMM_DAILY_SONGS, "/recommend/daily/songs", 2)

typedef struct ncm_module_entry {
	const char *path;
	unsigned param_size;
} ncm_module_entry;

#define NCM_MODULE_ENTRY(name, _path, _count) NCM_DECL ncm_module_entry name = {.path = _path, .param_size = _count};

NCM_MODULE_ENTRY_LIST

NCM_DECL
char *ncm_get_url(ncm_source *s, ncm_address_type type, const ncm_module_entry entry)
{
	if (type == NCM_HTTP_REQUEST) {
		return parseFormat("http://%s:%d%s", s->address, s->port, entry.path);
	} else if (type == NCM_LOCAL_SOCKET) {
		return parseFormat("http://localhost%s", entry.path);
	}
	return NULL;
}

// ncm ipc json parser
#define JSON_ERR_HEAD "NCM"
typedef struct ncm_json_msg {
	yyjson_doc *doc;
	bool success;
	int code;
	union {
		char *err_msg;
		yyjson_val *data;
	};
} ncm_json_msg;

NCM_DECL
void ncm_json_msg_free(ncm_json_msg *msg)
{
	if (msg == NULL)
		return;
	yyjson_doc_free(msg->doc);
	if (!msg->success && !msg->code) {
		xfree(msg->err_msg);
		return;
	}
}

NCM_DECL
int ncm_curlbuf2json_msg(ncm_json_msg *msg, curlbuf *buf, struct json_err *jerr)
{
	yyjson_doc *doc;
	yyjson_read_err err;
	yyjson_val *root;
	bool success;
	int code;

	doc = yyjson_read_opts(buf->buf, buf->len, 0, 0, &err);
	if (doc == NULL) {
		write_jsonerr(jerr, "[%s]: failed to parse json: %s, code: %u at byte position: %lu\n", JSON_ERR_HEAD,
			      err.msg, err.code, err.pos);
		msg->doc = NULL;
		return -1;
	}
	msg->doc = doc;
	root = yyjson_doc_get_root(doc);

	success = yyjson_get_bool(yyjson_obj_get(root, "success"));
	code = yyjson_get_int(yyjson_obj_get(root, "code"));

	msg->success = success;
	msg->code = code;

	if (!success && !code) {
		msg->err_msg = xstrdup(yyjson_get_str(yyjson_obj_get(root, "msg")));
	} else {
		msg->data = yyjson_obj_get(root, "data");
	}
	return 0;
}

NCM_DECL
char *ncm_curlbuf2key(curlbuf *buf, struct json_err *jerr)
{
	yyjson_val *val;
	char *key;
	ncm_json_msg msg = {0};
	if (ncm_curlbuf2json_msg(&msg, buf, jerr) < 0)
		mxrec_cleanup(cleanup, key, 0);

	if (!msg.success && !msg.code) {
		write_jsonerr(jerr, "failed to create qr key:%s", msg.err_msg);
		mxrec_cleanup(cleanup, key, 0);
	}

	// data.body.data.unikey
	val = YYJSON_GET(msg.data, "body", "data", "unikey");
	key = xstrdup(yyjson_get_str(val));

cleanup:
	ncm_json_msg_free(&msg);
	return key;
}

NCM_DECL
char *ncm_curlbuf2qr_create_url(curlbuf *buf, struct json_err *jerr)
{
	yyjson_val *val;
	char *url;
	ncm_json_msg msg = {0};
	if (ncm_curlbuf2json_msg(&msg, buf, jerr) < 0)
		mxrec_cleanup(cleanup, url, 0);

	if (!msg.success && !msg.code) {
		write_jsonerr(jerr, "failed to create url of qrcode:%s", msg.err_msg);
		mxrec_cleanup(cleanup, url, 0);
	}

	// data.body.data.qrurl
	val = YYJSON_GET(msg.data, "body", "data", "qrurl");
	url = xstrdup(yyjson_get_str(val));
cleanup:
	ncm_json_msg_free(&msg);
	return url;
}

NCM_DECL
artist *ncm_json2artist(yyjson_val *val, struct json_err *jerr)
{
	artist *ar;
	size_t i, alia_size;
	const char *name, **als;
	yyjson_val *alias, *alia;

	name = yyjson_get_str(YYJSON_GET(val, "name"));
	alias = YYJSON_GET(val, "alias");
	alia_size = yyjson_arr_size(alias);
	if (alia_size == 0) {
		als = NULL;
	} else {
		als = xmalloc(alia_size * sizeof(*als));
		yyjson_arr_foreach(alias, i, alia_size, alia) { als[i] = yyjson_get_str(alia); }
	}

	ar = artist_new(name, alia_size, als);
	xfree(als);
	return ar;
}

NCM_DECL
void ncm_json2playitem(yyjson_val *val, playitem *pi, struct json_err *jerr)
{
	track *tr;
	const char *name, *album;
	const char **als;
	artist **ars;
	size_t i, ar_size, alia_size;
	yyjson_val *artists, *artist, *alias, *alia;
	playitem_init(pi);

	name = yyjson_get_str(YYJSON_GET(val, "name"));
	album = yyjson_get_str(YYJSON_GET(val, "al", "name"));

	artists = YYJSON_GET(val, "ar");
	ar_size = yyjson_arr_size(artists);
	ars = xmalloc(sizeof(*ars) * ar_size);

	yyjson_arr_foreach(artists, i, ar_size, artist) { ars[i] = ncm_json2artist(artist, jerr); }

	alias = YYJSON_GET(val, "alia");
	alia_size = yyjson_arr_size(alias);
	if (alia_size == 0) {
		als = NULL;
	} else {
		als = xmalloc(alia_size * sizeof(*als));
		yyjson_arr_foreach(alias, i, alia_size, alia) { als[i] = yyjson_get_str(alia); }
	}
	tr = track_new(name, album, ar_size, ars, alia_size, als);
	pi->tr = tr;
	xfree(als);
	// NO urls
}

NCM_DECL
int ncm_curlbuf2playlist(source *s, curlbuf *buf, size_t num, playlist *pl_ref, struct json_err *jerr, bool update)
{
	int ret;
	ncm_json_msg msg = {0};
	playlist p = NULL;
	size_t i, len;
	yyjson_arr_iter iter;
	yyjson_val *songs, *song;

	if ((ret = ncm_curlbuf2json_msg(&msg, buf, jerr)) < 0) {
		mxrec_cleanup(cleanup, ret, ret);
	}

	if (!msg.success && !msg.code) {
		write_jsonerr(jerr, "%s", msg.err_msg);
		mxrec_cleanup(cleanup, ret, -1);
	}

	// data.body.data.dailySongs
	songs = YYJSON_GET(msg.data, "body", "data", "dailySongs");
	if (!yyjson_is_arr(songs)) {
		mxrec_cleanup(cleanup, ret, -1);
	}

	len = yyjson_arr_size(songs);
	len = len <= num ? len : num;
	da_init2(p, sizeof(playitem), len);
	iter = yyjson_arr_iter_with(songs);
	i = 0;
	while ((song = yyjson_arr_iter_next(&iter))) {
		if (i >= len) {
			break;
		}
		ncm_json2playitem(song, &p[i], jerr);
		if (update && s->ur) {
			s->ur(s->update_entry, i, len, "collecting tracks");
		}
		++i;
	}
	if (update && s->ur) {
		s->ur(s->update_entry, i, len, "collecting tracks");
	}
	assert(i == len);
	DAHDR(p)->len = i;
	if (update && s->uc)
		s->uc(s->update_entry);
	ret = len;

cleanup:
	*pl_ref = p;
	ncm_json_msg_free(&msg);
	return ret;
}

NCM_DECL int ncm_xfer_cb(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	source *s = userdata;
	size_t done, total;
	(void)ultotal;
	(void)ulnow;
	done = (size_t)dlnow;
	total = dltotal > 0 ? (size_t)dltotal : 100;
	if (s->ur)
		s->ur(s->update_entry, done, total, "fetching data");
	return 0;
}

// ncm curl
NCM_DECL
int ncm_curl(source *s, curlbuf *buf, bool update, const ncm_module_entry entry, ...)
{
	int ret;
	size_t i, para_str_len;
	CURLcode code;
	struct curl_slist *headers = NULL;
	long http_code = 0;
	char *url, *para_str = NULL;

	// json
	va_list paras;
	yyjson_mut_doc *doc = NULL;
	yyjson_mut_val *root;
	ncm_source *ns = (ncm_source *)s;

	CURL *curl = ns->curl;
	ncm_address_type type = ns->addr_type;

	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, s->timeout);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, ncm_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buf);

	url = ncm_get_url(ns, type, entry);
	if (url == NULL)
		mxrec_cleanup(cleanup, ret, -1);

	// all routes are post methods
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	if (type == NCM_LOCAL_SOCKET) {
		curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, ns->address);
	}

	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	doc = yyjson_mut_doc_new(NULL);
	if (doc == NULL)
		mxrec_cleanup(cleanup, ret, -1);
	root = yyjson_mut_obj(doc);
	if (root == NULL)
		mxrec_cleanup(cleanup, ret, -1);
	yyjson_mut_doc_set_root(doc, root);

	va_start(paras, entry);
	for (i = 0; i < entry.param_size; ++i) {
		kv_t pair = va_arg(paras, kv_t);
		yyjson_mut_obj_add_str(doc, root, pair.key, pair.val);
	}
	va_end(paras);
	para_str = yyjson_mut_write(doc, 0, &para_str_len);
	if (para_str == NULL) {
		error("failed to write json in ncm curl");
		mxrec_cleanup(cleanup, ret, -1);
	}

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, para_str);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, para_str_len);

	if (update && s->ur) {
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ncm_xfer_cb);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, s);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	}

	code = curl_easy_perform(curl);
	if (update && s->uc) {
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, NULL);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, NULL);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
		s->uc(s->update_entry);
	}

	if (code != CURLE_OK) {
		error("ncm curl failed: %s", curl_easy_strerror(code));
		mxrec_cleanup(cleanup, ret, -1);
	}

	curl_easy_getinfo(curl, CURLINFO_HTTP_CODE, &http_code);
	if (http_code != 200) {
		mxrec_cleanup(cleanup, ret, -1);
	}

	ret = 0;
cleanup:
	if (headers)
		curl_slist_free_all(headers);
	xfree(url);
	yyjson_mut_doc_free(doc);
	xfree(para_str);
	return ret;
}

NCM_DECL
char *ncm_bool2str(bool val)
{
	// true or false
	char *str = xmalloc(6);
	memset(str, 0, 6);
	if (val)
		strcpy(str, "true");
	else
		strcpy(str, "false");
	return str;
}

NCM_DECL int ncm_recomm_single(source *s, playitem *p, recomm_option opts)
{
	// TODO
	return -1;
}

NCM_DECL
int ncm_recomm_multi(source *s, size_t num, playlist *p, recomm_option opts)
{
	int ret;
	ncm_source *ns = (ncm_source *)s;
	curlbuf buf;
	struct json_err jerr;
	jerr.length = 0;
	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	char *fresh_str = NULL;

	fresh_str = ncm_bool2str(opts.ncm_opts.daily_recomm_fresh);
	if ((ret = ncm_curl(s, &buf, opts.progress_bar, RECOMM_DAILY_SONGS, MAKE_KV("cookie", ns->cookie),
			    MAKE_KV("afresh", fresh_str))) < 0) {
		mxrec_cleanup(cleanup, ret, ret);
	}

	if ((ret = ncm_curlbuf2playlist(s, &buf, num, p, &jerr, opts.progress_bar)) < 0) {
		error("failed to extract playlist from ncm json:%s", jerr.msg);
		mxrec_cleanup(cleanup, ret, ret);
	}

cleanup:
	xfree(fresh_str);
	curlbuf_free(&buf);
	return ret;
}

NCM_DECL
void ncm_security_handle(source *s) {}

NCM_DECL
bool ncm_check_username(ncm_source *s)
{
	int cmp, cmp_result;
	bool ret;
	curlbuf buf;
	ncm_json_msg msg = {0};
	yyjson_val *val;
	u8s username = NULL;

	if (!ncm_module_is_running(&s->M))
		return false;

	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	if (ncm_curl(&s->src, &buf, false, LOGIN_STATUS, MAKE_KV("cookie", s->cookie)) < 0) {
		mxrec_cleanup(cleanup, ret, false);
	}

	if (ncm_curlbuf2json_msg(&msg, &buf, NULL) < 0) {
		mxrec_cleanup(cleanup, ret, false);
	}

	if (!msg.success && !msg.code) {
		error("failed to check ncm username:%s", msg.err_msg);
		mxrec_cleanup(cleanup, ret, false);
	}

	// data.body.data.profile.nickname
	val = YYJSON_GET(msg.data, "body", "data", "profile", "nickname");
	username = u8snew(yyjson_get_str(val));

	cmp = u8scmp(username, s->username, &cmp_result);
	if (cmp_result != U8S_OK) {
		error("ncm failed to compare utf8 string %s:%s", username, s->username);
		ret = false;
	}
	ret = (cmp == 0);
cleanup:
	curlbuf_free(&buf);
	ncm_json_msg_free(&msg);
	u8sfree(username);
	return ret;
}

#define check(expr, val)                                                                                               \
	do {                                                                                                           \
		if (!(expr)) {                                                                                         \
			(val) = false;                                                                                 \
			error("ncm source init failed on %s", (#expr));                                                \
		}                                                                                                      \
	} while (0)

NCM_DECL
bool ncm_config_check(source *s)
{
	bool ret = true;
	ncm_source *ns = (ncm_source *)s;
	check(ns->username != NULL, ret);
	check(ns->address != NULL, ret);
	check(ns->addr_type != NCM_NONE_ADDRESS, ret);
	check(ns->cookie != NULL, ret);
	check(ncm_module_is_running(&ns->M), ret);
	ret = ncm_check_username(ns);
	if (!ret) {
		error("username in cookie don't match username in config");
	}
	return ret;
}

NCM_DECL
int ncm_qr_check(ncm_source *s, curlbuf *buf, struct json_err *jerr, const char *key, int timeout, int interval,
		 char **cookie_ref)
{
	ncm_json_msg msg = {0};
	int ret, elapsed_ms, timeout_ms;
	int code;
	elapsed_ms = 0;
	timeout_ms = timeout * 1000;
	char *cookie = NULL;

	assert(cookie_ref);

	while (elapsed_ms < timeout_ms) {
		if ((ret = ncm_curl(&s->src, buf, false, LOGIN_QR_CHECK, MAKE_KV("key", key))) < 0) {
			mxrec_cleanup(end, ret, -2);
		}
		if (ncm_curlbuf2json_msg(&msg, buf, jerr) < 0) {
			mxrec_cleanup(end, ret, -2);
		}
		curlbuf_clear(buf);
		if (!msg.success && !msg.code) {
			error("ncm failed to check qr code:%s", msg.err_msg);
			mxrec_cleanup(end, ret, -2);
		}

		// data.body.code
		code = yyjson_get_int(YYJSON_GET(msg.data, "body", "code"));
#if 0
		char *info = xstrdup(yyjson_get_str(YYJSON_GET(msg.data, "body", "message")));
		printf("Current status\n%d:%s\n", code, info);
		xfree(info);
#endif
		switch (code) {
		case 500: {
			write_jsonerr("failed to login:%s", "need \"nocookie\" parameter");
			mxrec_cleanup(end, ret, -2);
		}
		case 800: {
			write_jsonerr("failed to login:%s", "qrcode has been expired");
			mxrec_cleanup(end, ret, -1);
		}
		case 801:
		case 802:
			break;
		case 803: {
			// data.body.cookie
			cookie = xstrdup(yyjson_get_str(YYJSON_GET(msg.data, "body", "cookie")));
			mxrec_cleanup(end, ret, 0);
		}
		}

		struct timeval tv = {
			.tv_sec = interval / 1000,
			.tv_usec = (interval % 1000) * 1000,
		};

		select(0, 0, 0, 0, &tv);
		elapsed_ms += interval;
		ncm_json_msg_free(&msg);
	}
end:
	// timeout
	if (elapsed_ms >= timeout_ms)
		ret = -1;
	ncm_json_msg_free(&msg);
	*cookie_ref = cookie;
	return ret;
}

NCM_DECL
void ncm_get_cookie_by_qr(ncm_source *s)
{
	int ret;
	char *key = NULL, *url = NULL, *cookie = NULL;
	struct json_err jerr;
	jerr.length = 0;
	curlbuf buf;
	curlbuf_init(&buf, CURLBUF_DEFAULT_CAP);
	if ((ret = ncm_curl(&s->src, &buf, false, LOGIN_QR_KEY)) < 0) {
		mxrec_cleanup(cleanup, ret, ret);
	}

	key = ncm_curlbuf2key(&buf, &jerr);
	if (key == NULL)
		mxrec_cleanup(cleanup, ret, -1);
	curlbuf_clear(&buf);

	if ((ret = ncm_curl(&s->src, &buf, false, LOGIN_QR_CREATE, MAKE_KV("key", key))) < 0) {
		mxrec_cleanup(cleanup, ret, ret);
	}

	url = ncm_curlbuf2qr_create_url(&buf, &jerr);
	if (url == NULL)
		mxrec_cleanup(cleanup, ret, -1);
	curlbuf_clear(&buf);

	printf("[INFO]: Please scan the QRCode from Netease Music APP to get cookie\n");
	if (strwriteQR(url, NULL, 2, 0, 0, 1) < 0)
		mxrec_cleanup(cleanup, ret, -1);

	ret = ncm_qr_check(s, &buf, &jerr, key, 60 * 1000, 1000, &cookie);

	if (!ret && cookie)
		printf("[INFO]: cookie of NCM is:\n%s\n", cookie);
	else if (ret == -1)
		printf("[INFO]: NCM QR Code has been expired, please generate again.\n");
	else
		error("NCM failed to get cookie from QRCode");

cleanup:
	xfree(key);
	xfree(url);
	xfree(cookie);
	curlbuf_free(&buf);
}

// EXPOSE
int ncm_source_new(source **src, config_t *cfg)
{
	assert(cfg);
	*src = NULL;
	void *s = xmalloc(sizeof(struct ncm_source));
	source_init(s, cfg);
	if (ncm_source_init(s, cfg) < 0) {
		xfree(s);
		return -1;
	}
	*src = s;
	return 0;
}

void ncm_get_auth(source *s)
{
	if (s == NULL)
		return;
	ncm_get_cookie_by_qr((ncm_source *)s);
}
