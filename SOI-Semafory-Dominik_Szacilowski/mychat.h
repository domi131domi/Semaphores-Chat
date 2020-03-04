#ifndef MYCHAT_H
#define MYCHAT_H
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>

#include <pthread.h>

#define TESTS
#define MAX_MESSAGES 5
#define MAX_LETTERS 50
#define NAME "MyChatSharedMemory"
#define SIZE sizeof(struct Memory)
#define MAX_USERS 4
#define USER 0
#define VIP 1
#define ADMIN 2
#define EMPTY 3
#define EMPTY_TEXT "$#$#$"
#define ADMIN_MESS "Wiadomosc usunieta przez administratora."
#define BAD_WORD "ZLE SLOWO"

typedef struct User {
    int pid;
    unsigned who;
} User;

typedef struct Message {
    char text[MAX_LETTERS];
    User user;
} Message;

typedef struct Memory {
    Message buffor[MAX_MESSAGES];
    sem_t full; //sem do kontroli przepelnienia
    sem_t empty; //sem do kontroli odczytu z pustego buff
    sem_t mutex; //zamek dostepu do buffora

    sem_t read[MAX_USERS]; //sem do powiadomieniu uzytkownika o wiadomosci
    sem_t fullUsers; //sem do kotroli przepelnienia liczby uzytkownikow
    sem_t emptyUsers; //sem do wstrzymania pracy readera w przypadku braku uzytkownikow
    unsigned numberOfUsers;
    User users[MAX_USERS];  //lista uzytwkownikow
    sem_t waitToDel[MAX_USERS]; //sem do wstrzymania kasowania wiadomosci do momentu odczytania jej przez wszystkich uzytkownikow


}Memory;

//wyczyszczenie calego buffora
void cleanBuffor(Memory* data) {
    int i = 0;
    for(i; i < MAX_MESSAGES; i++)
        strcpy(data->buffor[i].text, EMPTY_TEXT);
}

//wyczysczenie buffora przez admina (zamiana na ADMIN_MESS w przypadku BAD_WORD)
void cleanBuffAdmin( Memory* data ) {
    int i = 0;
    for(i; i < MAX_MESSAGES; i++) {
        if(strcmp(data->buffor[i].text, EMPTY_TEXT) != 0)
            strcpy(data->buffor[i].text, ADMIN_MESS);
    }
}

void initvalues( Memory* data ) {
    data->numberOfUsers = 0;
}

//znalezienie numeru uzytkownika po jego numerze pid
int findUser( Memory* data, User user ) {
    int i = 0;
    for( i; i < data->numberOfUsers; i++)
        if(user.pid == data->users[i].pid && user.who == data->users[i].who)
            return i;
    return -1;
}

//dodanie wiadomosci do buffora ( utrzymuje porzadek w jakiej zostaja dodane wiadomosci oraz wyjatek
// VIP ustaw przed USEREM )
void addMessage( Memory* data, User user, char* const message ) {
    int current = 0;
    if(user.who == VIP) {
    while(data->buffor[current].user.who == VIP && strcmp(data->buffor[current].text,EMPTY_TEXT) != 0)
        current ++;

    if(strcmp(data->buffor[current].text,EMPTY_TEXT) != 0) {
    int i = MAX_MESSAGES;
    for( i; i > current; i--) {
        strcpy(data->buffor[i].text,data->buffor[i-1].text);
        data->buffor[i].user.pid = data->buffor[i-1].user.pid;
        data->buffor[i].user.who = data->buffor[i-1].user.who;
    }
    }
    strcpy(data->buffor[current].text,message);
    data->buffor[current].user.pid = user.pid;
    data->buffor[current].user.who = user.who;
    }
    else {
    int last = MAX_MESSAGES - 1;
    while( last >= 0 && strcmp(data->buffor[last].text,EMPTY_TEXT) == 0)
        last--;
    strcpy(data->buffor[last+1].text,message);
    data->buffor[last+1].user.pid = user.pid;
    data->buffor[last+1].user.who = user.who;
    }
}

//usuniecie pierwszej wiadomosci w bufforze
void deleteFirstMessage( Memory* data ) {
    int i = 0;
    for( i; i < MAX_MESSAGES - 1; i++) {
        strcpy(data->buffor[i].text,data->buffor[i+1].text);
        data->buffor[i].user.pid = data->buffor[i+1].user.pid;
        data->buffor[i].user.who = data->buffor[i+1].user.who;
    }
    strcpy(data->buffor[MAX_MESSAGES-1].text,EMPTY_TEXT);
}

