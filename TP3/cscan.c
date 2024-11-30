#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/jiffies.h>

/* Estrutura para o escalonador C-SCAN */
struct cscan_data {
    struct list_head queue;        		// Lista de requisições de IO
    struct timer_list dispatch_timer; 	// Timer para timeout e despacho periódico
    int request_count;              	// Contador de requisições na fila
    struct list_head processed_list; 	// Lista para armazenar blocos de setores processados
};

/* Parâmetros default */
static int cscan_queue_size = 10; // Tamanho máximo da fila de requisições
static int cscan_max_wait = 100;  // Timeout em milissegundos para o despacho
static int cscan_debug = 1;  	  // Ativa ou desativa mensagens de depuração

/* Parâmetros do módulo */
module_param(cscan_queue_size, int, 0644);  
MODULE_PARM_DESC(cscan_queue_size, "Tamanho da fila de requisições (5-100)"); 
module_param(cscan_max_wait, int, 0644);  
MODULE_PARM_DESC(cscan_max_wait, "Tempo máximo de espera em ms (1-100)");
module_param(cscan_debug, int, 0644);  
MODULE_PARM_DESC(cscan_debug, "Habilitar mensagens de depuração"); 

/* Estrutura para armazenar blocos de setores processados */
struct processed_list {
    struct list_head list;             // Nó para vincular na lista principal de blocos
    struct list_head sectors;          // Lista de setores processados neste bloco
};

/* Estrutura para armazenar setores processados */
struct processed_sector {
    struct list_head list;             // Nó para vincular na lista de setores dentro de um bloco
    unsigned long sector;              // Número do setor processado
};

/* Função para imprimir todos os setores processados até o momento */
static void print_all_processed(struct cscan_data *cd) {
    struct processed_list *block;  // Ponteiro para blocos processados
    struct processed_sector *ps;  // Ponteiro para setores processados

    if (list_empty(&cd->processed_list)) {  // Verifica se a lista de blocos está vazia
        printk(KERN_INFO "C-SCAN [summary]: Nenhum bloco processado.\n");
        return;
    }

    printk(KERN_INFO "C-SCAN [summary]: Blocos processados até o momento:\n");
    list_for_each_entry(block, &cd->processed_list, list) {  // Itera sobre cada bloco
        printk(KERN_INFO "C-SCAN [block]:");  
        list_for_each_entry(ps, &block->sectors, list) {  // Itera sobre cada setor no bloco
            printk(KERN_CONT " %lu", ps->sector);  
        }
        printk(KERN_INFO "\n");
    }
}

/* Função para ordenar a fila em ordem crescente de setores */
static void sort_queue(struct cscan_data *cd) {
    struct list_head sorted_queue;
    struct request *rq, *tmp;

    INIT_LIST_HEAD(&sorted_queue); // Inicializa uma lista temporária para ordenar as requisições

    while (!list_empty(&cd->queue)) {
        struct request *min_rq = NULL;
        unsigned long min_sector = ULONG_MAX;

        list_for_each_entry_safe(rq, tmp, &cd->queue, queuelist) {
            unsigned long sector = blk_rq_pos(rq);
            if (sector < min_sector) {
                min_sector = sector;
                min_rq = rq;
            }
        }

        if (min_rq) {
            list_del_init(&min_rq->queuelist); // Remove a requisição com o menor setor da fila original
            list_add_tail(&min_rq->queuelist, &sorted_queue); // Adiciona na fila ordenada
        }
    }

    // Substitui a fila original pela fila ordenada
    list_splice_tail_init(&sorted_queue, &cd->queue);
}


