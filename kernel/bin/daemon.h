#ifndef __DAEMON_H__
#define __DAEMON_H__

#define DAEMON_DATADIR              "/etc/daemon.d/"

#define COMMAND_START               1
#define COMMAND_RESTART             2
#define COMMAND_STOP                3
#define COMMAND_STATUS              4

struct daemon_t
{
    char *name;
    char *desc;
    char *cmd;
    char *cmdargs;
    char *envpath;
    pid_t pid;
};

#endif      /* __DAEMON_H__ */
