#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

// 支持高达 100 万条 IP 规则的合并
#define MAX_IPS 1000000

char *ipv4_list[MAX_IPS];
char *ipv6_list[MAX_IPS];
int ipv4_count = 0;
int ipv6_count = 0;

// 通用读取函数：读取已有文件或提取好的纯IP文件
void load_ips(const char *filename, char **list, int *count) {
    FILE *f = fopen(filename, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue; 
        
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) > 5) {
            list[(*count)++] = strdup(line);
        }
    }
    fclose(f);
}

// 排序比较函数
int compare_ips(const void *a, const void *b) {
    const char *str_a = *(const char **)a;
    const char *str_b = *(const char **)b;
    return strverscmp(str_a, str_b);
}

// 写入并去重
void write_and_dedup(const char *filename, char **list, int count, const char *date_str) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        perror("致命错误：无法创建或写入输出文件");
        printf("路径: %s\n", filename);
        exit(EXIT_FAILURE); 
    }

    fprintf(f, "# Update Date: %s\n", date_str);
    fprintf(f, "# Author: Zahc\n");

    for (int i = 0; i < count; i++) {
        if (i > 0 && strcmp(list[i], list[i-1]) == 0) {
            continue;
        }
        fprintf(f, "%s\n", list[i]);
    }
    fclose(f);
}

int main() {
    mkdir("rules", 0777);

    // 1. 加载本地可能已存在的旧数据
    load_ips("rules/ipv4.txt", ipv4_list, &ipv4_count);
    load_ips("rules/ipv6.txt", ipv6_list, &ipv6_count);

    // 2. 加载 GitHub Actions 正则提取出的第三方库数据
    load_ips("extracted_v4.txt", ipv4_list, &ipv4_count);
    load_ips("extracted_v6.txt", ipv6_list, &ipv6_count);

    // 3. 处理 APNIC 官方原始数据
    FILE *apnic_file = fopen("apnic_raw.txt", "r");
    if (apnic_file) {
        char line[512];
        while (fgets(line, sizeof(line), apnic_file)) {
            if (strstr(line, "|CN|ipv4|") || strstr(line, "|CN|ipv6|")) {
                char *registry = strtok(line, "|");
                char *cc = strtok(NULL, "|");
                char *type = strtok(NULL, "|");
                char *ip = strtok(NULL, "|");
                char *value_str = strtok(NULL, "|");

                if (registry && cc && type && ip && value_str) {
                    char buffer[128];
                    if (strcmp(type, "ipv4") == 0) {
                        int count = atoi(value_str);
                        int prefix = 32 - (int)(log2(count));
                        snprintf(buffer, sizeof(buffer), "%s/%d", ip, prefix);
                        ipv4_list[ipv4_count++] = strdup(buffer);
                    } else if (strcmp(type, "ipv6") == 0) {
                        snprintf(buffer, sizeof(buffer), "%s/%s", ip, value_str);
                        ipv6_list[ipv6_count++] = strdup(buffer);
                    }
                }
            }
        }
        fclose(apnic_file);
    }

    // 4. 对数据进行排序
    printf("开始对 %d 条 IPv4 和 %d 条 IPv6 进行排序去重...\n", ipv4_count, ipv6_count);
    qsort(ipv4_list, ipv4_count, sizeof(char *), compare_ips);
    qsort(ipv6_list, ipv6_count, sizeof(char *), compare_ips);

    // 5. 生成时间戳
    time_t t = time(NULL) + 8 * 3600; 
    struct tm *tm_info = gmtime(&t);
    char date_str[64];
    strftime(date_str, sizeof(date_str), "%Y-%m-%d %H:%M:%S", tm_info);

    // 6. 覆写输出最终文件
    write_and_dedup("rules/ipv4.txt", ipv4_list, ipv4_count, date_str);
    write_and_dedup("rules/ipv6.txt", ipv6_list, ipv6_count, date_str);

    printf("处理完成！\n");
    return EXIT_SUCCESS;
}
