// Trabalho Prático 1 de Construção de Sistemas Operacionais
// Guilherme Specht
// Última atualização: 27/08/2024

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// VERSÃO DO SISTEMA E KERNEL
void get_system_version(char *version, size_t size){
    FILE *file = fopen("/proc/version", "r");
    if(file){
        fgets(version, size, file); // Pega toda a linha
        fclose(file);
    } 
    else{
        snprintf(version, size, "ERRO NA VERSÃO DO SISTEMA E KERNEL!");
    }
}

// UPTIME E TEMPO OCIOSO
void get_uptime_and_idle_time(char *uptime_buffer, char *idle_buffer, size_t size){
    FILE *file = fopen("/proc/uptime", "r");
    if(file){
        double uptime, idle_time;
        fscanf(file, "%lf %lf", &uptime, &idle_time);
        int uptime_days = uptime / 86400;
        int uptime_hours = ((int)uptime % 86400) / 3600;
        int uptime_minutes = ((int)uptime % 3600) / 60;
        int uptime_seconds = (int)uptime % 60;
        int idle_days = idle_time / 86400;
        int idle_hours = ((int)idle_time % 86400) / 3600;
        int idle_minutes = ((int)idle_time % 3600) / 60;
        int idle_seconds = (int)idle_time % 60;
        snprintf(uptime_buffer, size, "%d dias, %d horas, %d minutos e %d segundos", uptime_days, uptime_hours, uptime_minutes, uptime_seconds);
        snprintf(idle_buffer, size, "%d dias, %d horas, %d minutos e %d segundos", idle_days, idle_hours, idle_minutes, idle_seconds);
        fclose(file);
    }
    else{
        snprintf(uptime_buffer, size, "ERRO NO UPTIME!");
        snprintf(idle_buffer, size, "ERRO NO TEMPO OCIOSO!");
    }
}

