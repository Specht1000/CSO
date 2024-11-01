#include <linux/init.h>
#include <linux/module.h> 
#include <linux/device.h> 
#include <linux/kernel.h>
#include <linux/fs.h> 
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>

// Definições do nome do dispositivo e da classe
#define DEVICE_NAME "mqueue"     // Nome do dispositivo no /dev
#define CLASS_NAME  "mqueue_class" // Nome da classe do dispositivo

// Informações do módulo
MODULE_LICENSE("GPL");           
MODULE_AUTHOR("Guilherme Martins Specht");
MODULE_DESCRIPTION("Desenvolvimento do trabalho TP2 de CSO."); 
MODULE_VERSION("0.1.0");         

// Variáveis globais para o número principal e a classe do dispositivo
static int majorNumber;          // Número principal do dispositivo
static struct class *mqueueClass = NULL;  // Classe do dispositivo
static struct device *mqueueDevice = NULL;  // Dispositivo do kernel

// Parâmetros configuráveis pelo usuário
static int max_messages = 5;     // Número máximo de mensagens por processo
static int max_msg_size = 250;   // Tamanho máximo de cada mensagem

// Define os parâmetros que podem ser configurados pelo usuário no momento da carga do módulo
module_param(max_messages, int, 0644);  // Parâmetro de número máximo de mensagens
MODULE_PARM_DESC(max_messages, "Número máximo de mensagens por processo"); // Descrição do parâmetro

module_param(max_msg_size, int, 0644);  // Parâmetro de tamanho máximo de mensagens
MODULE_PARM_DESC(max_msg_size, "Tamanho máximo de cada mensagem (bytes)"); // Descrição do parâmetro

// Definir a estrutura para armazenar as mensagens
struct message_s {
    struct list_head link;       // Estrutura de lista ligada para conectar as mensagens
    char *message;               // Ponteiro para o conteúdo da mensagem (alocado dinamicamente)
    short size;                  // Tamanho da mensagem
};

// Definir a estrutura para armazenar processos
struct process_s {
    struct list_head link;       // Estrutura de lista ligada para conectar os processos
    pid_t pid;                   // PID do processo
    char *name;                  // Nome do processo (alocado dinamicamente)
    struct list_head msg_list;   // Lista de mensagens associadas a este processo
    int msg_count;               // Contador de mensagens na fila
};

// Lista de processos registrados
struct list_head process_list;   // Lista de processos que foram registrados

// Função para registrar um processo e inicializar sua lista de mensagens
int register_process(char *name, pid_t pid) {
    struct process_s *new_proc = kmalloc(sizeof(struct process_s), GFP_KERNEL);  // Aloca memória para um novo processo
    
    if (!new_proc) {              // Verifica se a alocação foi bem-sucedida
        printk(KERN_INFO "Memory allocation failed for process registration\n");
        return -ENOMEM;
    }

    new_proc->name = kmalloc(max_msg_size, GFP_KERNEL);  // Aloca memória para o nome do processo
    if (!new_proc->name) {        // Verifica se a alocação do nome foi bem-sucedida
        kfree(new_proc);          // Libera a memória do processo se a alocação falhar
        printk(KERN_INFO "Memory allocation failed for process name\n");
        return -ENOMEM;
    }

    strcpy(new_proc->name, name);  // Copia o nome do processo para a estrutura
    new_proc->pid = pid;           // Armazena o PID do processo
    new_proc->msg_count = 0;       // Inicializa o contador de mensagens
    INIT_LIST_HEAD(&new_proc->msg_list);  // Inicializa a lista de mensagens do processo
    list_add_tail(&new_proc->link, &process_list);  // Adiciona o processo à lista de processos
    printk(KERN_INFO "Process %s (PID: %d) registered successfully\n", name, pid);
    
    return 0;
}

