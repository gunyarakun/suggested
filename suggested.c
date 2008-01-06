#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <err.h>

#include <sys/queue.h>
#ifndef TAILQ_FOREACH
#define TAILQ_FOREACH(var, head, field) \
  for ((var) = ((head)->tqh_first); \
    (var); \
    (var) = ((var)->field.tqe_next))
#endif /* LIST_FOREACH */
#include <event.h>
#include <evhttp.h>

#include <senna/senna.h>
/* copy from senna/str.h */
typedef struct {
  const char *orig;
  size_t orig_blen;
  char *norm;
  size_t norm_blen;
  uint_least8_t *ctypes;
  int16_t *checks;
  size_t length;
  int flags;
  sen_ctx *ctx;
  /* sen_encoding encoding; */
} sen_nstr;
sen_nstr *sen_nstr_open(const char *str, size_t str_len, sen_encoding encoding, int flags);

#include <stdio.h>
#include <string.h>

#define PORT 8000 /* port number listened */

sen_sym *tags;
char gbuf[SEN_SYM_MAX_KEY_SIZE];

static char *
chomp(char *string)
{
  int l = strlen(string);
  if (l) {
    char *p = string + l - 1;
    if (*p == '\n') { *p = '\0'; }
  }
  return string;
}

static int
do_insert(const char *filename)
{
  if (!(tags = sen_sym_create(filename, 0, 0, sen_enc_utf8))) {
    fprintf(stderr, "sym create failed\n");
    return -1;
  }
  while (!feof(stdin)) {
    char *cstr;
    sen_nstr *s;
    if (!fgets(gbuf, SEN_SYM_MAX_KEY_SIZE, stdin)) { break; }
    cstr = chomp(gbuf);
    s = sen_nstr_open(cstr, strlen(cstr), sen_enc_utf8, 0);
    sen_sym_get(tags, s->norm);
  }
  return 0;
}

void
generic_handler(struct evhttp_request *req, void *arg)
{
  const char *s;
  sen_nstr *prefix;
  struct evbuffer *buf;
  if (!(buf = evbuffer_new())) {
    err(1, "failed to create response buffer");
  }
  evhttp_add_header(req->output_headers, "Content-Type",
                    "text/plain; charset=UTF-8");

  /* get parameter */
  s = evhttp_decode_uri(evhttp_request_uri(req)) + 1;
  prefix = sen_nstr_open(s, strlen(s), sen_enc_utf8, 0);

  /* return tags */
  {
    sen_set *s;
    sen_id *tid;
    sen_set_cursor *c;
    if (!(s = sen_sym_prefix_search(tags, prefix->norm))) {
      /* no entry found */
      evbuffer_add(buf, "0\n", 2);
      evhttp_send_reply(req, HTTP_OK, "OK", buf);
      return;
      /* err(1, "failed to sen_sym_prefix_search"); */
    }
    if (!(c = sen_set_cursor_open(s))) {
      err(1, "failed to sen_set_cursor_open");
    }
    {
      unsigned int nent;
      sen_set_info(s, NULL, NULL, &nent);
      evbuffer_add_printf(buf, "%u\n", nent);
    }
    while (sen_set_cursor_next(c, (void **)&tid, NULL)) {
      int tag_len = sen_sym_key(tags, *tid, gbuf, SEN_SYM_MAX_KEY_SIZE);
      evbuffer_add(buf, gbuf, tag_len - 1);
      evbuffer_add(buf, "\n", 1);
    }
    sen_set_cursor_close(c);
    sen_set_close(s);
  }
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

int
main(int argc, char **argv)
{
  struct evhttp *httpd;

  if (argc < 2) {
    puts("usage: suggested tag-dic");
    return 1;
  }

  sen_init();
  if (!(tags = sen_sym_open(argv[1]))) {
    fprintf(stderr, "create dictionary...\n");
    do_insert(argv[1]);
    fprintf(stderr, "dictionary created !!\n");
  }
  event_init();
  if (httpd = evhttp_start("0.0.0.0", PORT)) {
    evhttp_set_gencb(httpd, generic_handler, NULL);

    event_dispatch();

    evhttp_free(httpd);
  } else {
    fprintf(stderr, "cannot bind port %d", PORT);
  }
  sen_sym_close(tags);

  sen_fin();
  return 0;
}
