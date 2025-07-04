#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <sqlite3.h>
#include <sodium.h>

#define PORT 8080
#define BUFFER_SIZE 4096
extern int errno;


void creare_bd()
{
    sqlite3 *db;
    char *err_msg = NULL;

    //verif daca exista deja bd
    if (access("baza_de_date.db", F_OK) == 0)
    {
        printf("Baza de date exista\n");
        return;
    }

    sqlite3_open("baza_de_date.db", &db);

    const char *interogare = 
        "CREATE TABLE IF NOT EXISTS beneficiari ("
        "    nume TEXT NOT NULL UNIQUE,"
        "    parola TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS donatori ("
        "    nume TEXT NOT NULL UNIQUE,"
        "    parola TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS anunturi ("
        "    nume TEXT NOT NULL,"
        "    produs TEXT NOT NULL,"
        "    cantitate INTEGER NOT NULL,"
        "    data_expirare DATE NOT NULL,"
        "    data_creare DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS rezervari ("
        "    nume TEXT NOT NULL,"
        "    produs TEXT NOT NULL,"
        "    cantitate INTEGER NOT NULL"
        ");";


    int rc = sqlite3_exec(db, interogare, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Eroare la crearea tabeleleor: %s\n", err_msg);
    }
    else
        printf("Baza de date a fost creata\n");

    sqlite3_close(db);
}

void sterge_anunturi_expirate()
{

    sqlite3 *db;
    sqlite3_stmt *stmt;

    sqlite3_open("baza_de_date.db", &db);

    char *interogare = "DELETE FROM anunturi WHERE data_expirare < date('now')";

    sqlite3_prepare_v2(db, interogare, -1, &stmt, 0);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    printf("Am sters anuturle expirate\n");
}

int data_expirata(const char *data_expirare)
{
    struct tm tm_expirare = {0};
    time_t timp_expirare, timp_curent;
    char data_crt[11]; // YYYY-MM-DD

    time(&timp_curent); // data curenta

    strftime(data_crt, sizeof(data_crt), "%Y-%m-%d", localtime(&timp_curent));

    if (strptime(data_expirare, "%Y-%m-%d", &tm_expirare) == NULL)
        return 1; // invalid

    timp_expirare = mktime(&tm_expirare);

    if (difftime(timp_curent, timp_expirare) > 0)
        return 1; // invalid
    return 0;
}

