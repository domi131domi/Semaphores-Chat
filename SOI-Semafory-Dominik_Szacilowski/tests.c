#include "mychat.h"

//testy przedstawiaja przykladowe dzialanie programu oraz sytuacje brzegowe
//bilbioteka names.h jest jednak stworzona do dzialania w wielu procesach
//jest to jednak ciezkie do przetestowania w czytelny sposob
//dlatego w testach zostaly zastosowane watki
//struktura potrzebna na rzecz testow do przeslania argumentow do innego watku
struct Args {
    Memory* data;
    User user;
};

void* readerth(void* args) {
    struct Args* arg = (struct Args*)args;
    int i;
    while(1)
    reader(arg->data);
    return NULL;
}

void* inituserth(void* args) {
    struct Args* arg = (struct Args*)args;
    int i;
    initUser(arg->data,arg->user);
    return NULL;
}

int main() {

    int fd = shm_open(NAME, O_CREAT | O_EXCL | O_RDWR, 0600);   //otworz, stworz shared memory
    if (fd < 0) {
        perror("Chat was already initialised. Last time chat was probably closed by killing process. I closed it. Now you can try again.\n");
        shm_unlink(NAME);
        return EXIT_FAILURE;
      }
    ftruncate(fd, SIZE);
    Memory* data = NULL;
    data = (Memory *)mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(data == NULL) {
        perror("Cannot map memory.\n");
        shm_unlink(NAME);
        close(fd);
        return EXIT_FAILURE;
    }

    struct Args *arg = (struct Args *)malloc(sizeof(struct Args));
    //watki na potrzeby symulacji roznych procesow
    pthread_t tid;
    int whichTest = 0;

    while( whichTest != 5 ) {
        printf("\n////////////////////////////////////////// \n");
        printf("1.Pokaz przykladowe dzialanie programu.\n");
        printf("2.Testuj dodawanie wiadomosci do zapchanego buffora.\n");
        printf("3.Testuj odczyt wiadomosci z pustego bufora.\n");
        printf("4.Testuj dodanie nadmiarowego uzytkownika.\n");
        printf("5.Wyjdz.\n");
        printf("////////////////////////////////////////// \n");
        scanf("%d",&whichTest);

        initSemaphores(data);
        cleanUsers(data);
        cleanBuffor(data);
        initvalues(data);
        User user;
        user.who = USER;
        user.pid = 1111;    //pid zmyslamy na potrzeby testow (pidy uzytkownikow musza sie roznic) , w normalnej sytuacji zostalo by tu wywolane getpid()
        arg->data = data;
        arg->user = user;

        switch ( whichTest ) {
        case 1:
            printf("W tym tescie user, vip i admin wysylaja po jednej zwyklej wiadomosci, a nastepnie \nwszyscy ja odczytuja. Czwarta wiadomosc bedzie zawierac zle slowo dlatego admin\n(to on pierwszy otrzymuje wiadomosci) usunie ja i juz u vipa sie ona nie pojawi.\nCo wiecej mimo ze dodalismy najpierw wiadomosc usera to zostanie wyslana najpierw wiadomosc vipa\nZgodnie z trescia zadania vip poprzedza usera w buforze.\n\n");
            User vip;
            vip.pid = 2222;
            vip.who = VIP;
            User admin;
            admin.pid = 3333;
            admin.who = ADMIN;

            if(pthread_create(&tid, NULL, readerth, (void *)arg) != 0) {
                perror("Cannot test");
                munmap(data, SIZE);
                shm_unlink(NAME);
                close(fd);
                return EXIT_FAILURE;
            }
            initUser(data,user);
            initUser(data,vip);
            initUser(data,admin);
            sendMessage(data,user,"Czesc wszystkim tu user");
            sendMessage(data,vip,"Czesc wszystkim tu vip");
            sendMessage(data,admin,"Czesc wszystkim tu admin");
            sendMessage(data,user,"ZLE SLOWO");

            //to jest uproszczenie, w normalnym dzialaniu kazdy z tych odbiorcow powinien dzialac w nieskonczonej petli w osobnych procesach, dzialaja one jednak rowniez w jednym procesie w odpowiedniej kolejnosci
            gotMessageAdmin(data,admin);
            gotMessage(data,user);
            gotMessageAdmin(data,admin);
            gotMessage(data,vip);
            gotMessage(data,user);
            gotMessage(data,vip);
            gotMessageAdmin(data,admin);
            gotMessage(data,vip);

        sleep(1);   //sleep dodany dla zapewnienia nie pomieszania sie wiadomosci o wyborze testu z wynikami popprzedniego testu
        break;
        case 2:
            printf("Maksymalna liczba wiadomosci w buforze zostala ustawiona na 5. Wysylamy 7 wiadomosci naraz.\nW innym watku zostanie odpalony reader, ktory zwolni miejsce dla 6 i 7 wiadomosci i dopiero wtedy zostana one\ndodane do bufora a nastepnie wyslane.\n\n");

            if(pthread_create(&tid, NULL, readerth, (void *)arg) != 0) {
                perror("Cannot test");
                munmap(data, SIZE);
                shm_unlink(NAME);
                close(fd);
                return EXIT_FAILURE;
            }

            initUser(data,user);
            sendMessage(data,user,"Wiadomosc 1");
            sendMessage(data,user,"Wiadomosc 2");
            sendMessage(data,user,"Wiadomosc 3");
            sendMessage(data,user,"Wiadomosc 4");
            sendMessage(data,user,"Wiadomosc 5");
            sendMessage(data,user,"Wiadomosc 6");
            sendMessage(data,user,"Wiadomosc 7");

            sleep(1);
            break;
        case 3:
            printf("Przy braku wiadomosci i probie odczytu reader zatrzyma sie i bedzie czekal na wiadomosc.\nW tym tescie odpalimy readera przy pustym buforze. Reader bedzie czekal az otrzyma wiadomosc.\n\n");
            initUser(data,user);
            if(pthread_create(&tid, NULL, readerth, (void *)arg) != 0) {
                perror("Cannot test");
                munmap(data, SIZE);
                shm_unlink(NAME);
                close(fd);
                return EXIT_FAILURE;
            }

            sleep(1);
            break;
        case 4:
            printf("Maksymalna liczba uzytkownikow zostala ustawiona na 4. Po probie dodania 5 uzytwkownika program czekac bedzie do momentu az zwolni sie miejsce.\nZauwazmy ze nie mimo wywolania funkcji dla 5 usera nie zostaje on dodany.\n\n");

            printf("Wywolana funkcja initUser() dla usera o pid: %d\n",arg->user.pid);
            if(pthread_create(&tid, NULL, inituserth, (void *)arg) != 0) {
                perror("Cannot test");
                munmap(data, SIZE);
                shm_unlink(NAME);
                close(fd);
                return EXIT_FAILURE;
            }
            sleep(1);
            arg->user.pid++;
            printf("Wywolana funkcja initUser() dla usera o pid: %d\n",arg->user.pid);
            if(pthread_create(&tid, NULL, inituserth, (void *)arg) != 0) {
                perror("Cannot test");
                munmap(data, SIZE);
                shm_unlink(NAME);
                close(fd);
                return EXIT_FAILURE;
            }
            sleep(1);
            arg->user.pid++;
            printf("Wywolana funkcja initUser() dla usera o pid: %d\n",arg->user.pid);
            if(pthread_create(&tid, NULL, inituserth, (void *)arg) != 0) {
                perror("Cannot test");
                munmap(data, SIZE);
                shm_unlink(NAME);
                close(fd);
                return EXIT_FAILURE;
            }
            sleep(1);
            arg->user.pid++;
            printf("Wywolana funkcja initUser() dla usera o pid: %d\n",arg->user.pid);
            if(pthread_create(&tid, NULL, inituserth, (void *)arg) != 0) {
                perror("Cannot test");
                munmap(data, SIZE);
                shm_unlink(NAME);
                close(fd);
                return EXIT_FAILURE;
            }
            sleep(1);
             arg->user.pid++;
            printf("Wywolana funkcja initUser() dla usera o pid: %d\n",arg->user.pid);
            if(pthread_create(&tid, NULL, inituserth, (void *)arg) != 0) {
                perror("Cannot test");
                munmap(data, SIZE);
                shm_unlink(NAME);
                close(fd);
                return EXIT_FAILURE;
            }

            sleep(1);
            break;
        default:
            break;
        }
    }
    munmap(data, SIZE);
    shm_unlink(NAME);
    close(fd);
    return 0;
}
