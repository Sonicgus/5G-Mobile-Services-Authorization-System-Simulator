// Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
// João Maria Pereira Carvalho de Picado Santos 2022213725

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

// #define DEBUG //remove this line to remove debug messages

#define USER_PIPE "/tmp/USER_PIPE"

pid_t pid;

int main(int argc, char *argv[]) {
    // check if the number of parameters is correct
    if (argc != 7) {
        printf("Error: Wrong parameters, use ./mobile_user {plafond inicial} {número de pedidos de autorização} {intervalo VIDEO} {intervalo MUSIC} {intervalo SOCIAL} {dados a reservar}\n");
        exit(1);
    }

    // check if the parameters are numbers
    for (int i = 1; i < 7; i++) {
        for (int j = 0; j < strlen(argv[i]); j++) {
            if (!isdigit(argv[i][j])) {
                printf("Error: invalid paramter %d, must be a number\n", i);
                exit(1);
            }
        }
    }

    int plafond = atoi(argv[1]), n_pedidos = atoi(argv[2]), intervalo_video = atoi(argv[3]), intervalo_musica = atoi(argv[4]), intervalo_social = atoi(argv[5]), dados = atoi(argv[6]);

    pid = getpid();
    printf("Mobile user pid: %d\n", pid);

    if (plafond < 0) {
        printf("Error: Plafond inicial must be a positive number\n");
        exit(1);
    }
    if (n_pedidos < 0) {
        printf("Error: Número de pedidos de autorização must be a positive number\n");
        exit(1);
    }
    if (intervalo_video < 0) {
        printf("Error: Intervalo VIDEO must be a positive number\n");
        exit(1);
    }
    if (intervalo_musica < 0) {
        printf("Error: Intervalo MUSIC must be a positive number\n");
        exit(1);
    }
    if (intervalo_social < 0) {
        printf("Error: Intervalo SOCIAL must be a positive number\n");
        exit(1);
    }
    if (dados < 0) {
        printf("Error: Dados a reservar must be a positive number\n");
        exit(1);
    }

    exit(0);
}