/* Função para despachar todas as requisições acumuladas na fila */
static int cscan_dispatch(struct request_queue *q, int force) {
    struct cscan_data *cd = q->elevator->elevator_data;  // Obtém os dados do escalonador
    struct processed_sector *ps;  // Ponteiro para setores processados
    struct processed_list *processed_block;  // Ponteiro para blocos processados

    if (!cd || list_empty(&cd->queue)) {  // Verifica se a fila está vazia
        if (cscan_debug) {
            printk(KERN_INFO "C-SCAN [dispatch]: Fila vazia ou estrutura nula\n");
        }
        return 0;
    }

    if (cscan_debug) {
        printk(KERN_INFO "C-SCAN [dispatch]: Ordenando a fila por número de setor\n");
    }

    // Ordena a fila em ordem crescente de setores
    sort_queue(cd);

    if (cscan_debug) {
        printk(KERN_INFO "C-SCAN [dispatch]: Iniciando processamento da lista ordenada\n");
    }

    // Aloca memória para um novo bloco de setores processados
    processed_block = kmalloc(sizeof(*processed_block), GFP_KERNEL);
    if (!processed_block) { 
        if (cscan_debug) {
            printk(KERN_ERR "C-SCAN [dispatch]: Falha ao alocar memória para bloco processado\n");
        }
        return -ENOMEM;
    }
    INIT_LIST_HEAD(&processed_block->sectors);  // Inicializa a lista de setores no bloco

    // Adiciona o setor inicial (0)
    ps = kmalloc(sizeof(*ps), GFP_KERNEL);
    if (ps) {
        ps->sector = 0;  // Define o setor inicial
        list_add_tail(&ps->list, &processed_block->sectors);
        if (cscan_debug) {
            printk(KERN_INFO "C-SCAN [dispatch]: Setor inicial [0] adicionado ao bloco\n");
        }
    }

    // Percorre os setores em ordem crescente
    while (!list_empty(&cd->queue)) {  // Processa enquanto houver requisições na fila
        struct request *rq = list_first_entry(&cd->queue, struct request, queuelist);  // Obtém a primeira requisição
        if (!rq) {  
            printk(KERN_ERR "C-SCAN [dispatch]: Requisição nula encontrada\n");
            continue;
        }

        unsigned long sector = blk_rq_pos(rq);  // Obtém o setor da requisição
        list_del_init(&rq->queuelist);  // Remove a requisição da fila
        cd->request_count--;  // Decrementa o contador de requisições

        // Aloca memória para o setor processado
        ps = kmalloc(sizeof(*ps), GFP_KERNEL);
        if (!ps) {  // Verifica falha de alocação
            printk(KERN_ERR "C-SCAN [dispatch]: Falha ao alocar memória para setor processado\n");
            continue;
        }
        ps->sector = sector;  // Define o número do setor
        list_add_tail(&ps->list, &processed_block->sectors);  // Adiciona o setor ao bloco

        if (cscan_debug) {
            printk(KERN_INFO "C-SCAN [dispatch]: Setor [%lu] processado, requisições restantes [%d]\n",
                   sector, cd->request_count);
        }

        elv_dispatch_sort(q, rq);  // Despacha a requisição
    }

    // Adiciona o setor final (fim do disco)
    ps = kmalloc(sizeof(*ps), GFP_KERNEL);
    if (ps) {
        unsigned long max_sector = 2097152;  // Valor máximo do setor (exemplo)
        ps->sector = max_sector;  // Define o setor final
        list_add_tail(&ps->list, &processed_block->sectors);
        if (cscan_debug) {
            printk(KERN_INFO "C-SCAN [dispatch]: Setor final [%lu] adicionado ao bloco\n", max_sector);
        }
    }

    // Adiciona o bloco processado à lista geral
    list_add_tail(&processed_block->list, &cd->processed_list);

    // Imprime os setores do bloco após o processamento
    printk(KERN_INFO "C-SCAN [block]: Setores processados neste bloco:");
    list_for_each_entry(ps, &processed_block->sectors, list) {
        printk(KERN_CONT " %lu", ps->sector);
    }
    printk(KERN_INFO "\n");

    if (cscan_debug) {
        printk(KERN_INFO "C-SCAN [dispatch]: Movendo para o fim do disco\n");
        printk(KERN_INFO "C-SCAN [dispatch]: Retornando ao início do disco para o próximo bloco\n");
    }

    return 1;  // Retorna sucesso
}


/* Adiciona uma requisição à fila */
static void cscan_add_request(struct request_queue *q, struct request *rq) {
    struct cscan_data *cd = q->elevator->elevator_data;  // Obtém os dados do escalonador

    unsigned long sector = blk_rq_pos(rq);  // Obtém o setor da requisição

    list_add_tail(&rq->queuelist, &cd->queue);  // Adiciona a requisição à fila
    cd->request_count++;  // Incrementa o contador de requisições

    if (cscan_debug) {
        printk(KERN_INFO "C-SCAN [add]: Adicionando setor [%lu]\n", sector);
    }

    if (cd->request_count >= cscan_queue_size) {  // Verifica se a fila atingiu o tamanho máximo
        printk(KERN_INFO "C-SCAN [add]: Fila cheia, despachando requisições\n");
        cscan_dispatch(q, 1);  // Despacha as requisições
    }
}

