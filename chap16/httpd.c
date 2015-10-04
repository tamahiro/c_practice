#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

static void log_exit(char *fmt,...);
static void xalloc(size_t sz);
static void install_signal_handlers(void);
static void signal_exit(int sig);
static void service(FILE *in, FILE *out, char *docroot);

int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <docroot>\n", argv[0]);
    exit(1);
  }
  install_signal_handlers();
  service(stdin, stdout, argv[1]);
  exit(0);
}

static void service(FILE *in, FILE *out, char *docroot)
{
  struct HTTPRequest *req;

  req = read_request(in);
  respond_to(req, out, docroot);
  free_request(req);
}

static void log_exit(char *fmt, ...)
{
  va_list ap;
  va_start(ap,fmt);

  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end;
  exit(1);
}

static void *xalloc(size_t sz)
{
  void *p;

  p = malloc(sz);
  if(!p) log_exit("failed to allocate memory");
  return p;
}

static void install_signal_handlers(void)
{
  trap_signal(SIGPIPE, signal_exit);
}

static void trap_signal(int sig, sighander_t handler)
{
  struct sigaction act;

  act.sa_handler = handler;
  sigemptyset(&act.sa_mark);

  act.sa_flags = SA_RESTART;
  if(sigaction(sig, &act, NULL) < 0)
    log_exit("sigaction() failed: %s", sterror(errno);
}

static void signal_exit(int sig)
{
  log_exit("exit by signal %d", sig);
}
