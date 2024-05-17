#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <fcntl.h> // For open()
#include <string.h> // For strtok()

// Function to get CPU usage
double get_cpu_usage() {
    FILE* file = fopen("/proc/stat", "r");
    if (!file) {
        perror("Failed to open /proc/stat");
        return -1.0;
    }

    unsigned long long user, nice, system, idle;
    fscanf(file, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle);
    fclose(file);

    unsigned long long total = user + nice + system + idle;
    unsigned long long busy = total - idle;

    return (double)busy / total;
}

// Function to get memory usage
double get_memory_usage() {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        perror("Failed to get system information");
        return -1.0;
    }

    unsigned long long total_mem = info.totalram;
    unsigned long long free_mem = info.freeram;
    double used_mem = (double)(total_mem - free_mem) / total_mem;

    return used_mem;
}

// Function to get disk usage
double get_disk_usage() {
    struct statvfs stat;
    if (statvfs("/", &stat) != 0) {
        perror("Failed to get file system information");
        return -1.0;
    }

    unsigned long long total_blocks = stat.f_blocks * stat.f_frsize;
    unsigned long long free_blocks = stat.f_bfree * stat.f_frsize;
    double used_space = (double)(total_blocks - free_blocks) / total_blocks;

    return used_space;
}

// Function to get the number of running processes
int get_running_processes() {
    int running_processes = -1; // Default to -1 if failed to open file
    int file = open("/proc/loadavg", O_RDONLY);
    if (file != -1) {
        char buffer[128];
        ssize_t num_read = read(file, buffer, sizeof(buffer));
        if (num_read != -1) {
            buffer[num_read] = '\0'; // Null-terminate the buffer
            char *token = strtok(buffer, " ");
            if (token != NULL) {
                // The fifth token in the loadavg file is the number of currently running processes
                for (int i = 1; i < 5; ++i) {
                    token = strtok(NULL, " ");
                    if (token == NULL)
                        break;
                }
                if (token != NULL)
                    running_processes = atoi(token);
            }
        }
        close(file);
    }
    return running_processes;
}

int main() {
    while (1) {
        double cpu_usage = get_cpu_usage() * 100;
        double memory_usage = get_memory_usage() * 100;
        double disk_usage = get_disk_usage() * 100;
        int running_processes = get_running_processes();

        printf("CPU Usage: %.2f%%\n", cpu_usage);
        printf("Memory Usage: %.2f%%\n", memory_usage);
        printf("Disk Usage: %.2f%%\n", disk_usage);
        printf("Running Processes: %d\n", running_processes);

        // Sleep for 1 second before checking again
        sleep(5);
    }

    return 0;
}