// Função para desregistrar um processo, removendo suas mensagens e liberando memória
int unregister_process(char *name, pid_t pid) {
    struct process_s *proc;
    struct message_s *msg, *tmp;
    
    list_for_each_entry(proc, &process_list, link) {  // Percorre a lista de processos registrados
        if (strcmp(proc->name, name) == 0 && proc->pid == pid) {  // Verifica se o nome e o PID correspondem
            // Remove todas as mensagens associadas ao processo
            list_for_each_entry_safe(msg, tmp, &proc->msg_list, link) {
                list_del(&msg->link);   // Remove a mensagem da lista
                kfree(msg->message);    // Libera a memória da mensagem
                kfree(msg);             // Libera a estrutura da mensagem
            }
            list_del(&proc->link);      // Remove o processo da lista de processos
            kfree(proc->name);          // Libera a memória do nome do processo
            kfree(proc);                // Libera a estrutura do processo
            printk(KERN_INFO "Process %s (PID: %d) unregistered and messages discarded\n", name, pid);
            return 0;
        }
    }
    
    printk(KERN_INFO "Process %s (PID: %d) not found for unregistration\n", name, pid);  // Caso o processo não seja encontrado
    return -EINVAL;
}

// Função para adicionar uma mensagem à lista de mensagens de um processo
static int list_add_message_to_process(struct process_s *proc, char *data) {
    if (proc->msg_count >= max_messages) {  // Verifica se o número de mensagens excede o limite configurado
        printk(KERN_INFO "Process %s message queue is full. Discarding oldest message.\n", proc->name);
        // Remove a mensagem mais antiga (primeira da lista)
        struct message_s *oldest_msg = list_first_entry(&proc->msg_list, struct message_s, link);
        list_del(&oldest_msg->link);        // Remove a mensagem da lista
        kfree(oldest_msg->message);         // Libera a memória da mensagem
        kfree(oldest_msg);                  // Libera a estrutura da mensagem
        proc->msg_count--;                  // Decrementa o contador de mensagens
    }

    // Verifica se o tamanho da mensagem excede o tamanho máximo permitido
    if (strlen(data) > max_msg_size) {
        printk(KERN_INFO "Message exceeds the maximum allowed size for process %s. Discarding.\n", proc->name);
        return -EINVAL;
    }

    struct message_s *new_msg = kmalloc(sizeof(struct message_s), GFP_KERNEL);  // Aloca memória para uma nova mensagem
    if (!new_msg) {                        // Verifica se a alocação foi bem-sucedida
        printk(KERN_INFO "Memory allocation failed for message\n");
        return -ENOMEM;
    }

    new_msg->message = kmalloc(max_msg_size, GFP_KERNEL);  // Aloca memória para o conteúdo da mensagem
    if (!new_msg->message) {          // Verifica se a alocação foi bem-sucedida
        kfree(new_msg);               // Libera a estrutura da mensagem se a alocação falhar
        printk(KERN_INFO "Memory allocation failed for message content\n");
        return -ENOMEM;
    }

    strcpy(new_msg->message, data);  // Copia o conteúdo da mensagem para a estrutura
    new_msg->size = strlen(data);    // Armazena o tamanho da mensagem
    
    list_add_tail(&(new_msg->link), &proc->msg_list);  // Adiciona a mensagem à lista do processo
    proc->msg_count++;              // Incrementa o contador de mensagens
    
    printk(KERN_INFO "Message added to process %s: %s\n", proc->name, data);
    return 0;
}