//reader to funkcja, ktora czeka na wiadomosc w bufforze, a nastepnie wysyla ja
//wszystkim uzytkownikom, czeka az wszyscy ja odbiora, by na samym koncu usunac ja z buffora
//reader nie wysyla wiadomosci do jej nadawcy
//reader wysyla wiadomosc najpierw do adminow w celu weryfikacji, a potem do userow i vipow
void reader( Memory* data ) {
    sem_wait(&data->emptyUsers);
    #ifdef TESTS
    printf("Reader czeka na wiadomosc...\n");
    #endif
    sem_wait(&data->empty);
    #ifdef TESTS
    printf("Reader dostal wiadomosc\n");
    #endif
    sem_wait(&data->mutex);
    if(strcmp(data->buffor[0].text, EMPTY_TEXT) != 0) {
    int i = 0;
    for( i; i < data->numberOfUsers; i++)
        if(data->users[i].who == ADMIN) {
            if(data->users[i].pid != data->buffor[0].user.pid) {
            sem_post(&data->read[i]);
            sem_wait(&data->waitToDel[i]);
            }
        }
    i = 0;
    for( i; i < data->numberOfUsers; i++) {
        if(data->users[i].who != ADMIN) {
            if(data->users[i].pid != data->buffor[0].user.pid) {
            sem_post(&data->read[i]);
            sem_wait(&data->waitToDel[i]);
            }
        }
    }
    }
    #ifdef TESTS
    printf("Wiadomosc o tresci: %s od uzytkownika o pid: %d zostala rozeslana i usunieta z bufora\n",data->buffor[0].text,data->buffor[0].user.pid);
    #endif
    deleteFirstMessage(data);
    sem_post(&data->mutex);
    sem_post(&data->full);
    sem_post(&data->emptyUsers);
}

//funkcja odbioru wiadomosci przez usera i vipa
//czeka na wiadomosc od readera, wypisuje ja i zwieksza semafor zwrotny do readera w celu zaznaczania o jej przeczytaniu
void gotMessage( Memory* data, User user ) {
       int numberOfUser = findUser(data,user);
       sem_wait(&data->read[numberOfUser]);
        #ifdef TESTS
        printf("Wiadomosc widoczna na konsoli uzytkownika/vipa o pid: %d : ",user.pid);
        #endif
       printf("(%d):%s\n",data->buffor[0].user.pid,data->buffor[0].text);
       sem_post(&data->waitToDel[numberOfUser]);
}

//analogiczne do gotMessage, ale po odczytaniu BAD_WORD czysci wiadomosci w bufforze
void gotMessageAdmin( Memory* data, User user ) {
    int numberOfUser = findUser(data,user);
    sem_wait(&data->read[numberOfUser]);
    #ifdef TESTS
    printf("Wiadomosc widoczna na konsoli admina o pid: %d : ",user.pid);
    #endif
    printf("(%d):%s\n",data->buffor[0].user.pid,data->buffor[0].text);
    if(strcmp(data->buffor[0].text, BAD_WORD) == 0)
        cleanBuffAdmin(data);
    sem_post(&data->waitToDel[numberOfUser]);
}

//wypisuje buffor na konsole
void showBuffor( Memory* data ) {
    int i = 0;
    for( i; i < MAX_MESSAGES; i++)
        printf("%d . %s\n",i,data->buffor[i].text);
}

//wyslanie wiadomosci przez uzytkownika
void sendMessage( Memory* data, User user, char* const message ) {
    sem_wait(&data->full);
    sem_wait(&data->mutex);
    addMessage(data,user,message);
    #ifdef TESTS
    printf("Wiadomosc o tresci: '%s' od uzytkownika o pid: %d zostala dodana do bufora\n",message,user.pid);
    #endif
    sem_post(&data->mutex);
    sem_post(&data->empty);
}

//ustawienie poczatkowych wartosci
void cleanUsers( Memory* data ) {
    User cl;
    cl.pid = -1;
    int i = 0;
    for( i; i < MAX_USERS; i++)
        data->users[i] = cl;
}

//ustawienie poczatkowych wartosci
int initUser(Memory* data, User user ) {
    sem_wait(&data->fullUsers);
    sem_wait(&data->mutex);
    data->users[data->numberOfUsers] = user;
    data->numberOfUsers++;
    #ifdef TESTS
    printf("Dodano uzytkownika o pid: %d i pozycji: %d (0 - USER, 1 - VIP, 2 - ADMIN)\n",user.pid,user.who);
    #endif
    sem_post(&data->mutex);
    sem_post(&data->emptyUsers);
    return (data->numberOfUsers) - 1;
}

//ustawienie poczatkowych wartosci
void initSemaphores(Memory* data) {
    sem_init(&data->full,1,MAX_MESSAGES);
    sem_init(&data->empty,1,0);
    sem_init(&data->fullUsers,1,MAX_USERS);
    sem_init(&data->mutex,1,1);
    sem_init(&data->emptyUsers,1,0);
    int i = 0;
    for( i; i < MAX_USERS; i++) {
        sem_init(&data->waitToDel[i],1,0);
        sem_init(&data->read[i],1,0);
    }
}

#endif  /* MYCHAT */
