#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/mount.h>
int main(int argc, char **argv, char **envp) {
  write(1, "init starting\n", 14);
  mount("proc", "/proc", "proc", 0, 0);
  mount("sysfs", "/sys", "sysfs", 0, 0);
  mount("devtmpfs", "/dev", "devtmpfs", 0, 0);
  if (fork() == 0) {
    execl("/etc/init.d/S90keystone", "S90keystone", (char*)0);
    _exit(1);
  }
  wait(0);
  write(1, "enclave done\n", 13);
  while(1) pause();
}
