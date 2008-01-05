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

#include <stdio.h>
#include <string.h>

typedef enum {
  send_rc_success = 0,
  send_memory_exhausted,
  send_invalid_argument
} send_rc;

#define PORT 8000 /* port number listened */
#define BUFSIZE 8192

sen_sym *tags;

void
generic_handler(struct evhttp_request *req, void *arg)
{
  const char *prefix;
  struct evbuffer *buf;
  if (!(buf = evbuffer_new())) {
    err(1, "failed to create response buffer");
  }

  /* get parameter */
  prefix = evhttp_decode_uri(evhttp_request_uri(req)) + 1;

  /* return tags */
  {
    char tag[BUFSIZE];
    sen_set *s;
    sen_id *tid;
    sen_set_cursor *c;
    if (!(s = sen_sym_prefix_search(tags, prefix))) {
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
      int tag_len = sen_sym_key(tags, *tid, tag, BUFSIZE);
      evbuffer_add(buf, tag, tag_len - 1);
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
    SEN_LOG(sen_log_alert, "sym open error!");
    return 2;
  }
  event_init();
  if (httpd = evhttp_start("0.0.0.0", PORT)) {
    evhttp_set_gencb(httpd, generic_handler, NULL);

    event_dispatch();

    evhttp_free(httpd);
  } else {
    puts("cannot bind");
  }
  sen_sym_close(tags);

  sen_fin();
  return 0;
}
