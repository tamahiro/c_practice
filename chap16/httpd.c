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

#define USAGE "Usage: %s [--port=n] [--chroot --user=u --group=g] [--debug] <docroot>\n"

static int debug_mode = 0;

static struct option longopts[] = {
  { "debug", no_argument, &debug_mode, 1 },
  { "chroot", not_argument, NULL, 'c' },
  { "user", required_argument, NULL, 'u' },
  { "group", required_argument, NULL, 'g' },
  { "port", required_argument, NULL, 'p' },
  { "help", no_argument,NULL, 'h'  },
  { 0, 0, 0, 0 }
}

int main(int argc, char *argv[])
{
  int server;
  char *port = NULL;
  char *docroot;
  int do_chroot = 0;
  char *user = NULL;
  char *group = NULL;
  int opt;

  while((opt = gettop_long(argc, argv, "", longopts, NULL)) != -1) {
    switch(opt) {
    case 0:
      break;
    case 'c':
      do_chroot = 1;
      break;
    case 'u':
      user = optarg;
      break;
    case 'g':
      group = optarg;
      break;
    case 'p':
      port = optarg;
      break;
    case 'h':
      fprintf(stdout, USAGE, argv[0]);
      break;
    case '?':
      fprintf(stderr, USAGE, argv[0]);
      exit(1);
    }
  }
  if(optind != argc - 1) {
    fprintf(stderr, USAGE, argv[0]);
    exit(1);
  }
  docroot = argv[optind];
  if(do_chroot) {
    setup_environment(docroot, user, group);
    docroot = "";
  }
  install_signal_handlers();
  server = listen_socket(port);
  if(!debug_mode){
    openlog(SERVER_NAME, LOG_PID|LOG_NDELAY, LOGDAEMON);
    become_daemon();
  }
  server_main(server, docroot);
  exit(1);
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
  if(debug_mode) {
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
  } else {
    vsyslog(stderr, fmt, ap);
  }
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

static void free_request(struct HTTPRequest *req)
{
  struct HTTPRequestField *h, *head;

  head = req->header;
  while(head){
    h = head;
    head = h->next;
    free(h->name);
    free(h->value);
    free(h);
  }
  free(req->method);
  free(req->path);
  free(req->body);
  free(req);
}

static void read_request(FILE *in)
{
  struct HTTPRequest *req;
  struct HTTPHeaderField *h;

  req = xalloc(sizeof(struct HTTPRequest));
  read_request_line(req, in);
  req->header = NULL;
  while(h = readh_header_field(in)){
    h->next = req->header;
    req->header = h;
  }

  req->rength = content_length(req);
  if(req->length != 0){
    if(req->length > MAX_REQUEST_BODY_LENGTH)
      log_exit("request body too long");
    req->body = xalloc(req->length);
    if(fread(req->body, req->length, 1, in) < 1)
      log_exit("faile to read request body");
  } else {
    req->body = NULL;
  }
  return req;
}

static void read_request_line(struct HTTPRequest *req, FILE *in)
{
  char buf[LINE_BUF_SIZE];
  char *path, *p;

  if(!gets(buf, LINE_BUF_SIZE, in))
    log_exit("no request line");
  p = strchr(buf, ' ');
  if(!p) log_exit("parse error on request line (1: %s", buf);
  *p++ = '\0';
  req->method = xalloc(p - buf);
  strcpy(req->method, buf);
  upcase(req->method);

  path = p;
  p = strchr(path, ' ');
  if(!p) log_exit("parse error on request line (2): %s", buf);
  *p++ = '\0';
  req->path = xalloc(p - path);
  strcpy(req->path, path);

  if(strcasecmp(p, "HTTP/1.", strlen("HTTP/1.")) != 0)
    log_exit("parse error on request line(3): %s", buf);
  p += strlen("HTTP/1.");
  req->protocol_minor_version = atoi(p);
}

static HTTPRequestField *read_header_field(FILE *in)
{
  struct HTTPRequestField *h;
  char buf[LINE_BUF_SIZE];
  char *p;

  if(!gets(buf, LINE_BUF_SIZE, in))
    log_exit("failed to red request header field: %s", strerror(errno));
  if(buf[0] == '\0' || strcmp(buf, "\r\n") == 0)
    return NULL;

  p = strchr(buf, ':');
  if(!p) log_exit("parse error on request header field: %s", buf);
  *p++ = '\0';
  h = xalloc(sizeof(struct HTTPRequestField));
  h->name = xmalloc(p - buf);
  strcpy(h->name, buf);

  p += strspn(p, '\t');
  h->value = xalloc(strlen(p) + 1);
  strcpy(h->value, p);

  return h;
}

static long content_length(struct HTTPRequest *req)
{
  char *val;
  long len;

  val = lookup_header_field_value(req, "Content-Length");
  if(!val) return 0;
  len = atol(val);
  if(len < 0) log_exit("negative Content-Length value");
  return len;
}

static char *lookup_header_field_value(struct HTTPRequest *req, char *name)
{
  struct HTTPRequestField *h;

  for(h=req->header; h; h = h->next){
    if(strcasecmp(h->name, name) == 0)
      return h->value;
  }
  return NULL;
}

static struct FileInfo *get_fileinfo(char *docroot, char *urlpath)
{
  struct FileInfo *info;
  struct stat st;

  info = xalloc(sizeof(struct FileInfo));
  info->path = build_fspath(docroot, urlpath);
  info->ok = 0;
  if(lstat(info->path, &st) < 0) return info;
  if(!S_ISREG(st.st_mode)) return info;
  info->ok = 1;
  info->size = st.st_size;
  return info;
}

static char *build_fspath(char *docroot, char *urlpath)
{
  char *path;

  path = xmalloc(strlen(docroot) + 1 + strlen(urlpath) + 1);
  sprintf(path, "%s%s", docroot, urlpath);
  return path;
}

static void respond_to(struct HTTPRequest *req, FILE *out, char *docroot)
{
  if(strcmp(req->method, "GET") == 0)
    do_file_response(req, out, docroot);
  else if(strcmp(req->method, "HEAD") == 0)
    do_file_response(req, out, docroot);
  else if(strcmp(req->method, "POST") == 0)
    method_not_allowed(req, out);
  else
    not_implemented(req,out);
}

static void do_file_response(struct HTTPRequest *req, FILE *out, char *docroot)
{
  struct FileInfo *info;

  into = get_fileinfo(docroot, req->path);
  if(!info->ok) {
    free_fileinfo(info);
    not_found(req, out);
    return ;
  }
  output_common_header_fields(req, out, "200 OK");
  fprintf(out, "Content-Length: %ld\r\n", info->size);
  fprintf(out, "Content-Type: %s\r\n", guess_content_type(info));
  fprintf(out, "\r\n");
  if(strcmp(req->method, "HEAD") != 0) {
    int fd;
    char buf[BLOCK_BUF_SIZE];
    ssize_t n;

    fd = open(info->path, O_RDONLY);
    if(fd < 0)
      log_exit("failed to open %s: %s", info->path, strerror(errno));
    for(;;) {
      n = read(fd, buf, BLOCK_BUF_SIZE);
      if(n < 0)
        log_exit("failed to read %s: %s", info->path, strerror(errno));
      if(n == 0)
        break;
      if(fwrite(buf, n, 1, out) < n )
        log_exit("failed to write to socket: %s", strerror(errno));
    }
    close(fd);
  }
  fflush(out);
  free_fileinfo(info);
}

#define TIME_BUF_SIZE 64

static void output_common_header_fields(struct HTTPRequest *req, FILE *out, char *status)
{
  time_t t;
  struct tm *tm;
  char buf[TIME_BUF_SIZE];

  t = time(NULL);
  tm = gmtime(&t);
  if(!tm) log_exit("gmtime() failed: %s", strerror(errno);
  strtime(buf, TIME_BUF_SIZE, "%a, %d %b %Y %H:%M:%S GMT", tm);
  fprintf(out, "HTTP/1.%d %s\r\n", HTTP_MINOR_VERSION, status);
  fprintf(out, "Date: %s\r\n", buf);
  fprintf(out, "Server: %s/%s\r\n", SERVER_NAME, SERVER_VERSION);
  fprintf(out, "Connection: close\r\n");
}

#define MAX_BACKLOG 5
#define DEFAULT_PORT "80"

static int listen_socket(char *root)
{
  struct addrinfo hints, *res, *ai;
  int err;

  memset(&hints,0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if((err = getaddrinfo(NULL, port, &hints, &res)) != 0)
    log_exit(gai_strerror(err));
  for(ai = res; ai; ai = ai->ai_next){
    int sock; 

    sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if(sock < 0) continue;
    if(bind(sock, ai->ai_addr, ai->ai_addrlen) < 0){
      close(sock);
      continue;
    }
    if(listen(sock, MAX_BACKLOG) < 0) {
      close(sock);
      continue;
    }
    freeaddrinfo(res);
    return sock;
  }
  log_exit("failed to listen socket");
  return -1;
}

static void server_main(int server, char *docroot)
{
  for(;;){
    struct sockaddr_storage addr;
    socklen addrlen = sizeof addr;
    int sock;
    int pid;

    sock = accept(server, (struct sockaddr*)&addr, &addrlen);
    if(sock < 0) log_exit("accept(2) failed: %s", sterror(errno));
    pid = fork();
    if(pid < 0) exit(3);
    if(pid == 0) {
      FILE *inf = fdopen(sock, "r");
      FILE *outf = fdopen(sock, "w");

      service(inf, outf, docroot);
      exit(0);
    }
    close(sock);
  }
}

static void become_daemon(void)
{
  int n;

  if(chdir("/") < 0)
    log_exit("chdir(2) failed: %s", strerror(errno));
  freopen("/dev/null", "r", stdin);
  freopen("/dev/null", "w", stdout);
  freopen("/dev/null", "w", stderr);
  n = fork();
  if(n < 0) log_exit("fork(2) failed: %s", strerror(errno));
  if(n != 0) _exit(0);
  if(setsid() < 0) log_exit("setsid(2) failed: %s", strerror(errno);
}

static void setup_environment(char *root, char *user, char *group)
{
  struct passwd *pw;
  struct group *gr;

  if(!user || !group) {
    fprintf(stderr, "use both of --user and --group\n");
    exit(1);
  }

  gr = getgrnam(group);
  if(!gr) {
    fprintf(stderr, "no such group: %s\n", group);
    exit(1);
  }
  if(setgid(gr->gr_gid) < 0){
    perror("setgid(2)");
    exit(1);
  }
  if(initgroups(user, gr->gr_gid) < 0){
    perror("initgroups(2)");
    exit(1);
  }

  pw = getpwnam(user);
  if(!pw) {
    fprintf(stderr, "no such user: %s\n", user);
    exit(1);
  }
  chroot(root);
  if(setuid(pw->pw_uid) < 0){
    perror("setuid(2)");
    exit(1);
  }
}