// DATA E HORA DO SISTEMA (TEM QUE SER COM /driver/rtc)
void get_datetime(char *datetime, size_t size){
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(datetime, size, "%02d-%02d-%04d %02d:%02d:%02d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

// MODELO DE PROCESSADOR, VELOCIDADE E NÚMERO DE NÚCLEOS
void get_cpu_info(char *model, size_t model_size, int *cores, char *speed, size_t speed_size){
    FILE *file = fopen("/proc/cpuinfo", "r");
    if(file){
        char line[256];
        while(fgets(line, sizeof(line), file)){
            if(strncmp(line, "model name", 10) == 0) {
                sscanf(line, "model name : %[^\n]", model);
            }
            else if(strncmp(line, "cpu cores", 9) == 0){
                sscanf(line, "cpu cores : %d", cores);
            }
            else if(strncmp(line, "cpu MHz", 7) == 0){
                sscanf(line, "cpu MHz : %s", speed);
            }
        }
        fclose(file);
    }
    else{
        snprintf(model, model_size, "ERRO NO MODELO DO PROCESSADOR!");
        snprintf(speed, speed_size, "ERRO NA VELOCIDADE E NÚMERO DE NÚCLEOS!");
    }
}

// CARGA DO SISTEMA
void get_load_average(char *lavg, size_t size){
    FILE *file = fopen("/proc/loadavg", "r");
    if(file){
        fgets(lavg, size, file);
        fclose(file);
    }
    else{
        snprintf(lavg, size, "ERRO NA CARGA DO SISTEMA!");
    }
}

// CAPACIDADE OCUPADA DO PROCESSADOR
void get_cpu_usage(char *cpu_usage, size_t size){
    FILE *file = fopen("/proc/stat", "r");
    if(file){
        long user, nice, system, idle;
        fscanf(file, "cpu %ld %ld %ld %ld", &user, &nice, &system, &idle);
        long total = user + nice + system + idle;
        long usage = user + nice + system;
        double usage_percent = (double)usage / total * 100;
        snprintf(cpu_usage, size, "%.2f%%", usage_percent);
        fclose(file);
    } 
    else{
        snprintf(cpu_usage, size, "ERRO NA CAPACIDADE DA CPU!");
    }
}

// QUANTIDADE DE MEMÓRIA RAM TOTAL E USADA
void get_memory_info(char *mem_info, size_t size){
    FILE *file = fopen("/proc/meminfo", "r");
    if(file){
        char line[256];
        unsigned long total_memory = 0, free_memory = 0;
        while(fgets(line, sizeof(line), file)){
            if(strncmp(line, "MemTotal", 8) == 0){
                sscanf(line, "MemTotal: %lu kB", &total_memory);
            }
            else if (strncmp(line, "MemAvailable", 12) == 0){
                sscanf(line, "MemAvailable: %lu kB", &free_memory);
            }
        }
        snprintf(mem_info, size, "Total: %lu MB, Usada: %lu MB", total_memory / 1024, (total_memory - free_memory) / 1024);
        fclose(file);
    }
    else{
        snprintf(mem_info, size, "ERRO NA QUANTIDADE DE RAM!");
    }
}

// OPERAÇÕES SOBRE O SISTEMA DE I/O
void get_io_info(char *io_info, size_t size){
    FILE *file = fopen("/proc/diskstats", "r");
    if(file){
        char line[256];
        unsigned long reads = 0, writes = 0;
        while(fgets(line, sizeof(line), file)){
            unsigned long r, w;
            sscanf(line, "%*d %*d %*s %*lu %lu %*lu %*lu %*lu %lu", &r, &w);
            reads += r;
            writes += w;
        }
        snprintf(io_info, size, "Leituras: %lu, Escritas: %lu", reads, writes);
        fclose(file);
    }
    else{
        snprintf(io_info, size, "ERRO SOBRE O SISTEMA DE I/O!");
    }
}

// SISTEMA DE ARQUIVOS SUPORTADAS PELO KERNEL
void get_filesystems(char *fsystem, size_t size){
    FILE *file = fopen("/proc/filesystems", "r");
    if(file){
        char line[256];
        fsystem[0] = '\0';
        while(fgets(line, sizeof(line), file)){
            strncat(fsystem, line, size - strlen(fsystem) - 1);
        }
        fclose(file);
    }
    else{
        snprintf(fsystem, size, "ERRO NO SISTEMA DE ARQUIVOS SUPORTADOS!");
    }
}

// DISPOSITIVOS (CARACTER E BLOCO) E GRUPOS
void get_device_info(char *dev_info, size_t size){
    FILE *file = fopen("/proc/devices", "r");
    if(file){
        char line[256];
        dev_info[0] = '\0';
        while(fgets(line, sizeof(line), file)){
            strncat(dev_info, line, size - strlen(dev_info) - 1);
        }
        fclose(file);
    }
    else{
        snprintf(dev_info, size, "ERRO NOS DISPOSITIVOS E GRUPOS!");
    }
}

// DISPOSITIVO DE REDE
void get_network_devices(char *net_dev, size_t size){
    FILE *file = fopen("/proc/net/dev", "r");
    if(file){
        char line[256];
        net_dev[0] = '\0';
        fgets(line, sizeof(line), file);  // Pula o header
        fgets(line, sizeof(line), file);
        while (fgets(line, sizeof(line), file)) {
            strncat(net_dev, line, size - strlen(net_dev) - 1);
        }
        fclose(file);
    }
    else{
        snprintf(net_dev, size, "ERRO NO DISPOSITIVO DE REDE!");
    }
}

// LISTA DE PROCESSOS EM EXECUÇÃO
void get_process_list(char *pl, size_t size){
    FILE *file = popen("ps -o pid,comm", "r");
    if(file){
        char line[256];
        pl[0] = '\0';
        while(fgets(line, sizeof(line), file)){
            strncat(pl, line, size - strlen(pl) - 1);
        }
        pclose(file);
    }
    else{
        snprintf(pl, size, "ERRO NA LISTA DE PROCESSOS!");
    }
}

// GERAÇÃO DO TEXTO
void generate_text_file(){
    
    char version[256];
    char uptime[256];
    char idle_time[256];
    char datetime[256];
    char cpu_model[256];
    int cpu_cores;
    char cpu_speed[64];
    char load_avg[256];
    char cpu_usage[64];
    char memory_info[256];
    char io_info[256];
    char filesystems[1024];
    char device_info[1024];
    char network_devices[1024];
    char process_list[2048];

    get_system_version(version, sizeof(version));
    get_uptime_and_idle_time(uptime, idle_time, sizeof(uptime));
    get_datetime(datetime, sizeof(datetime));
    get_cpu_info(cpu_model, sizeof(cpu_model), &cpu_cores, cpu_speed, sizeof(cpu_speed));
    get_load_average(load_avg, sizeof(load_avg));
    get_cpu_usage(cpu_usage, sizeof(cpu_usage));
    get_memory_info(memory_info, sizeof(memory_info));
    get_io_info(io_info, sizeof(io_info));
    get_filesystems(filesystems, sizeof(filesystems));
    get_device_info(device_info, sizeof(device_info));
    get_network_devices(network_devices, sizeof(network_devices));
    get_process_list(process_list, sizeof(process_list));

    FILE *html_file = fopen("index.html", "w");
    if(html_file){
        fprintf(html_file, "<html>\n");
        fprintf(html_file, "<head><title>Trabalho Prático 1 - Construção de Sistemas Operacionais</title></head>\n");
        fprintf(html_file, "<meta http-equiv=\"refresh\" content=\"5\">\n");
        fprintf(html_file, "<body>\n");
        fprintf(html_file, "<h1>Trabalho Prático 1 de Construção de Sistemas Operacionais</h1>\n");
        fprintf(html_file, "<h2>Guilherme Specht</h2>\n");
        fprintf(html_file, "<hr>\n");
        fprintf(html_file, "<p><strong>Versão do Sistema e Kernel:</strong> %s</p>\n", version);
        fprintf(html_file, "<p><strong>Uptime:</strong> %s</p>\n", uptime);
        fprintf(html_file, "<p><strong>Tempo Ocioso:</strong> %s</p>\n", idle_time);
        fprintf(html_file, "<p><strong>Data e Hora:</strong> %s</p>\n", datetime);
        fprintf(html_file, "<p><strong>Modelo do Processador:</strong> %s</p>\n", cpu_model);
        fprintf(html_file, "<p><strong>Velocidade do Processador:</strong> %s MHz</p>\n", cpu_speed);
        fprintf(html_file, "<p><strong>Número de Núcleos:</strong> %d</p>\n", cpu_cores);
        fprintf(html_file, "<p><strong>Carga do Sistema:</strong> %s</p>\n", load_avg);
        fprintf(html_file, "<p><strong>Capacidade ocupada do processador:</strong> %s</p>\n", cpu_usage);
        fprintf(html_file, "<p><strong>Memória:</strong> %s</p>\n", memory_info);
        fprintf(html_file, "<p><strong>Operações sobre o sistema de I/O:</strong> %s</p>\n", io_info);
        fprintf(html_file, "<p><strong>Sistemas de Arquivos Suportados pelo Kernel:</strong></p>\n<pre>%s</pre>\n", filesystems);
        fprintf(html_file, "<p><strong>Dispositivos de Caractere e Bloco e Grupos:</strong></p>\n<pre>%s</pre>\n", device_info);
        fprintf(html_file, "<p><strong>Dispositivos de Rede:</strong></p>\n<pre>%s</pre>\n", network_devices);
        fprintf(html_file, "<p><strong>Lista de Processos:</strong></p>\n<pre>%s</pre>\n", process_list);
        fprintf(html_file, "</body>\n");
        fprintf(html_file, "</html>\n");
        fclose(html_file);
    }
    else{
        perror("ERRO AO ABRIR O ARQUIVO!");
    }
}

int main(){
    while(1){
        generate_text_file();
        sleep(5);
    }
    return 0;
}