// Função de escrita no dispositivo, que processa comandos como registro/desregistro e envio de mensagens
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char command[max_msg_size], target_process[max_msg_size], read_process[max_msg_size];
    struct process_s *proc;
    int num_messages = 1;  // Número de mensagens a ser lido, por padrão 1

    if (sscanf(buffer, "/reg %s", target_process) == 1) {  // Comando para registrar um processo
        register_process(target_process, current->pid);    // Registra o processo
        return len;
    }
    
    if (sscanf(buffer, "/unreg %s", target_process) == 1) {  // Comando para desregistrar um processo
        unregister_process(target_process, current->pid);    // Desregistra o processo
        return len;
    }

    if (sscanf(buffer, "/read %s %d", read_process, &num_messages) >= 1) {  // Comando para ler mensagens
        list_for_each_entry(proc, &process_list, link) {  // Percorre a lista de processos
            if (strcmp(proc->name, read_process) == 0) {  // Encontra o processo correspondente
                int i = 0;
                struct message_s *msg, *tmp;

                // Verifica se há mensagens suficientes disponíveis
                int available_messages = 0;
                list_for_each_entry(msg, &proc->msg_list, link) {
                    available_messages++;
                }

                if (available_messages == 0) {  // Nenhuma mensagem disponível
                    printk(KERN_INFO "Error: process %s has no messages\n", read_process);
                    return -EINVAL;
                }

                if (available_messages < num_messages) {  // Número insuficiente de mensagens
                    printk(KERN_INFO "Error: process %s has only %d messages\n", read_process, available_messages);
                    return -EINVAL;
                }

                // Lê e remove as mensagens solicitadas da lista do processo
                list_for_each_entry_safe(msg, tmp, &proc->msg_list, link) {
                    if (i >= num_messages) break;
                    printk(KERN_INFO "Process %s read message: %s\n", read_process, msg->message);
                    list_del(&msg->link);        // Remove a mensagem da lista
                    kfree(msg->message);         // Libera a memória da mensagem
                    kfree(msg);                  // Libera a estrutura da mensagem
                    i++;
                }
                return len;
            }
        }

        // Se o processo não for encontrado
        printk(KERN_INFO "Error: process %s not found\n", read_process);
        return -EINVAL;
    }

    if (sscanf(buffer, "/%s", target_process) == 1) {  // Envio de mensagem a um processo
        list_for_each_entry(proc, &process_list, link) {  // Percorre a lista de processos
            if (strcmp(proc->name, target_process) == 0) {
                char *message = strchr(buffer, ' ') + 1;  // Obtém o conteúdo da mensagem
                list_add_message_to_process(proc, message);  // Adiciona a mensagem à lista do processo
                printk(KERN_INFO "Message sent to process %s: %s\n", target_process, message);
                return len;
            }
        }
        printk(KERN_INFO "Error: process %s not found\n", target_process);  // Processo não encontrado
        return -EINVAL;
    }

    printk(KERN_INFO "Invalid command\n");  // Comando inválido
    return -EINVAL;
}

