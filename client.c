#include "padrao.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int palpite_valido(const char *entrada, int saida[5]) {
    if (strlen(entrada) != 5) {
        return -1;
    }
    for (int i = 0; i < 5; i++) {
        if (entrada[i] < '0' || entrada[i] > '9') {
            return -1;
        }
        saida[i] = entrada[i] - '0';
    }
    return 0;
}


int leitura(int sockfd, void *buf, int len) {
    char *ptr = (char *)buf;
    int total_lido = 0;
    int faltante = len;
    int n;

    while (total_lido < len) {
        n = recv(sockfd, ptr + total_lido, faltante, 0);
        
        if (n <= 0) {
            return n; 
        }

        total_lido += n;
        faltante -= n;
    }

    return total_lido; 
}

int envio(int sockfd, const void *buf, int len) {
    const char *ptr = (const char *)buf;
    int total_enviado = 0;
    int faltante = len;
    int n;

    while (total_enviado < len) {
        n = send(sockfd, ptr + total_enviado, faltante, 0);

        if (n <= 0) {
            return n;
        }

        total_enviado += n;
        faltante -= n;
    }

    return total_enviado;
}

static void mostrar_uso(const char *programa) {
    fprintf(stderr, "Uso: %s <ip> <porta>\n", programa);
}

static int porta_valida(const char *entrada) {
    char *fim;
    long valor;

    if (entrada == NULL || entrada[0] == '\0') {
        return 0;
    }

    errno = 0;
    valor = strtol(entrada, &fim, 10);

    return errno == 0 && *fim == '\0' && valor >= 1 && valor <= 65535;
}

static int detectar_familia_ip(const char *ip) {
    struct in_addr endereco4;
    struct in6_addr endereco6;

    if (inet_pton(AF_INET, ip, &endereco4) == 1) {
        return AF_INET;
    }

    if (inet_pton(AF_INET6, ip, &endereco6) == 1) {
        return AF_INET6;
    }

    return -1;
}

static int conectar_servidor(const char *ip, int porta, int familia) {
    int sockfd;

    sockfd = socket(familia, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Erro ao criar socket");
        return -1;
    }

    if (familia == AF_INET) {
        struct sockaddr_in endereco4;

        memset(&endereco4, 0, sizeof(endereco4));
        endereco4.sin_family = AF_INET;
        endereco4.sin_port = htons((uint16_t)porta);
        inet_pton(AF_INET, ip, &endereco4.sin_addr);

        if (connect(sockfd, (struct sockaddr *)&endereco4, sizeof(endereco4)) < 0) {
            perror("Erro ao conectar");
            close(sockfd);
            return -1;
        }
    } else {
        struct sockaddr_in6 endereco6;

        memset(&endereco6, 0, sizeof(endereco6));
        endereco6.sin6_family = AF_INET6;
        endereco6.sin6_port = htons((uint16_t)porta);
        inet_pton(AF_INET6, ip, &endereco6.sin6_addr);

        if (connect(sockfd, (struct sockaddr *)&endereco6, sizeof(endereco6)) < 0) {
            perror("Erro ao conectar");
            close(sockfd);
            return -1;
        }
    }

    return sockfd;
}

static void imprimir_feedback(const HackerMessage *msg) {
    printf("Dica: ");
    for (int i = 0; i < 5; i++) {
        if (msg->feedback[i] == 2) {
            printf("%d", msg->guess[i]);
        } else if (msg->feedback[i] == 1) {
            printf("*");
        } else {
            printf("_");
        }
    }
    printf("\n");
    printf("Tentativas realizadas: %d\n", msg->attempts);
}

int main() {
    int sockfd;
    int porta;
    int familia;
    HackerMessage msg;

    if (argc != 3) {
        mostrar_uso(argv[0]);
        return 1;
    }

    if (!porta_valida(argv[2])) {
        fprintf(stderr, "Porta deve ser um numero entre 1 e 65535.\n");
        mostrar_uso(argv[0]);
        return 1;
    }

    porta = atoi(argv[2]);
    familia = detectar_familia_ip(argv[1]);
    if (familia < 0) {
        fprintf(stderr, "Endereco IP invalido.\n");
        mostrar_uso(argv[0]);
        return 1;
    }

    sockfd = conectar_servidor(argv[1], porta, familia);
    if (sockfd < 0) {
        fprintf(stderr, "Erro ao conectar ao servidor.\n");
        return 1;
    }

    if (leitura(sockfd, &msg, sizeof(msg)) <= 0 || msg.type != MSG_START) {
        fprintf(stderr, "Erro ao receber mensagem inicial.\n");
        close(sockfd);
        return 1;
    }

    while (1) {
        char entrada[MSG_SIZE];
        int guess[5];

        printf("Insira seu palpite: ");
        fflush(stdout);

        if (fgets(entrada, sizeof(entrada), stdin) == NULL) {
            memset(&msg, 0, sizeof(msg));
            msg.type = MSG_EXIT;
            envio(sockfd, &msg, sizeof(msg));
            break;
        }

        entrada[strcspn(entrada, "\n")] = '\0';

        memset(&msg, 0, sizeof(msg));
        msg.type = MSG_GUESS;

        if (palpite_valido(entrada, guess) == 0) {
            for (int i = 0; i < 5; i++) {
                msg.guess[i] = guess[i];
            }
        } else {
            for (int i = 0; i < 5; i++) {
                msg.guess[i] = -1;
            }
        }

        if (envio(sockfd, &msg, sizeof(msg)) <= 0) {
            fprintf(stderr, "Erro ao enviar palpite.\n");
            break;
        }

        if (leitura(sockfd, &msg, sizeof(msg)) <= 0) {
            fprintf(stderr, "Conexao encerrada pelo servidor.\n");
            break;
        }

        if (msg.type == MSG_ERROR) {
            printf("Insira uma sequência válida!\n");
            continue;
        }

        if (msg.type == MSG_FEEDBACK) {
            imprimir_feedback(&msg);
            continue;
        }

        if (msg.type == MSG_WIN) {
            printf("Acesso concedido! Thaísa recuperou o sistema!\n");
            break;
        }

        fprintf(stderr, "Erro: resposta inesperada do servidor.\n");
        break;
    }

    close(sockfd);
    return 0;
}
