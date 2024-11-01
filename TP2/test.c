#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 256

int main() {
    int ret, fd, len;  // Variáveis para armazenar o valor de retorno de funções, descritor de arquivo e o tamanho da string
    char receive[BUFFER_LENGTH];  // Buffer para armazenar as mensagens lidas do dispositivo
    char stringToSend[BUFFER_LENGTH];  // Buffer para armazenar os comandos/mensagens a serem enviados ao dispositivo

    // Abrir o dispositivo /dev/mqueue no modo leitura e escrita
    fd = open("/dev/mqueue", O_RDWR);
    if (fd < 0) {  // Verifica se o dispositivo foi aberto com sucesso
        perror("Failed to open the device..."); 
        return errno;  
    }

    printf("Program started.\n"); 
    
    while (1) {  
        printf("Enter command: ");  
        memset(stringToSend, 0, BUFFER_LENGTH);  // Limpa o buffer de envio para garantir que ele esteja vazio
        fgets(stringToSend, BUFFER_LENGTH - 1, stdin);  // Lê a entrada do usuário
        len = strnlen(stringToSend, BUFFER_LENGTH);  // Obtém o comprimento da string de entrada
        stringToSend[len - 1] = '\0';  // Remove o caractere de nova linha ('\n') da string

        // Se o usuário só pressionou Enter (string vazia), sai do loop
        if (len == 1) {
            break;
        }

        // Envia o comando/mensagem para o driver de dispositivo
        ret = write(fd, stringToSend, strlen(stringToSend));  // Escreve a mensagem para o dispositivo
        if (ret < 0) {  // Verifica se a escrita foi bem-sucedida
            perror("Failed to write the message to the device.");  // Imprime a mensagem de erro, caso falhe
            continue;  // Continua no loop mesmo após um erro de escrita
        }

        // Limpa o buffer de leitura antes de receber a resposta do dispositivo
        memset(receive, 0, BUFFER_LENGTH);
        // Lê a resposta do dispositivo
        ret = read(fd, receive, BUFFER_LENGTH);  // Lê a mensagem do dispositivo
        if (ret < 0) {  // Verifica se a leitura foi bem-sucedida
            perror("Failed to read the message from the device.");  // Imprime a mensagem de erro, caso falhe
            continue;  // Continua no loop mesmo após um erro de leitura
        }

        // Se houver uma mensagem recebida, a exibe
        len = strnlen(receive, BUFFER_LENGTH);  // Calcula o comprimento da mensagem recebida
        if (len > 0) {  // Se o comprimento for maior que zero, há uma mensagem para exibir
            printf("Read message from device: [%s]\n", receive);  // Exibe a mensagem recebida do dispositivo
        } else {
            printf("No messages available to read.\n");  // Indica que não há mensagens para ler
        }
    }

    printf("Program ended.\n");  // Mensagem de encerramento do programa
    close(fd);  // Fecha o descritor de arquivo, liberando o dispositivo
    
    return 0;  // Retorna 0, indicando que o programa terminou com sucesso
}
