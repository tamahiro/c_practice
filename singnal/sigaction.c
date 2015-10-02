#include <signal.h>

typdef void(*singal_hander)(int);

signal_hander trap_signal(int sig, singal_hander handler)
{
  struct sigaction act, old;

  act.sa_hander = handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART;
  if(sigaction(sig, &act, &old) < 0)
    return NULL;

  return old.sa_hander;
}
