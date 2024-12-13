#include <stdio.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <syscall.h>
#include <errno.h>

#define SYS_capture_memory_snapshot 549  // Número de tu syscall en syscall_64.tbl

struct extended_sysinfo {
    struct sysinfo si;
    unsigned long active_memory; // En KB
};

int main() {
    struct extended_sysinfo info;

    // invoco la sys
    if (syscall(SYS_capture_memory_snapshot, &info) == -1) {
        perror("Error al invocar la syscall");
        return 1;
    }

    // results
    printf("Resumen de memoria:\n");
    printf("Memoria Total: %lu KB\n", info.si.totalram);
    printf("Memoria Libre: %lu KB\n", info.si.freeram);
    printf("Memoria Usada: %lu KB\n", info.si.totalram - info.si.freeram);
    printf("Memoria Cache: %lu KB\n", info.si.bufferram);
    printf("Memoria Activa: %lu KB\n", info.active_memory);
    printf("Swap Total: %lu KB\n", info.si.totalswap);
    printf("Swap Libre: %lu KB\n", info.si.freeswap);

    return 0;
}
