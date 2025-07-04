#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
extern int errno;

void meniu_principal()
{
    printf("\n=== Meniu principal ===\n");
    printf("1. REGISTER <nume> <parola>\n");
    printf("2. LOGIN <nume> <parola>\n");
    printf("3. EXIT\n");
}

void meniu_donatori()
{
    printf("\n=== Meniu donatori ===\n");
    printf("1. ADAUGA_ANUNT <titlu> <cantitate> <valabilitate>\n");
    printf("2. ANULEAZA_ANUNT <titlu>\n");
    printf("3. ACTUALIZEAZA_ANUNT \n");
    printf("4. STERGE_UTILIZATOR\n");
    printf("5. NOUTATI\n");
    printf("6. BACK\n");
    printf("7. EXIT\n");
}

void meniu_beneficiari()
{
    printf("\n=== Meniu beneficiari ===\n");
    printf("1. VIZUALIZEAZA_ANUNTURI\n");
    printf("2. REZERVA_ANUNT <titlu> <cantitate>\n");
    printf("3. VIZUALIZEAZA_REZERVARI\n");
    printf("4. FILTREAZA_ANUNTURI <titlu/cantitate/valabilitate> <valoare>\n");
    printf("5. STERGE_UTILIZATOR\n");
    printf("6. NOUTATI\n");
    printf("7. BACK\n");
    printf("8. EXIT\n");
}

void comunicare_cu_server(int sd)
{
    char buffer[BUFFER_SIZE];
    char raspuns[BUFFER_SIZE];
    int autentificat = 0;
    char tip_utilizator[20] = "";

    printf("=== In ce cont doriti sa va conectati? ===\n");
    printf("1. Donator\n");
    printf("2. Beneficiar\n");
    printf("3. exit\n");
    printf("Introduceti o optiune: ");

    fgets(buffer, BUFFER_SIZE, stdin);
    buffer[strcspn(buffer, "\n")] = '\0';

    if (strcmp(buffer, "1") == 0)
        strcpy(tip_utilizator, "donator");
    else if (strcmp(buffer, "2") == 0)
        strcpy(tip_utilizator, "beneficiar");
    else
    {
        printf("Iesire...\n");
        return;
    }

    while (1)
    {
        if (!autentificat)
            meniu_principal();
        else
        {
            if (strcmp(tip_utilizator, "donator") == 0)
                meniu_donatori();
            else if (strcmp(tip_utilizator, "beneficiar") == 0)
                meniu_beneficiari();
        }

        printf("Introduceti o comanda: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strncmp(buffer, "REGISTER", 8) == 0) // trimitem register nume parola tip_utilizator
        {
            char tip[BUFFER_SIZE];
            snprintf(tip, BUFFER_SIZE, "%s %s", buffer, tip_utilizator);
            strcpy(buffer, tip);
        }

        if (strncmp(buffer, "ACTUALIZEAZA_ANUNT", 18) == 0)
        {
            char titlu_vechi[50], camp[20], valoare[50];

            printf("Introduceti titlul produsului de actualizat: ");
            fgets(titlu_vechi, sizeof(titlu_vechi), stdin);
            titlu_vechi[strcspn(titlu_vechi, "\n")] = '\0';

            printf("Ce doriti sa modificati? (titlu/cantitate/valabilitate): ");
            fgets(camp, sizeof(camp), stdin);
            camp[strcspn(camp, "\n")] = '\0';

            if (strcmp(camp, "titlu") != 0 && strcmp(camp, "cantitate") != 0 && strcmp(camp, "valabilitate") != 0)
            {
                printf("Camp invalid! Trebuie să fie una dintre: titlu, cantitate, valabilitate.\n");
                return;
            }

            printf("Introduceti noua valoare: ");
            fgets(valoare, sizeof(valoare), stdin);
            valoare[strcspn(valoare, "\n")] = '\0';

            snprintf(buffer, BUFFER_SIZE, "ACTUALIZEAZA_ANUNT %s %s %s", titlu_vechi, camp, valoare);
        }

        if (strcmp(buffer, "BACK") == 0)
        {
            autentificat = 0;
            continue;
        }

        if (strncmp(buffer, "EXIT", 4) == 0)
        {
            printf("Iesire...\n");
            break;
        }

        send(sd, buffer, strlen(buffer), 0);
        int bytes_received = recv(sd, raspuns, BUFFER_SIZE, 0);
        raspuns[bytes_received] = '\0';
        printf("Raspunsul de la server: \n\n%s\n", raspuns);

        if (strstr(raspuns, "Te-ai logat cu succes ca"))
        {
            autentificat = 1;
            sscanf(raspuns, "Te-ai logat cu succes ca %s", tip_utilizator);
        }
    }
}

int main()
{
    int sd;
    struct sockaddr_in server;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[client] Eroare la socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;

    if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Eroare la connect().\n");
        return errno;
    }

    comunicare_cu_server(sd);
    close(sd);

    return 0;
}