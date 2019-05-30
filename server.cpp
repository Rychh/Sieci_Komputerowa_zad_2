#include <iostream>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "helper.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <stdarg.h>

namespace po = boost::program_options;
namespace fs = boost::filesystem;
using namespace std;


string MCAST_ADDR;// -g
int CMD_PORT;// -p
unsigned long long MAX_SPACE = 52428800;// -b
string SHRD_FLDR = ".";// -f
short TIMEOUT = 5;// -t
int used_space = 0;

//PYTANIA:
//Czy robić nowe mess? - Nie trzeba bo forki "tworzą nowe".
//Jak ogółem postawić server? wielowątkowo? -Tak!
//TCP?? jak? Wątki?
//Zamykanie?

//TODO
//ZADANIA:
//Na zrobić samo przesyłanie plikow danych do clienta a on tylko wypisuje co widzi

/* Po prau sekundach zamykanie czekania na odpowiedź
 * */

/* Parser
 * Dowiedzieć się jaki najlepiej użyć do client
 * Client
 * Może zmienić inne parsery
 * */

/* Obsługa zapytań clienta
 * jakas strukturka do hello
 * hello
 * FETch
 * serach
 * remove
 * i cos tam jeszcze
 * */

/* TCP
 * jak zrobic?
 * przeczytac jak zrobiłem to w zad1
 * TCP  server -> client
 * TCP client -> server
 * */

/* Obsluga bledow
 *
 * */
//TCP_2

void syserr(const char *fmt, ...) {
    va_list fmt_args;
    int err = errno;

    fprintf(stderr, "ERROR: ");

    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end (fmt_args);
    fprintf(stderr, " (%d; %s)\n", err, strerror(err));
    exit(EXIT_FAILURE);
}

void parser(int ac, char *av[]) {
    try {

        po::options_description desc("Allowed options");
        auto style = po::command_line_style::style_t(
                po::command_line_style::unix_style |
                po::command_line_style::case_insensitive |
                po::command_line_style::allow_long_disguise);
        desc.add_options()
                ("help", "produce help message")
                ("g", po::value<string>(), "adres rozgłaszania ukierunkowanego")
                ("p", po::value<int>(), "port UDP używany do przesyłania i odbierania poleceń")
                ("b", po::value<unsigned long long>(),
                 "maksymalna liczba bajtów udostępnianej przestrzeni dyskowej na pliki grupy przez ten węzeł serwerowy, wartość domyślna 52428800")
                ("f", po::value<string>(),
                 "ścieżka do dedykowanego folderu dyskowego, gdzie mają być przechowywane pliki")
                ("t", po::value<short>(),
                 "liczba sekund, jakie serwer może maksymalnie oczekiwać na połączenia od klientów, wartość domyślna 5, wartość maksymalna 300");

        po::variables_map vm;
        po::store(po::parse_command_line(ac, av, desc, style), vm);
        po::notify(vm);

        if (vm.count("help")) {
            cout << desc << "\n";
        }

        if (vm.count("g")) {
            MCAST_ADDR = vm["g"].as<string>();
        }

        if (vm.count("p")) {
            CMD_PORT = vm["p"].as<int>();
        }

        if (vm.count("b")) {
            MAX_SPACE = vm["b"].as<unsigned long long>();
        }

        if (vm.count("f")) {
            SHRD_FLDR = vm["f"].as<string>();
        }

        if (vm.count("t")) {
            if (vm["t"].as<short>() <= 300) {
                TIMEOUT = vm["t"].as<short>();
            } else {
                cerr << "Hola hola, Kolego. TIMEOUT nie większy niż 300!"; //TODO
            }
        }
    }
    catch (exception &e) {
        cerr << "error: " << e.what() << "\n";
    }
    catch (...) {
        cerr << "Exception of unknown type!\n";
    }
    /*
    cout << "MCAST_ADDR:" << MCAST_ADDR << "\n"
         << "CMD_PORT:" << CMD_PORT << "\n"
         << "MAX_SPACE:" << MAX_SPACE << "\n"
         << "SHRD_FLDR:" << SHRD_FLDR << "\n"
         << "TIMEOUT:" << TIMEOUT << "\n";*/

}

void load_files_names() {
    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        cout << itr->path() << '=' << itr->path().filename().string() << " "; // display filename only //TODO
        if (is_regular_file(itr->status())) {
            cout << " [" << file_size(itr->path()) << ']'; //TODO
            used_space += file_size(itr->path());
        }
        cout << '\n';
    }
}

