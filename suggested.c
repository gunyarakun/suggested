#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <err.h>

#include <sys/queue.h>

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

sen_sym *tags;

void
root_handler(struct evhttp_request *req, void *arg)
{
  char *prefix;
  int sorted = 0;
  struct evbuffer *buf;
  if (!(buf = evbuffer_new())) {
    err(1, "failed to create response buffer");
  }

  /* get parameter */
  {
    struct evkeyval *kv;
    struct evkeyvalq *q;
    evhttp_parse_query(evhttp_request_uri(req), q);
    TAILQ_FOREACH(kv, q, next) {
      if (*kv->key == 'q') {
        prefix = kv->value;
      } else if (*kv->key == 's') {
        sorted = 1;
      }
    }
  }

  /* return tags */
  {
    char *tag;
    sen_set *s;
    sen_set_cursor *c;
    if (!(s = sen_sym_prefix_search(tags, prefix))) {
      err(2, "failed to sen_sym_prefix_search");
    }
    if (!(c = sen_set_cursor_open(s))) {
      err(2, "failed to sen_set_cursor_open");
    }
    while (sen_set_cursor_next(c, (void **)&tag, NULL)) {
      evbuffer_add(buf, tag, strlen(tag));
      evbuffer_add(buf, "\n", 1);
    }
    sen_set_cursor_close(c);
    sen_set_close(s);
  }
  evhttp_send_reply(req, HTTP_OK, "OK", buf);
}

void
generic_handler(struct evhttp_request *req, void *arg)
{
  struct evbuffer *buf;

  buf = evbuffer_new();
  if (buf == NULL)
          err(1, "failed to create response buffer");
  evbuffer_add_printf(buf, "Requested: %sn", evhttp_request_uri(req));
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
    evhttp_set_cb(httpd, "/", root_handler, NULL);
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
