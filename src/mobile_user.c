// Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
// João Maria Pereira Carvalho de Picado Santos 2022213725

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    // check if the number of parameters is correct
    if (argc != 7)
    {
        printf("Error: Wrong parameters, use ./mobile_user {plafond inicial} {número de pedidos de autorização} {intervalo VIDEO} {intervalo MUSIC} {intervalo SOCIAL} {dados a reservar}\n");
        exit(1);
    }

    return 0;
}