// Função de leitura do dispositivo, que envia mensagens para o espaço do usuário
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    struct process_s *proc;              // Ponteiro para iterar sobre os processos registrados
    struct message_s *msg;               // Ponteiro para iterar sobre as mensagens de cada processo
    char *tmp_buffer;                    // Buffer temporário para armazenar as mensagens antes de enviá-las ao espaço do usuário
    int offset_len = 0;                  // Variável para acompanhar o tamanho atual do buffer temporário

    // Aloca um buffer temporário para armazenar as mensagens, com tamanho máximo de 10 vezes o tamanho máximo de uma mensagem
    tmp_buffer = kmalloc(max_msg_size * 10, GFP_KERNEL);  
    if (!tmp_buffer) {                   // Verifica se a alocação foi bem-sucedida
        printk(KERN_INFO "Memory allocation failed for temporary buffer\n");
        return -ENOMEM;                  // Retorna erro se a alocação falhar
    }

    memset(tmp_buffer, 0, max_msg_size * 10);  // Inicializa o buffer temporário com zeros

    // Percorre a lista de processos registrados para buscar as mensagens associadas a cada processo
    list_for_each_entry(proc, &process_list, link) {
        // Se o processo tiver mensagens na fila
        if (!list_empty(&proc->msg_list)) {
            // Percorre a lista de mensagens do processo
            list_for_each_entry(msg, &proc->msg_list, link) {
                // Adiciona as mensagens ao buffer temporário, formatando a saída com o nome do processo e o conteúdo da mensagem
                offset_len += snprintf(tmp_buffer + offset_len, max_msg_size * 10 - offset_len, "Process %s message: %s\n", proc->name, msg->message);
                // Verifica se o buffer temporário foi preenchido até o limite
                if (offset_len >= max_msg_size * 10) {
                    printk(KERN_INFO "Buffer overflow, truncating messages.\n");
                    break;  // Se o buffer estourar, interrompe a adição de mensagens
                }
            }
        }
    }

    // Se não houver mensagens disponíveis em nenhum processo
    if (offset_len == 0) {
        printk(KERN_INFO "Mqueue Driver: No messages available to display.\n");
        kfree(tmp_buffer);               // Libera a memória do buffer temporário
        return 0;                        // Retorna 0, indicando que não há dados para ler
    }

    // Calcula o número correto de caracteres nas mensagens sem contar os extras
    int message_len = 0;
    list_for_each_entry(proc, &process_list, link) {
        if (!list_empty(&proc->msg_list)) {
            list_for_each_entry(msg, &proc->msg_list, link) {
                message_len += strlen(msg->message);  // Soma o comprimento de cada mensagem
            }
        }
    }

    // Copia o conteúdo do buffer temporário para o espaço do usuário
    if (copy_to_user(buffer, tmp_buffer, offset_len)) {
        printk(KERN_INFO "Mqueue Driver: Failed to send messages to the user.\n");
        kfree(tmp_buffer);               // Libera o buffer temporário em caso de falha
        return -EFAULT;                  // Retorna erro de falha na cópia para o espaço do usuário
    }

    // Loga a quantidade de caracteres enviados para o espaço do usuário
    printk(KERN_INFO "Mqueue Driver: Sent %d characters to the user\n", message_len);

    kfree(tmp_buffer);                   // Libera a memória do buffer temporário
    return message_len;                  // Retorna o número real de caracteres nas mensagens
}

// Função de fechamento do dispositivo
static int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "Mqueue Driver: device successfully closed\n");
    return 0;
}

// Estrutura de operações de arquivo (read, write, release)
static struct file_operations fops = {
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// Função de inicialização do módulo
static int __init mqueue_init(void) {
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);  // Registra o dispositivo e obtém o número major
    if (majorNumber < 0) {
        printk(KERN_ALERT "Mqueue Driver failed to register a major number\n");
        return majorNumber;
    }
    printk(KERN_INFO "Mqueue Driver: registered correctly with major number %d\n", majorNumber);  // Exibe o número major

    mqueueClass = class_create(THIS_MODULE, CLASS_NAME);  // Cria a classe do dispositivo
    if (IS_ERR(mqueueClass)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to register device class\n");
        return PTR_ERR(mqueueClass);
    }
    printk(KERN_INFO "Mqueue Driver: device class registered correctly\n");

    mqueueDevice = device_create(mqueueClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);  // Cria o dispositivo
    if (IS_ERR(mqueueDevice)) {
        class_destroy(mqueueClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "Failed to create the device\n");
        return PTR_ERR(mqueueDevice);
    }
    printk(KERN_INFO "Mqueue Driver: device created correctly\n");

    INIT_LIST_HEAD(&process_list);  // Inicializa a lista de processos
    printk(KERN_INFO "Mqueue Driver: initialized\n");
    return 0;
}

// Função de saída do módulo
static void __exit mqueue_exit(void) {
    device_destroy(mqueueClass, MKDEV(majorNumber, 0));  // Destrói o dispositivo
    class_unregister(mqueueClass);  // Remove a classe do dispositivo
    class_destroy(mqueueClass);     // Destroi a classe
    unregister_chrdev(majorNumber, DEVICE_NAME);  // Remove o registro do dispositivo
    printk(KERN_INFO "Mqueue Driver: exiting\n");
}

// Define as funções de inicialização e saída do módulo
module_init(mqueue_init);
module_exit(mqueue_exit);
