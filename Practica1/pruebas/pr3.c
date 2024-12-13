#include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <sys/syscall.h>
    #include <errno.h>
    
    struct io_stats {
        unsigned long long rchar;
        unsigned long long wchar;
        unsigned long long syscr;
        unsigned long long syscw;
        unsigned long long read_bytes;
        unsigned long long write_bytes;
    };
    
    #define SYSCALL_GET_IO_THROTTLE 551
    
    int main(int argc, char *argv[]) {
        if (argc != 2) {
            fprintf(stderr, "Uso: %s <pid>\n", argv[0]);
            return 1;
        }
    
        pid_t pid = atoi(argv[1]);
        struct io_stats stats;
    
        int result = syscall(SYSCALL_GET_IO_THROTTLE, pid, &stats);
    
        if (result < 0) {
            perror("syscall");
            return 1;
        }
    
        printf("Estadísticas de I/O para PID %d:\n", pid);
        printf("  Bytes leídos desde memoria (rchar): %llu\n", stats.rchar);
        printf("  Bytes escritos a memoria (wchar): %llu\n", stats.wchar);
        printf("  Operaciones de lectura (syscr): %llu\n", stats.syscr);
        printf("  Operaciones de escritura (syscw): %llu\n", stats.syscw);
        printf("  Bytes leídos desde disco (read_bytes): %llu\n", stats.read_bytes);
        printf("  Bytes escritos a disco (write_bytes): %llu\n", stats.write_bytes);
    
        return 0;
    }