int initSock(struct ip_mreq &ip_mreq, char *multicast_dotted_address, short local_port) {
    /* otworzenie gniazda */
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    /* podpięcie się pod lokalny adres i port */
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(local_port);


    /* podpięcie się do grupy rozsyłania (ang. multicast) */
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(multicast_dotted_address, &ip_mreq.imr_multiaddr) == 0)
        syserr("inet_aton");

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &ip_mreq, sizeof ip_mreq) < 0)
        syserr("setsockopt");

    int optVal = 1;

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &optVal, sizeof(optVal)) < 0)
        syserr("setsockopt");

    optVal = 4; //TODO czy to potrzebne?
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optVal, sizeof(optVal)) < 0)
        syserr("setsockopt");

    if (bind(sock, (struct sockaddr *) &server_address, sizeof server_address) < 0)
        syserr("bind");
    return sock;
}

void send_to_client(int sock, CMD &mess, struct sockaddr_in &client_address) {
    int flag = 0;
    socklen_t snda_len = (socklen_t) sizeof(client_address);

    ssize_t snd_len = sendto(sock, &mess, sizeof(CMD), flag,
                             (struct sockaddr *) &client_address, snda_len);
    cout << "Wysłalem: { cmd:" << mess.CMPLX.cmd << "; data:" << mess.CMPLX.data << "; bitow:" << snd_len
         << "; }\n";
    if (snd_len != sizeof(CMD))
        syserr("error on sending datagram to client socket");
}

bool filename_exist(string filename) {
    /* Sprawdzenie czy nie istnieje już plik o tej nazwie.*/
    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        if (is_regular_file(itr->status())
            && itr->path().filename().compare(filename) == 0) {
            return true;
        }
    }
    return false;
}

/* Odpowiedź serwera na komunikat "HELLO" */
void cmd_hello(int sock, CMD &mess, struct sockaddr_in &client_address) {
    uint64_t cmd_seq = mess.SIMPL.cmd_seq;
    int flag = 0;
    memset(mess.CMPLX.cmd, 0, 10);
    strcpy(mess.CMPLX.cmd, "GOOD_DAY");
    mess.CMPLX.param = MAX_SPACE - used_space;
    strcpy(mess.CMPLX.data, MCAST_ADDR.c_str());
    mess.CMPLX.cmd_seq = cmd_seq;

    send_to_client(sock, mess, client_address);

}

/* Odpowiedź serwera na komunikat "LIST" */
void cmd_list(int sock, CMD &mess, struct sockaddr_in &client_address) {
    uint64_t cmd_seq = mess.SIMPL.cmd_seq;
    string recv_data = mess.SIMPL.data;
    string send_data;
    cout << "Wysylam liste z nazwami: " << recv_data << " \n";
    int tmp = 0; //TODO nie potrzebne

    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        if (is_regular_file(itr->status()) &&
            (recv_data.empty() ||
             itr->path().filename().string().find(recv_data) != string::npos)) {
            //TODO find nie sprawdza wielkosci liter
            if (send_data.size() + itr->path().filename().string().size() + 1 > CMD_CMPLX_DATA_SIZE) {
                //TODO nie jestem pewien co do messega czy robic nowego czy co
                cout << send_data.size() << " + " << itr->path().filename().string().size() << " > "
                     << CMD_CMPLX_DATA_SIZE << "\n";
                memset(mess.SIMPL.cmd, 0, 10);
                strcpy(mess.SIMPL.cmd, "MY_LIST");
                mess.SIMPL.cmd_seq = cmd_seq;
                strcpy(mess.SIMPL.data, send_data.c_str());

                send_to_client(sock, mess, client_address);
                cout << "Send_data jest za duzy. Wysle to w osobnych komunikatach.\n";
                cout << "LISTALISTALISTALISTALISTALISTALISTALISTALISTALISTALISTALISTALISTALISTA\n";
                send_data = "";
            }
            send_data += itr->path().filename().string() + "\n";
            tmp++;
        }
    }

    if (!send_data.empty()) {
        //TODO nie jestem pewien co do messega czy robic nowego czy co
        cout << "Wysyłam komunikat\n";
        memset(mess.SIMPL.cmd, 0, 10);
        strcpy(mess.SIMPL.cmd, "MY_LIST");
        mess.SIMPL.cmd_seq = cmd_seq;
        strcpy(mess.SIMPL.data, send_data.c_str());

        send_to_client(sock, mess, client_address);
    }

    cout << "Wysłałem info o " << tmp << " plikach\n";
}

/* Odpowiedź serwera na komunikat "GET" */
void cmd_get(int sock, CMD &mess, struct sockaddr_in &client_address) {
    uint64_t cmd_seq = mess.SIMPL.cmd_seq;
    string filename = mess.SIMPL.data;

    if (!filename_exist(filename)) {
        memset(mess.CMPLX.cmd, 0, 10);
        strcpy(mess.CMPLX.cmd, "CONNECT_ME");
        mess.CMPLX.param = htobe64(6666);//TODO jakis port?
        strcpy(mess.CMPLX.data, filename.c_str());
        mess.CMPLX.cmd_seq = cmd_seq;


    } else {
        //TODO jezeli dany plik nie istnieje to nic nie wysylam ale odnotowuje
    }

}

