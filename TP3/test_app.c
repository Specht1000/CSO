/*
 * Simple disk I/O generator with multiple forks for testing C-SCAN
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define SECTOR_SIZE   512       // Tamanho de um setor do disco
#define DISK_SZ       (2097152 * SECTOR_SIZE) // Tamanho do disco em setores
#define N_ACCESSES    50        // Número de acessos por fork
#define N_FORKS       10        // Número de forks (processos filhos)

int main() {
    int fd, i, j;
    unsigned int pos;
    char buf[SECTOR_SIZE];

    printf("Iniciando teste de leitura de setores com múltiplos forks...\n");

    // Limpa buffers e caches de disco
    printf("Limpando cache de disco...\n");
    system("echo 3 > /proc/sys/vm/drop_caches");

    // Configura filas de escalonamento
    printf("Configurando filas de escalonamento...\n");
    system("echo 2 > /sys/block/sdb/queue/nomerges");
    system("echo 4 > /sys/block/sdb/queue/max_sectors_kb");
    system("echo 0 > /sys/block/sdb/queue/read_ahead_kb");

    // Abre o dispositivo de bloco (disco)
    fd = open("/dev/sdb", O_RDWR);
    if (fd < 0) {
        perror("Erro ao abrir o dispositivo...");
        return errno;
    }

    // String para escrita (exemplo de dados para escrever no disco)
    strcpy(buf, "C-SCAN Test Data");

    // Cria múltiplos processos filhos
    for (i = 0; i < N_FORKS; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("Erro ao criar fork...");
            return errno;
        }

        if (pid == 0) {
            // Processo filho
            srand(getpid()); // Semente única baseada no PID

            for (j = 0; j < N_ACCESSES; j++) {
                // Gera posição aleatória no disco
                pos = (rand() % (DISK_SZ / SECTOR_SIZE));
                printf("Processo PID %d, setor %d\n", getpid(), pos);

                // Posiciona o ponteiro do arquivo no setor aleatório
                lseek(fd, pos * SECTOR_SIZE, SEEK_SET);

                // Lê do setor (simula operação de leitura)
                read(fd, buf, SECTOR_SIZE);
            }

            close(fd); // Fecha o descritor de arquivo no processo filho
            exit(0);   // Finaliza o processo filho
        }
    }

    // Processo pai aguarda todos os filhos terminarem
    while (wait(NULL) > 0);

    // Fecha o descritor de arquivo no processo pai
    close(fd);

    printf("Teste concluído.\n");
    return 0;
}

