// Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
// João Maria Pereira Carvalho de Picado Santos 2022213725

#include <stdio.h>
#include <stdlib.h>

// #define DEBUG //remove this line to remove debug messages

#define BACK_PIPE "/tmp/BACK_PIPE"

int main(int argc, char *argv[]) {
    int console_id = 1;
    char input[50];

    while (1) {
        printf("> 1#");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';
    }
    exit(0);
}
