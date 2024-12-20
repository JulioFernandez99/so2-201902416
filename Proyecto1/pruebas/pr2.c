#include <stdio.h>

#include <time.h> // Para struct timespec

#include <string.h> // Para memset

#include <linux/kernel.h>

#include <sys/syscall.h>

#include <unistd.h>

  

#define SYSCALL_TRACK 550 

  

struct syscall_usage {

unsigned long count;

struct timespec last_called;

};

  

#define MAX_SYSCALLS 1024

  

// busco el numero de syscall a nombres

const char *syscall_names[MAX_SYSCALLS] = {

[0] = "read",

[1] = "write",

[2] = "open",

[57] = "fork",

};

  

int main() {

struct syscall_usage stats[MAX_SYSCALLS];

memset(stats, 0, sizeof(stats));

  

// Llamar a la syscall

if (syscall(SYSCALL_TRACK, stats) < 0) {

perror("syscall");

return 1;

}

  

// muestro las estadisticas para las syscalls rastreadas

for (int i = 0; i < MAX_SYSCALLS; i++) {

if (syscall_names[i]) { // Solo mostrar syscalls con nombre definido

printf("Syscall %d (%s): Count=%lu, Last Called=%ld.%09ld\n",

i,

syscall_names[i],

stats[i].count,

stats[i].last_called.tv_sec,

stats[i].last_called.tv_nsec);

}

}

  

return 0;

}

