#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

#define HIGH_CPU_THRESHOLD 85.0
#define LOW_MEMORY_THRESHOLD 15.0
#define LOW_DISK_THRESHOLD 15.0
#define REPORT_INTERVAL 60

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

int get_running_processes() {
    int running_processes = -1;
    int file = open("/proc/loadavg", O_RDONLY);
    if (file != -1) {
        char buffer[128];
        ssize_t num_read = read(file, buffer, sizeof(buffer));
        if (num_read != -1) {
            buffer[num_read] = '\0';
            char *token = strtok(buffer, " ");
            if (token != NULL) {
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

void get_network_usage(double *rx_mb, double *tx_mb) {
    FILE *file = fopen("/proc/net/dev", "r");
    if (!file) {
        perror("Failed to open /proc/net/dev");
        *rx_mb = 0;
        *tx_mb = 0;
        return;
    }

    char line[256];
    fgets(line, sizeof(line), file);
    fgets(line, sizeof(line), file);

    unsigned long long rx_bytes = 0;
    unsigned long long tx_bytes = 0;
    while (fgets(line, sizeof(line), file)) {
        char interface[32];
        unsigned long long r_bytes, t_bytes;
        sscanf(line, "%s %llu %*s %*s %*s %*s %*s %*s %*s %llu", interface, &r_bytes, &t_bytes);
        rx_bytes += r_bytes;
        tx_bytes += t_bytes;
    }

    *rx_mb = (double)rx_bytes / 1048576;
    *tx_mb = (double)tx_bytes / 1048576;

    fclose(file);
}

double get_uptime() {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        perror("Failed to get system information");
        return -1.0;
    }
    return (double)info.uptime / 60;
}

void get_battery_status(int *capacity, char *status, size_t status_size) {
    FILE *file;
    
    file = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (file) {
        fscanf(file, "%d", capacity);
        fclose(file);
    } else {
        perror("Failed to open /sys/class/power_supply/BAT0/capacity");
        *capacity = -1;
    }
    
    file = fopen("/sys/class/power_supply/BAT0/status", "r");
    if (file) {
        fgets(status, status_size, file);
        status[strcspn(status, "\n")] = '\0';
        fclose(file);
    } else {
        perror("Failed to open /sys/class/power_supply/BAT0/status");
        strncpy(status, "Unknown", status_size);
    }
}

void check_for_alerts(double cpu_usage, double memory_usage, double disk_usage) {
    if (cpu_usage > HIGH_CPU_THRESHOLD) {
        printf("ALERT: High CPU usage detected: %.2f%%\n", cpu_usage);
    }
    if (memory_usage > (100 - LOW_MEMORY_THRESHOLD)) {
        printf("ALERT: Low memory available: %.2f%% used\n", memory_usage);
    }
    if (disk_usage > (100 - LOW_DISK_THRESHOLD)) {
        printf("ALERT: Low disk space available: %.2f%% used\n", disk_usage);
    }
}

void generate_report(double cpu_usage, double memory_usage, double disk_usage, int running_processes, double rx_mb, double tx_mb, double uptime_minutes, int battery_capacity, const char *battery_status) {
    FILE *file = fopen("system_report.txt", "a");
    if (!file) {
        perror("Failed to open system_report.txt");
        return;
    }

    time_t now = time(NULL);
    fprintf(file, "Report generated at: %s", ctime(&now));
    fprintf(file, "CPU Usage: %.2f%%\n", cpu_usage);
    fprintf(file, "Memory Usage: %.2f%%\n", memory_usage);
    fprintf(file, "Disk Usage: %.2f%%\n", disk_usage);
    fprintf(file, "Running Processes: %d\n", running_processes);
    fprintf(file, "Network Usage: RX %.2f MB, TX %.2f MB\n", rx_mb, tx_mb);
    fprintf(file, "Uptime: %.2f minutes\n", uptime_minutes);
    fprintf(file, "Battery: %d%% (%s)\n", battery_capacity, battery_status);
    fprintf(file, "--------------------------------------------------\n");

    fclose(file);
}

int main() {
    time_t last_report_time = time(NULL) - REPORT_INTERVAL;

    while (1) {
        double cpu_usage = get_cpu_usage() * 100;
        double memory_usage = get_memory_usage() * 100;
        double disk_usage = get_disk_usage() * 100;
        int running_processes = get_running_processes();

        double rx_mb, tx_mb;
        get_network_usage(&rx_mb, &tx_mb);
        double uptime_minutes = get_uptime();

        int battery_capacity;
        char battery_status[32];
        get_battery_status(&battery_capacity, battery_status, sizeof(battery_status));

        printf("CPU Usage: %.2f%%\n", cpu_usage);
        printf("Memory Usage: %.2f%%\n", memory_usage);
        printf("Disk Usage: %.2f%%\n", disk_usage);
        printf("Running Processes: %d\n", running_processes);
        printf("Network Usage: RX %.2f MB, TX %.2f MB\n", rx_mb, tx_mb);
        printf("Uptime: %.2f minutes\n", uptime_minutes);
        printf("Battery: %d%% (%s)\n", battery_capacity, battery_status);

        check_for_alerts(cpu_usage, memory_usage, disk_usage);

        time_t current_time = time(NULL);
        if (difftime(current_time, last_report_time) >= REPORT_INTERVAL) {
            generate_report(cpu_usage, memory_usage, disk_usage, running_processes, rx_mb, tx_mb, uptime_minutes, battery_capacity, battery_status);
            last_report_time = current_time;
        }

        sleep(5);
    }

    return 0;
}