/* Callback do timer */
static void cscan_dispatch_timer(unsigned long data) {
    struct cscan_data *cd = (struct cscan_data *)data; // Obtém a estrutura do escalonador (cscan_data) a partir do dado passado para o timer

    // Verifica se a estrutura ou os dados do timer são nulos e reporta um erro, se necessário
    if (!cd || !cd->dispatch_timer.data) {
        printk(KERN_ERR "C-SCAN [timer]: Estrutura de dados nula ou inválida\n");
        return;
    }

    // Se há requisições na fila, tenta processá-las
    if (cd->request_count > 0) {
        struct request_queue *q = (struct request_queue *)cd->dispatch_timer.data; // Obtém a fila de requisições associada a partir dos dados do timer
        
        // Verifica se a fila de requisições é nula e reporta um erro, se necessário
        if (!q) {
            printk(KERN_ERR "C-SCAN [timer]: Ponteiro de fila nulo\n");
            return;
        }

        cscan_dispatch(q, 1); // Despacha as requisições pendentes na fila
    }

    mod_timer(&cd->dispatch_timer, jiffies + msecs_to_jiffies(cscan_max_wait)); // Reprograma o timer para disparar novamente após cscan_max_wait milissegundos
}

/* Inicializa o escalonador */
static int cscan_init_queue(struct request_queue *q, struct elevator_type *e) {
    struct elevator_queue *eq; // Estrutura do elevador (escalonador)
    struct cscan_data *cd;     // Estrutura de dados do escalonador C-SCAN

    // Aloca uma nova estrutura de elevador associada à fila e ao tipo do escalonador
    eq = elevator_alloc(q, e);
    if (!eq) { // Verifica se a alocação falhou
        return -ENOMEM; // Retorna erro de memória insuficiente
    }

    // Aloca memória para a estrutura de dados específica do C-SCAN
    cd = kmalloc(sizeof(*cd), GFP_KERNEL);
    if (!cd) { // Verifica se a alocação falhou
        kobject_put(&eq->kobj); // Libera a estrutura do elevador previamente alocada
        return -ENOMEM; // Retorna erro de memória insuficiente
    }

    // Inicializa a lista de requisições (fila principal)
    INIT_LIST_HEAD(&cd->queue);

    // Inicializa a lista de setores processados
    INIT_LIST_HEAD(&cd->processed_list);

    // Inicializa o contador de requisições como 0
    cd->request_count = 0;

    // Configura o timer para gerenciar o timeout de requisições
    init_timer(&cd->dispatch_timer); // Inicializa o timer
    cd->dispatch_timer.function = cscan_dispatch_timer; // Define a função de callback do timer
    cd->dispatch_timer.data = (unsigned long)q; // Armazena o ponteiro para a request_queue no timer

    // Agenda o timer para disparar após o intervalo definido (cscan_max_wait)
    mod_timer(&cd->dispatch_timer, jiffies + msecs_to_jiffies(cscan_max_wait));

    // Associa os dados do escalonador ao elevador e configura na fila
    eq->elevator_data = cd; // Armazena a estrutura cscan_data no elevador
    spin_lock_irq(q->queue_lock); // Bloqueia a fila para proteger contra concorrência
    q->elevator = eq; // Associa o elevador configurado à fila de requisições
    spin_unlock_irq(q->queue_lock); // Libera a fila

    return 0; // Retorna sucesso na inicialização
}

/* Finaliza o escalonador */
static void cscan_exit_queue(struct elevator_queue *e) {
    struct cscan_data *cd = e->elevator_data;
    struct processed_list *block, *tmp_block;
    struct processed_sector *ps, *tmp_ps;

    // Imprime uma única mensagem consolidando todos os setores processados
    printk(KERN_INFO "C-SCAN [summary]: Consolidando todos os setores processados:\n");
    list_for_each_entry(block, &cd->processed_list, list) {
        list_for_each_entry(ps, &block->sectors, list) {
            printk(KERN_CONT " %lu", ps->sector);
        }
    }
    printk(KERN_INFO "\n");

    // Libera todos os blocos e setores processados
    list_for_each_entry_safe(block, tmp_block, &cd->processed_list, list) {
        list_for_each_entry_safe(ps, tmp_ps, &block->sectors, list) {
            kfree(ps);
        }
        kfree(block);
    }

    del_timer_sync(&cd->dispatch_timer);
    kfree(cd);
}

/* Estrutura do escalonador */
static struct elevator_type elevator_cscan = {
    .ops.sq = {
        .elevator_add_req_fn  = cscan_add_request,
        .elevator_dispatch_fn = cscan_dispatch,
        .elevator_init_fn     = cscan_init_queue,
        .elevator_exit_fn     = cscan_exit_queue,
    },
    .elevator_name = "cscan",
    .elevator_owner = THIS_MODULE,
};

/* Inicializa e finaliza o módulo */
static int __init cscan_init(void) {
    printk(KERN_INFO "C-SCAN driver init\n");
    return elv_register(&elevator_cscan);
}

static void __exit cscan_exit(void) {
    printk(KERN_INFO "C-SCAN driver exit\n");
    elv_unregister(&elevator_cscan);
}

module_init(cscan_init);
module_exit(cscan_exit);

MODULE_AUTHOR("Guilherme Martins Specht");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("C-SCAN IO scheduler");
