#include "padrao.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

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

void calcular_feedback(const int secret[5], const int guess[5], int feedback[5]) {
    int senha_usada[5] = {0, 0, 0, 0, 0};
    int palpite_processado[5] = {0, 0, 0, 0, 0}; 
    for (int i = 0; i < 5; i++) feedback[i] = 0;
    for (int i = 0; i < 5; i++) {
        if (guess[i] == secret[i]) {
            feedback[i] = 2;
            senha_usada[i] = 1;
            palpite_processado[i] = 1;
        }
    }
    for (int i = 0; i < 5; i++) {
        if (palpite_processado[i]) continue; 

        for (int j = 0; j < 5; j++) {
            if (!senha_usada[j] && guess[i] == secret[j]) {
                feedback[i] = 1;
                senha_usada[j] = 1; 
                break;
            }
        }
    }
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
    fprintf(stderr, "Uso: %s <v4|v6> <porta> <senha>\n", programa);
}

static int porta_valida(const char *entrada, int *porta) {
    char *fim;
    long valor;

    if (entrada == NULL || entrada[0] == '\0') {
        return -1;
    }

    errno = 0;
    valor = strtol(entrada, &fim, 10);

    if (errno != 0 || *fim != '\0' || valor < 1 || valor > 65535) {
        return -1;
    }

    *porta = (int)valor;
    return 0;
}

static int palpite_recebido_valido(const int guess[5]) {
    for (int i = 0; i < 5; i++) {
        if (guess[i] < 0 || guess[i] > 9) {
            return 0;
        }
    }

    return 1;
}

static int enviar_erro(int client_socket, const char *texto) {
    HackerMessage resposta;

    memset(&resposta, 0, sizeof(resposta));
    resposta.type = MSG_ERROR;
    resposta.winstatus = -1;
    snprintf(resposta.message, MSG_SIZE, "%s", texto);

    return envio(client_socket, &resposta, sizeof(resposta));
}

int main() {
    const char *protocolo;
    int porta;
    int senha[5];
    int familia;
    int server_socket;
    int client_socket;
    int contador = 0;
    int opt = 1;

    if (argc != 4) {
        mostrar_uso(argv[0]);
        return 1;
    }

    protocolo = argv[1];
    if (strcmp(protocolo, "v4") != 0 && strcmp(protocolo, "v6") != 0) {
        fprintf(stderr, "Protocolo deve ser v4 ou v6.\n");
        mostrar_uso(argv[0]);
        return 1;
    }

    if (porta_valida(argv[2], &porta) != 0) {
        fprintf(stderr, "Porta incorreta.\n");
        mostrar_uso(argv[0]);
        return 1;
    }

    if (palpite_valido(argv[3], senha) != 0) {
        fprintf(stderr, "Senha deve conter 5 digitos.\n");
        mostrar_uso(argv[0]);
        return 1;
    }

    familia = strcmp(protocolo, "v4") == 0 ? AF_INET : AF_INET6;

    server_socket = socket(familia, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Erro ao criar socket");
        return 1;
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Erro em setsockopt");
        close(server_socket);
        return 1;
    }

    if (familia == AF_INET) {
        struct sockaddr_in endereco4;

        memset(&endereco4, 0, sizeof(endereco4));
        endereco4.sin_family = AF_INET;
        endereco4.sin_port = htons((uint16_t)porta);
        endereco4.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_socket, (struct sockaddr *)&endereco4, sizeof(endereco4)) < 0) {
            perror("Erro no bind");
            close(server_socket);
            return 1;
        }
    } else {
        struct sockaddr_in6 endereco6;

        memset(&endereco6, 0, sizeof(endereco6));
        endereco6.sin6_family = AF_INET6;
        endereco6.sin6_port = htons((uint16_t)porta);
        endereco6.sin6_addr = in6addr_any;

        if (bind(server_socket, (struct sockaddr *)&endereco6, sizeof(endereco6)) < 0) {
            perror("Erro no bind");
            close(server_socket);
            return 1;
        }
    }

    if (listen(server_socket, 1) < 0) {
        perror("Erro no listen");
        close(server_socket);
        return 1;
    }

    if (familia == AF_INET) {
        printf("Servidor iniciado em modo IPv4 na porta %d\n", porta);
    } else {
        printf("Servidor iniciado em modo IPv6 na porta %d\n", porta);
    }

    client_socket = accept(server_socket, NULL, NULL);
    if (client_socket < 0) {
        perror("Erro no accept");
        close(server_socket);
        return 1;
    }

    printf("Cliente conectado\n");

    HackerMessage msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_START;

    if (envio(client_socket, &msg, sizeof(msg)) <= 0) {
        perror("Erro ao enviar mensagem inicial");
        close(client_socket);
        close(server_socket);
        return 1;
    }

    while (1) {
        int recebido = leitura(client_socket, &msg, sizeof(msg));

        if (recebido <= 0) {
            break;
        }

        if (msg.type == MSG_EXIT) {
            break;
        }

        if (msg.type != MSG_GUESS) {
            if (enviar_erro(client_socket, "Insira uma sequência válida!") <= 0) {
                break;
            }
            continue;
        }

        if (!palpite_recebido_valido(msg.guess)) {
            if (enviar_erro(client_socket, "Insira uma sequência válida!") <= 0) {
                break;
            }
            continue;
        }

        enviar_feedback(client_socket, &msg, senha, &contador);
        if (msg.type == MSG_WIN) {
            break;
        }
    }

    close(client_socket);
    printf("Cliente desconectado\n");

    close(server_socket);

    return 0;
}


void enviar_feedback(int client_socket, HackerMessage *msg, const int senha[5], int *contador) {
    int guess_recebido[5];

    for (int i = 0; i < 5; i++) {
        guess_recebido[i] = msg->guess[i];
    }

    memset(msg, 0, sizeof(*msg));
    for (int i = 0; i < 5; i++) {
        msg->guess[i] = guess_recebido[i];
    }

    (*contador)++;
    msg->attempts = *contador;
    calcular_feedback(senha, msg->guess, msg->feedback);
    int acertos = 0;
    for (int i = 0; i < 5; i++) {
        if (msg->feedback[i] == 2) acertos++;
    }
    if (acertos == 5) {
        msg->type = MSG_WIN;      
        msg->winstatus = 1;      
    } else {
        msg->type = MSG_FEEDBACK; 
        msg->winstatus = 0;      
    }
    if (envio(client_socket, msg, sizeof(HackerMessage)) <= 0) {
        perror("Erro ao enviar feedback");
    }
}