/* Odpowiedź serwera na komunikat "REMOVE" */
void cmd_remove(CMD &mess) {
    cout << "Chcę usunąć: " << mess.SIMPL.data << "\n";
    int tmp = 0; //TODO nie potrzebna
    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        if (is_regular_file(itr->status())
            && itr->path().filename().compare(mess.SIMPL.data) == 0) {
            remove(itr->path());
            tmp++;
        }
    }
    if (tmp != 0)
        cout << "Usunałem " << tmp << " plikow\n";
    else
        cout << "Nic nie usunalem\n";
}

/* Odpowiedź serwera na komunikat "ADD" */
void cmd_add(int sock, CMD &mess, struct sockaddr_in &client_address) {
    uint64_t cmd_seq = mess.SIMPL.cmd_seq;
    string filename = mess.SIMPL.data;
    cout << "Chce dodać: " << filename << "\n";

    /* Sprawdzenie czy możemy dodać plik */
    if (be64toh(mess.CMPLX.param) > MAX_SPACE - used_space ||
        filename.size() == 0 || (filename.compare("/") == 0) ||
        filename_exist(filename)) {
        cout << "No niestety nie wyszło :)\n";
        memset(mess.SIMPL.cmd, 0, 10);
        strcpy(mess.SIMPL.cmd, "NO_WAY");
        mess.SIMPL.cmd_seq = cmd_seq;
        strcpy(mess.SIMPL.data, filename.c_str());

        send_to_client(sock, mess, client_address);
    } else {
        cout << "Oki doki dawaj śmiało\n";
        memset(mess.CMPLX.cmd, 0, 10);
        strcpy(mess.CMPLX.cmd, "CAN_ADD");
        mess.CMPLX.cmd_seq = cmd_seq;
        strcpy(mess.CMPLX.data, filename.c_str());
        mess.CMPLX.param = htobe64(5555); //TODO trzeba dawać jakiś inny port

        send_to_client(sock, mess, client_address);
        cout << "Czekam na TCP\n";

        //TODO połącz po TCP
    }
}


int main(int ac, char *av[]) {
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t snda_len, rcva_len;
    CMD mess;

    char *multicast_dotted_address;
    in_port_t local_port;
    int sock;
    struct ip_mreq ip_mreq;
    ssize_t rcv_len;
    int i;

    std::cout << "SERVER! <--" << std::endl;
    parser(ac, av);

    if (fs::is_directory(SHRD_FLDR)) {
        load_files_names();
    } else {
        cerr << "There is no such directory.";
        exit(1);
    }

    if (used_space > MAX_SPACE) {
        cerr << "Sry Za amało pamieci."; //TODO
        exit(1);
    }
    multicast_dotted_address = (char *) MCAST_ADDR.c_str();
    sock = initSock(ip_mreq, multicast_dotted_address, CMD_PORT);
    cout << "polaczonao\n";

    /* czytanie tego, co odebrano */
    for (i = 0; i < 1000; ++i) {

        //TODO zawsze robić nowey sockem
        cout << "-----------------------------------------------------\nCzekam przed recv\n";

        rcva_len = (socklen_t) sizeof(client_address);
        int flag = 0;
        ssize_t len = recvfrom(sock, &mess, sizeof(CMD), flag,
                               (struct sockaddr *) &client_address, &rcva_len);

        sleep(TIMEOUT);

        if (len < 0)
            syserr("error on datagram from client socket");
        else {
            cout << "Odebrałem:  { cmd:" << mess.SIMPL.cmd << "; bitow:" << len << "}\n";
            if (strcmp(mess.SIMPL.cmd, "HELLO") == 0)
                cmd_hello(sock, mess, client_address);
            else if (strcmp(mess.SIMPL.cmd, "LIST") == 0)
                cmd_list(sock, mess, client_address);
            else if (strcmp(mess.SIMPL.cmd, "GET") == 0)
                cmd_get(sock, mess, client_address);
            else if (strcmp(mess.SIMPL.cmd, "DEL") == 0)
                cmd_remove(mess);
            else if (strcmp(mess.SIMPL.cmd, "ADD") == 0)
                cmd_add(sock, mess, client_address);
            else
                cout << "Coś nie pykło!!!\nZły komunikat.\n";
            cout << "_SERVER__SERVER__SERVER__SERVER__SERVER__SERVER__SERVER_\n\n ";
        }
    }
    /* w taki sposób można odpiąć się od grupy rozsyłania */
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &ip_mreq, sizeof ip_mreq) < 0)
        syserr("setsockopt");

    /* koniec */
    close(sock);
    exit(EXIT_SUCCESS);

    return 0;
}