int autentificare(char *nume, char *parola, char *tip_utilizator)
{
    if (sodium_init() < 0)
    {
        return 0;
    }

    sqlite3 *db;
    sqlite3_stmt *stmt;

    sqlite3_open("baza_de_date.db", &db);

    char *interogare =
        "SELECT 'donator' AS tip, parola FROM donatori WHERE nume = ? "
        "UNION "
        "SELECT 'beneficiar' AS tip, parola FROM beneficiari WHERE nume = ?;";

    sqlite3_prepare_v2(db, interogare, -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, nume, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, nume, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char *hash = (const char *)sqlite3_column_text(stmt, 1);

        if (crypto_pwhash_str_verify(hash, parola, strlen(parola)) == 0)
        {
            strcpy(tip_utilizator, (const char *)sqlite3_column_text(stmt, 0));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return 1;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return 0;
}

void inregistrare(char *nume, char *parola, char *tip_utilizator, char *raspuns)
{
    if (sodium_init() < 0)
    {
        return;
    }

    sqlite3 *db;
    sqlite3_stmt *stmt;
    char hash[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(hash, parola, strlen(parola), crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
    {
        return;
    }

    sqlite3_open("baza_de_date.db", &db);

    char *interogare;
    if (strcmp(tip_utilizator, "donator") == 0)
        interogare = "INSERT INTO donatori (nume, parola) VALUES (?,?);";
    else
        interogare = "INSERT INTO beneficiari (nume,parola) VALUES (?,?);";

    sqlite3_prepare_v2(db, interogare, -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, nume, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE)
        strcpy(raspuns, "Inregistrare realizata cu succes!");
    else
        strcpy(raspuns, "Eroare la inregistrare!");

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

//-------------------------------------------FUNCTII PENTRU DONATORI----------------------------------------------------------------

void fct_adauga_anunt(char *nume_donator, char *detalii, char *raspuns)
{
    char titlu[50];
    char cantitate[50];
    char valabilitate[50];

    sscanf(detalii, "%49s %49s %49s", titlu, cantitate, valabilitate);

    if (strlen(titlu) == 0 || strlen(cantitate) == 0 || strlen(valabilitate) == 0)
    {

        strcpy(raspuns, "\033[31mNu ai adaugat un titlu, o cantitate sau o valabilitate valida!\033[0m");
        return;
    }

    int val = atoi(cantitate);

    if (val <= 0)
    {
        strcpy(raspuns, "\033[31mNu ai adaugat o cantitate valida!\033[0m");
        return;
    }

    if (data_expirata(valabilitate))
    {
        strcpy(raspuns, "\033[31mData de expirare este invalida sau produsul e deja expiarat!\033[0m");
        return;
    }

    sqlite3 *db;
    sqlite3_stmt *stmt;

    sqlite3_open("baza_de_date.db", &db);
    char *interogare = "INSERT INTO anunturi (nume,produs,cantitate,data_expirare,data_creare) VALUES (?,?,?,?,datetime('now'));";

    sqlite3_prepare_v2(db, interogare, -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, nume_donator, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, titlu, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, val);
    sqlite3_bind_text(stmt, 4, valabilitate, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE)
        strcpy(raspuns, "\033[32mAnunt adaugat cu succes!\033[0m");
    else
        strcpy(raspuns, "\033[31mEroare la adaugarea anuntului!\033[0m");

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void fct_anulare_anunt(char *nume_donator, char *titlu, char *raspuns)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;

    sqlite3_open("baza_de_date.db", &db);

    char *interogare1 = "SELECT * FROM anunturi WHERE TRIM(nume)= ? AND TRIM(produs)=?;";
    // aici verific daca exista anuntul mai intai
    sqlite3_prepare_v2(db, interogare1, -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, nume_donator, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, titlu, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW)
    {
        strcpy(raspuns, "\033[31mAnuntul nu exista in baza de date!\033[0m");
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return;
    }
    sqlite3_finalize(stmt);

    char *interogare2 = "DELETE FROM anunturi WHERE TRIM(NUME)=? AND TRIM(produs)=?;";
    // aici il sterg
    sqlite3_prepare_v2(db, interogare2, -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, nume_donator, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, titlu, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        if (sqlite3_changes(db) > 0)
            strcpy(raspuns, "\033[32mAnuntul a fost sters!\033[0m");
        else
            strcpy(raspuns, "\033[31mAnuntul nu a fost gasit sau nu va apartine!\033[0m");
    }
    else
        strcpy(raspuns, "\033[31mEroare la stergere\033[0m");

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void fct_noutati_donatori(char *raspuns, char *nume_donator)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;

    time_t timp_curent;
    struct tm tm_anunt = {0};
    time(&timp_curent);

    sqlite3_open("baza_de_date.db", &db);

    char *interogare = "SELECT produs, cantitate, data_expirare FROM anunturi WHERE nume = ?;";
    sqlite3_prepare_v2(db, interogare, -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, nume_donator, -1, SQLITE_STATIC);
    strcpy(raspuns, "\033[34mAnunturile tale care expira in urmatoarele 24h:\n\033[0m");

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *produs = (char *)sqlite3_column_text(stmt, 0);
        int cantitate = sqlite3_column_int(stmt, 1);
        char *data_expirare = (char *)sqlite3_column_text(stmt, 2);

        if (strptime(data_expirare, "%Y-%m-%d", &tm_anunt) != NULL)
        {
            time_t timp_anunt = mktime(&tm_anunt);
            if (timp_anunt != -1 && difftime(timp_anunt, timp_curent) <= 86400 && difftime(timp_anunt, timp_curent) >= 0)
            {
                char msj[BUFFER_SIZE];
                snprintf(msj, sizeof(msj), "Produs: %s, Cantitate: %d, Expira la: %s\n", produs, cantitate, data_expirare);
                strncat(raspuns, msj, BUFFER_SIZE - strlen(raspuns) - 1);
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void fct_actualizeaza_anunt(char *nume_donator, char *titlu_vechi, char *camp, char *valoare, char *raspuns)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;

    sqlite3_open("baza_de_date.db", &db);

    char interogare[256];
    snprintf(interogare, sizeof(interogare), "UPDATE anunturi SET %s=? WHERE nume=? AND produs=?;", camp);

    sqlite3_prepare_v2(db, interogare, -1, &stmt, NULL);

    sqlite3_bind_text(stmt, 1, valoare, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, nume_donator, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, titlu_vechi, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_DONE)
    {
        if (sqlite3_changes(db) > 0)
            strcpy(raspuns, "\033[32mAnuntul a fost actualizat!\033[0m");
        else
            strcpy(raspuns, "\033[31mAnuntul nu a fost gasit sau nu va apartine!\033[0m");
    }
    else
        strcpy(raspuns, "\033[31mEroare la actualziare\033[0m");

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

//-------------------------------------------------------FUNCTII PENTRU BENEFICIARI-------------------------------------------------

void fct_vizualizare_anunturi(char *raspuns)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;

    sqlite3_open("baza_de_date.db", &db);

    char *interogare = "SELECT * FROM anunturi;";
    sqlite3_prepare_v2(db, interogare, -1, &stmt, 0);

    int ok = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (ok == 0)
        {
            strcpy(raspuns, "Anunturi: \n");
            ok = 1; // am gasit anunturi
        }
        char linie[BUFFER_SIZE];
        snprintf(linie, sizeof(linie), "%s %s %d %s\n", sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2), sqlite3_column_text(stmt, 3));
        strcat(raspuns, linie);
    }

    if (ok == 0)
        strcpy(raspuns, "\033[31mNu exista anunturi disponibile!\033[0m");

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void fct_rezervare(char *detalii, char *beneficiar, char *raspuns)
{
    sqlite3 *db;
    sqlite3_stmt *stmt1, *stmt2, *stmt3, *stmt4;

    sqlite3_open("baza_de_date.db", &db);

    char titlu[50];
    int cantitate_rezervata;
    sscanf(detalii, "%s %d", titlu, &cantitate_rezervata);

    if (cantitate_rezervata <= 0)
    {
        strcpy(raspuns, "\033[31mNu ai introdus o cantitate valida!\033[0m");
        sqlite3_close(db);
        return;
    }

    char *interogare1 = "SELECT cantitate FROM anunturi WHERE produs=?;";
    sqlite3_prepare_v2(db, interogare1, -1, &stmt1, 0);

    sqlite3_bind_text(stmt1, 1, titlu, -1, SQLITE_STATIC);

    int cantitate_disponibila = 0;
    if (sqlite3_step(stmt1) == SQLITE_ROW)
        cantitate_disponibila = sqlite3_column_int(stmt1, 0);
    sqlite3_finalize(stmt1);

    if (cantitate_disponibila < cantitate_rezervata)
    {
        strcpy(raspuns, "\033[31mRezervarea nu se poate realiza!\033[0m");
        sqlite3_close(db);
        return;
    }

    char *interogare2 = "UPDATE anunturi SET cantitate=cantitate-? WHERE produs=?;";
    sqlite3_prepare_v2(db, interogare2, -1, &stmt2, 0);

    sqlite3_bind_int(stmt2, 1, cantitate_rezervata);
    sqlite3_bind_text(stmt2, 2, titlu, -1, SQLITE_STATIC);

    sqlite3_step(stmt2);
    sqlite3_finalize(stmt2);

    char *interogare4 = "DELETE FROM anunturi WHERE produs=? AND cantitate=0;";
    sqlite3_prepare_v2(db, interogare4, -1, &stmt4, 0);

    sqlite3_bind_text(stmt4, 1, titlu, -1, SQLITE_STATIC);

    sqlite3_step(stmt4);
    sqlite3_finalize(stmt4);

    char *interogare3 = "INSERT INTO rezervari (nume,produs,cantitate) VALUES (?,?,?);";
    sqlite3_prepare_v2(db, interogare3, -1, &stmt3, 0);

    sqlite3_bind_text(stmt3, 1, beneficiar, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt3, 2, titlu, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt3, 3, cantitate_rezervata);

    sqlite3_step(stmt3);
    sqlite3_finalize(stmt3);

    strcpy(raspuns, "\033[32mRezervare realizata cu succes!\033[0m");
    sqlite3_close(db);
}

void fct_vizualizare_rezervari(char *beneficiar, char *raspuns)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;

    sqlite3_open("baza_de_date.db", &db);

    char *interogare = "SELECT produs,cantitate FROM rezervari WHERE nume=?;";

    sqlite3_prepare_v2(db, interogare, -1, &stmt, 0);

    sqlite3_bind_text(stmt, 1, beneficiar, -1, SQLITE_STATIC);

    int ok = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (ok == 0)
        {
            strcpy(raspuns, "Rezervarile dvs:\n");
            ok = 1;
        }
        char linie[BUFFER_SIZE];
        snprintf(linie, sizeof(linie), "Produs: %s, Cantitate: %d\n", sqlite3_column_text(stmt, 0), sqlite3_column_int(stmt, 1));
        strcat(raspuns, linie);
    }

    if (ok == 0)
        strcpy(raspuns, "\033[31mNu aveti nicio rezervare\033[0m");

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void fct_filtrare_anunturi(char *criteriu, char *valoare, char *raspuns)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;

    sqlite3_open("baza_de_date.db", &db);

    char *interogare = NULL;

    if (strcmp(criteriu, "titlu") == 0)
        interogare = "SELECT * FROM anunturi WHERE produs LIKE ?;";
    else if (strcmp(criteriu, "cantitate") == 0)
        interogare = "SELECT * FROM anunturi WHERE cantitate >=?;";
    else if (strcmp(criteriu, "data") == 0)
        interogare = "SELECT * FROM anunturi WHERE data_expirare<=?;";
    else
    {
        strcpy(raspuns, "\033[31mCriteriu invalid! Criteriul poate fi: titlu/cantitate/valabilitate!\033[0m");
        sqlite3_close(db);
        return;
    }

    sqlite3_prepare_v2(db, interogare, -1, &stmt, 0);

    if (strcmp(criteriu, "titlu") == 0)
    {
        char s[4096];
        snprintf(s, sizeof(s), "%%%s%%", valoare);
        sqlite3_bind_text(stmt, 1, s, -1, SQLITE_TRANSIENT);
    }
    else if (strcmp(criteriu, "cantitate") == 0)
        sqlite3_bind_int(stmt, 1, atoi(valoare));
    else if (strcmp(criteriu, "data") == 0)
        sqlite3_bind_text(stmt, 1, valoare, -1, SQLITE_STATIC);

    int ok = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        if (ok == 0)
        {
            strcpy(raspuns, "Anunturile dupa filtrare: \n");
            ok = 1;
        }
        char linie[BUFFER_SIZE];
        snprintf(linie, sizeof(linie), "%s %s %d %s\n", sqlite3_column_text(stmt, 0), sqlite3_column_text(stmt, 1), sqlite3_column_int(stmt, 2), sqlite3_column_text(stmt, 3));
        strcat(raspuns, linie);
    }

    if (ok == 0)
        strcpy(raspuns, "\033[31mNu am gasit anunturi cu aceste filtre!\033[0m");

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void fct_noutati_beneficiari(char *raspuns)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;

    strcpy(raspuns, "\033[34mAnunturile adaugate in ultimele 48h: \n\033[0m");
    char *interogare = "SELECT produs,cantitate,data_expirare FROM anunturi WHERE datetime(data_creare)>=datetime('now','-2 days')";

    sqlite3_open("baza_de_date.db", &db);
    sqlite3_prepare_v2(db, interogare, -1, &stmt, NULL);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        char *produs = (char *)sqlite3_column_text(stmt, 0);
        int cantitate = sqlite3_column_int(stmt, 1);
        char *data_expirare = (char *)sqlite3_column_text(stmt, 2);

        char anunt[256];
        snprintf(anunt, sizeof(anunt), "Produs: %s, Cantitate: %d, Valabilitate: %s\n", produs, cantitate, data_expirare);
        strcat(raspuns, anunt);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void fct_sterge_utilizator(char *nume, char *tip_utilizator, char *raspuns)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char *errmsg = 0;

    sqlite3_open("baza_de_date.db", &db);
    char interogare[256];
    if (strcmp(tip_utilizator, "donator") == 0)
    {
        sprintf(interogare, "DELETE FROM donatori WHERE nume='%s';", nume);
        sqlite3_prepare_v2(db, interogare, -1, &stmt, 0);

        sqlite3_exec(db, interogare, 0, 0, &errmsg);

        sprintf(interogare, "DELETE FROM anunturi WHERE nume='%s';", nume);
        sqlite3_exec(db, interogare, 0, 0, &errmsg);
    }
    else
    {
        sprintf(interogare, "DELETE FROM beneficiari WHERE nume='%s';", nume);
        sqlite3_exec(db, interogare, 0, 0, &errmsg);
    }

    strcpy(raspuns, "\033[32mCont sters cu succes!\033[0m");
    sqlite3_close(db);
}

// --------------------------------------------------------FUNCTIA DE PROCESARE A COMENZILOR DE LA CLIENT--------------------------------------------------

void procesare_comanda(char *comanda, char *raspuns, int client_socket)
{
    char nume[50];
    char parola[50];
    char tip_utilizator[20];
    char nume_beneficiar[50];
    char nume_donator[50];
    char detalii[BUFFER_SIZE];

    if (strncmp(comanda, "LOGIN", 5) == 0)
    {
        sscanf(comanda + 6, "%s %s", nume, parola);
        if (autentificare(nume, parola, tip_utilizator) != 0)
        {
            snprintf(raspuns, BUFFER_SIZE, "\033[0;32mTe-ai logat cu succes ca %s\033[0m", tip_utilizator);
            if (strcmp(tip_utilizator, "beneficiar") == 0)
                strcpy(nume_beneficiar, nume);
            if (strcmp(tip_utilizator, "donator") == 0)
                strcpy(nume_donator, nume);
        }
        else
            strcpy(raspuns, "\033[31mDatele introduse sunt invalide!\033[0m");
    }
    else if (strncmp(comanda, "REGISTER", 8) == 0)
    {
        sscanf(comanda + 9, "%s %s %s", nume, parola, tip_utilizator);
        inregistrare(nume, parola, tip_utilizator, raspuns);
    }
    else if (strncmp(comanda, "ADAUGA_ANUNT", 12) == 0)
    {
        sscanf(comanda + 13, "%[^\n]", detalii);
        fct_adauga_anunt(nume_donator, detalii, raspuns);
    }
    else if (strncmp(comanda, "ANULEAZA_ANUNT", 14) == 0)
    {
        char titlu[50];
        sscanf(comanda + 15, "%s", titlu);
        fct_anulare_anunt(nume_donator, titlu, raspuns);
    }
    else if (strncmp(comanda, "VIZUALIZEAZA_ANUNTURI", 21) == 0)
    {
        fct_vizualizare_anunturi(raspuns);
    }
    else if (strncmp(comanda, "REZERVA_ANUNT", 13) == 0)
    {
        sscanf(comanda + 14, "%[^\n]", detalii);
        fct_rezervare(detalii, nume, raspuns);
    }
    else if (strncmp(comanda, "VIZUALIZEAZA_REZERVARI", 22) == 0)
    {
        fct_vizualizare_rezervari(nume, raspuns);
    }
    else if (strncmp(comanda, "STERGE_UTILIZATOR", 17) == 0)
    {
        char nume_conectat[50] = "";
        if (strcmp(tip_utilizator, "donator") == 0)
            strcpy(nume_conectat, nume_donator);
        else if (strcmp(tip_utilizator, "beneficiar") == 0)
            strcpy(nume_conectat, nume_beneficiar);

        fct_sterge_utilizator(nume_conectat, tip_utilizator, raspuns);
    }
    else if (strncmp(comanda, "FILTREAZA_ANUNTURI", 18) == 0)
    {
        char criteriu[20], valoare[50];
        sscanf(comanda + 19, "%s %s", criteriu, valoare);
        fct_filtrare_anunturi(criteriu, valoare, raspuns);
    }
    else if (strncmp(comanda, "ACTUALIZEAZA_ANUNT", 18) == 0)
    {
        char titlu_vechi[50];
        char camp[20];
        char val[50];
        sscanf(comanda + 19, "%s %s %s", titlu_vechi, camp, val);

        if (strcmp(camp, "titlu") == 0)
            strcpy(camp, "produs");
        else if (strcmp(camp, "cantitate") == 0)
            strcpy(camp, "cantitate");
        else if (strcmp(camp, "valabilitate") == 0)
            strcpy(camp, "data_expirare");
        fct_actualizeaza_anunt(nume_donator, titlu_vechi, camp, val, raspuns);
    }
    else if (strcmp(comanda, "NOUTATI") == 0)
    {
        if (strcmp(tip_utilizator, "beneficiar") == 0)
        {
            fct_noutati_beneficiari(raspuns);
        }
        else if (strcmp(tip_utilizator, "donator") == 0)
        {
            fct_noutati_donatori(raspuns, nume);
        }
    }
    else
        strcpy(raspuns, "\033[31mComanda nerecunoscuta!\033[0m");
}

void *treat(void *arg)
{
    int client = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int read;

    while ((read = recv(client, buffer, sizeof(buffer), 0)) > 0)
    {
        buffer[read] = '\0';
        printf("Primit: %s\n", buffer);

        char rasp[BUFFER_SIZE];
        procesare_comanda(buffer, rasp, client);
        send(client, rasp, strlen(rasp), 0);
    }

    close(client);
    pthread_exit(NULL);
}

int main()
{
    creare_bd(); //creez baza de date daca nu exista 
    sterge_anunturi_expirate();
    int sd, *client;
    struct sockaddr_in server;
    struct sockaddr_in from;
    pthread_t th;
    socklen_t length = sizeof(from);

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server] Eroare la socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server] Eroare la bind().\n");
        return errno;
    }

    if (listen(sd, 5) == -1)
    {
        perror("[server] Eroare la listen().\n");
        return errno;
    }

    printf("[server] Asteptam la portul %d...\n", PORT);

    while (1)
    {
        client = malloc(sizeof(int));
        *client = accept(sd, (struct sockaddr *)&from, &length);

        pthread_create(&th, NULL, treat, client);
        pthread_detach(th);
    }

    close(sd);
    return 0;
}