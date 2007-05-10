#ifdef HAVE_MULTICAST
#include <stdlib.h>
#include <bcmnvram.h>
#include <shutils.h>
#include <utils.h>
#include <syslog.h>
#include <signal.h>
int
start_igmp_proxy (void)
{
  int ret = 0;
  pid_t pid;


  char *igmp_proxy_argv[] = { "igmprt",
    "-i",
    get_wan_face (),
    NULL
  };

  stop_igmp_proxy ();

  if (nvram_match ("block_multicast", "0"))
    {
      ret = _eval (igmp_proxy_argv, NULL, 0, &pid);
      syslog (LOG_INFO, "igmprt : multicast daemon successfully started\n");
    }

  cprintf ("done\n");
  return ret;
}

int
stop_igmp_proxy (void)
{
  if (pidof ("igmprt") > 0)
    syslog (LOG_INFO, "igmprt : multicast daemon successfully stopped\n");
  int ret = killall ("igmprt", SIGKILL);

  cprintf ("done\n");
  return ret;
}
#endif
