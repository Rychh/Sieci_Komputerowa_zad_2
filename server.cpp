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
#include <poll.h>
#include "helper.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;
using namespace std;


//TODO zmienic domyslne wartość !!
string MCAST_ADDR = "239.10.11.12";// -g
int CMD_PORT = 6665;// -p
unsigned long long MAX_SPACE = 52428800;// -b
string SHRD_FLDR = "./test/";// -f
short TIMEOUT = 1;// -t
unsigned long long used_space = 0;

//PYTANIA:
//Czy robić nowe mess? - Nie trzeba bo forki "tworzą nowe".
//Jak ogółem postawić server? wielowątkowo? -Tak!
//TCP?? jak? Wątki?
//Zamykanie?

//TODO
//ZADANIA:
//Na zrobić samo przesyłanie plikow danych do clienta a on tylko wypisuje co widzi

/*
 * Po prau sekundach zamykanie czekania na odpowiedź
 * */

/* Parser
 * Dowiedzieć się jaki najlepiej użyć do client
 * Client
 * Może zmienić inne parsery
 * */

/* Obsługa zapytań clienta
 * */

/* TCP
 * współbieżnie?
 * */

/* Co z constexpr size_t CMD_SIZE = 300; ?
 *
 * */

/* Obsluga bledow
 *  sprawdzac czy pliki sie otworzyły??
 *  Co jeżeli przy oczekiwaniu na CONNECT_ME albo CAN_ADD dostane zly cmd_seq? Mam udawac ze go nie dostałem?
 *  gdzie mam poprawiac wartosc using_space
 * */
//TCP_2

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

/*
void pckg_error(const struct sockaddr_in &addr, const string &info) {

    cout << "[PCKG ERROR] Skipping invalid package from " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port)
         << ". " << info << "\n";
}
*/

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

/*
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
*/

bool filename_exist(string filename) {
    /* Sprawdzenie czy nie istnieje już plik o tej nazwie.*/
    cout << filename << "\n";
    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        if (is_regular_file(itr->status())
            && itr->path().filename().compare(filename) == 0) {
            return true;
        }
    }
    return false;
}

/* Odpowiedź serwera na komunikat "HELLO" */
void cmd_hello(int sock, struct sockaddr_in &client_address, uint64_t cmd_seq) {
    unsigned long long diff;
    if (MAX_SPACE < used_space)
        diff = 0;
    else
        diff = MAX_SPACE - used_space;
    send_cmplx_cmd(sock, client_address, "GOOD_DAY", cmd_seq, diff, MCAST_ADDR);
}

/* Odpowiedź serwera na komunikat "LIST" */
void cmd_list(int sock, struct sockaddr_in &client_address, uint64_t cmd_seq, const string &data) {
    string send_data;
    cout << "Wysylam liste z nazwami: " << data << " \n";
    int tmp = 0; //TODO nie potrzebne

    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        if (is_regular_file(itr->status()) &&
            (data.empty() ||
             itr->path().filename().string().find(data) != string::npos)) {
            if (send_data.size() + itr->path().filename().string().size() + 1 > CMD_CMPLX_DATA_SIZE) {
                send_simpl_cmd(sock, client_address, "MY_LIST", cmd_seq, send_data);
                cout << "Send_data jest za duzy. Wysle to w osobnych komunikatach.\n";
                cout << "LISTALISTALISTALISTALISTALISTALISTALISTALISTALISTALISTALISTALISTALISTA\n";
                send_data = "";
            }
            send_data += itr->path().filename().string() + "\n";
            tmp++;
        }
    }

    if (!send_data.empty()) {
        cout << "Wysyłam komunikat\n";
        send_simpl_cmd(sock, client_address, "MY_LIST", cmd_seq, send_data);
    }
    cout << "Wysłałem info o " << tmp << " plikach\n";
}


void wyslij_plik(int new_sock, string filename) {
    cout << "Jestem w wyslij_plik\n";

    struct sockaddr_in new_client_address;
    socklen_t new_client_address_len;

    int queue_length = 5;
    int msg_sock;

    cout << "przed lissten\n";

    // switch to listening (passive open)
    if (listen(new_sock, queue_length) < 0)
        syserr("listen");

    cout << "Przed acceptem\n";
    msg_sock = accept(new_sock, (struct sockaddr *) &new_client_address, &new_client_address_len);
    if (msg_sock < 0)
        syserr("accept");
    string file_path = SHRD_FLDR + filename;
    FILE *fd = fopen(file_path.c_str(), "rb");
    size_t rret, wret;
    char buffer[60000]; //TODO zwiększyc. MALLOCOWAĆ??
    cout << "przed wysyłaniem\n";

    size_t bytes_read;
    ssize_t wyslane;
    while (!feof(fd)) {
        if ((bytes_read = fread(&buffer, 1, sizeof(buffer), fd)) > 0) { //TODO zmienic
            cout << "przed write\n";
            wyslane = write(msg_sock, buffer, bytes_read); //TODO send czy write??
            cout << "Wysłałem " << wyslane << "bytow, a miało byc:" << bytes_read << "\n";
        } else {
            cout << "Koniec wysyłania!\n";
            break;
        }
    }
    cout << "Po wsyłaniu \n";

    fclose(fd);
    close(msg_sock);
    cout << "Po zamknięciu fd\n";
}

/* Odpowiedź serwera na komunikat "GET" */
void cmd_get(int sock, struct sockaddr_in &client_address, uint64_t cmd_seq, string filename) {
    cout << "cmd_get\n";
    if (filename_exist(filename)) {
        cout << "filename_exist(filename)\n";

        struct sockaddr_in server_address;
        int new_sock;
        uint64_t port;


        new_sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
        if (new_sock < 0)
            syserr("socket");
        // after socket() call; we should close(sock) on any execution path;
        // since all execution paths exit immediately, sock would be closed when program terminates

        server_address.sin_family = AF_INET; // IPv4
        server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
        server_address.sin_port = 0; // listening on port PORT_NUM


        struct timeval timeout;
        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;

        if (setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
                       sizeof(timeout)) < 0)
            syserr("setsockopt failed\n");

        if (setsockopt(new_sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
                       sizeof(timeout)) < 0)
            syserr("setsockopt failed\n");

        cout << "Bind\n";

        // bind the socket to a concrete address
        if (bind(new_sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
            syserr("bind");

        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (getsockname(new_sock, (struct sockaddr *) &sin, &len) == -1)
            perror("getsockname");
        else {
            port = ntohs(sin.sin_port);
        }

        send_cmplx_cmd(sock, client_address, "CONNECT_ME", cmd_seq, port, filename);//TODO!! paaram/port
        cout << "connect_me wysłałem!!\n";
        wyslij_plik(new_sock, filename);
    } else {
        cout << "Nie ma tu tatkiego pliku:\"" << filename << "\". Proszę poszukać gdzieś indziej.\n";
        //TODO jezeli dany plik nie istnieje to nic nie wysylam ale odnotowuje
    }
}

/* Odpowiedź serwera na komunikat "REMOVE" */
void cmd_remove(string filename) {
    cout << "Chcę usunąć: " << filename << "\n";
    int tmp = 0; //TODO nie potrzebna
    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        if (is_regular_file(itr->status())
            && itr->path().filename().compare(filename) == 0) {
            used_space -= file_size(itr->path());
            remove(itr->path());
            tmp++;
        }
    }
    if (tmp != 0)
        cout << "Usunałem " << tmp << " plikow\n";
    else
        cout << "Nic nie usunalem\n";
}


void pobierz_pliki(int new_sock, string filename) {
    cout << "Jestem w pobierz_pliki\n";
    struct sockaddr_in new_client_address;
    socklen_t new_client_address_len;
    int queue_length = 5;
    int msg_sock;
    cout << "przed lissten\n";

    /*   struct pollfd fd;
       fd.fd = new_sock; // your socket handler
       fd.events = POLLIN;
       int ret = poll(&fd, 1, TIMEOUT * 1000); // 1 second for timeout
       switch (ret) {
           case -1:
               cout << "\n\nERROR_________ERROR_________ERROR_________ERROR_________ERROR_________\n\n";
               break;
           case 0:
               cout << "TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__\n";
               break;
           default:
               // switch to listening (passive open)
               break;
       }
   */
    if (listen(new_sock, queue_length) < 0)
        syserr("listen");
    cout << "Przed acceptem\n";


    msg_sock = accept(new_sock, (struct sockaddr *) &new_client_address, &new_client_address_len);
    if (msg_sock < 0)
        syserr("accept");

    string file_path = SHRD_FLDR + filename;
    char buffer[60000]; //TODO zwiększyc. MALLOCOWAĆ??
    cout << "przed wysyłaniem\n";
    ssize_t datasize = 0;
    FILE *file = fopen(file_path.c_str(), "wb");
    do {
        fwrite(&buffer, sizeof(char), datasize, file);
        datasize = read(msg_sock, buffer, sizeof(buffer));//TODO msg_sock1!
        cout << "Read datasize = " << datasize << "\n";
//        used_space += datasize; //TODO spoko?? chyb nie
    } while (datasize > 0);
    cout << "Pobrano!!\n";

    fclose(file);
    close(msg_sock);
    cout << "Po zamknięciu file\n";
}


/* Odpowiedź serwera na komunikat "ADD" */
void cmd_add(int sock, struct sockaddr_in &client_address, uint64_t cmd_seq, uint64_t param, const string &filename) {
    cout << "Chce dodać: " << filename << "\n";

    /* Sprawdzenie czy możemy dodać plik */
    if (param > MAX_SPACE - used_space ||
        filename.empty() || (filename.find("/") != string::npos) ||
        filename_exist(filename)) {
        cout << "No niestety nie wyszło :)\n";
        send_simpl_cmd(sock, client_address, "NO_WAY", cmd_seq, filename);
    } else {
        cout << "Oki doki dawaj śmiało\n";


        struct sockaddr_in server_address;
        int new_sock;
        uint64_t port = 0;


        new_sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
        if (new_sock < 0)
            syserr("socket");
        // after socket() call; we should close(sock) on any execution path;
        // since all execution paths exit immediately, sock would be closed when program terminates

        server_address.sin_family = AF_INET; // IPv4
        server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
        server_address.sin_port = 0; // listening on port PORT_NUM

        struct timeval timeout;
        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;

        if (setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
                       sizeof(timeout)) < 0)
            syserr("setsockopt failed\n");

        if (setsockopt(new_sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
                       sizeof(timeout)) < 0)
            syserr("setsockopt failed\n");

        cout << "Bind\n";

        // bind the socket to a concrete address
        if (bind(new_sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
            syserr("bind");

        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (getsockname(new_sock, (struct sockaddr *) &sin, &len) == -1)
            perror("getsockname");
        else {
            port = ntohs(sin.sin_port);
            char myIP[16]; //TODO skasowac to
            inet_ntop(AF_INET, &sin.sin_addr, myIP, sizeof(myIP));
            cout << "Port: " << port << "; IP : " << myIP << "\n";
        }

        if (port != 0) {
            used_space += param; //TODO spoko?? chyb nie

            //TODO sock new_cosk xDFGRWEAF?
            send_cmplx_cmd(sock, client_address, "CAN_ADD", cmd_seq, port, filename);//TODO!! paaram/port


            cout << filename << new_sock << "-przed\n";
            pid_t pid = fork();
            if (pid == 0) {// child process
                pobierz_pliki(new_sock, filename);

                cout << new_sock << "-d\n";
                syserr("wszystko gra");
            } else if (pid > 0) {
                cout << "ojciec\n";
            } else {
                // fork failed
                cout << "Problem z forkiem!!!\n"; //TODO;
            }

        }

        cout << "Czekam na TCP\n";
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

    multicast_dotted_address = (char *) MCAST_ADDR.c_str();
    sock = initSock(ip_mreq, multicast_dotted_address, CMD_PORT);
    cout << "polaczonao\n";
    uint64_t cmd_seq;
    uint64_t param;

    /* czytanie tego, co odebrano */
    for (i = 0; i < 1000; ++i) {
        //TODO zawsze robić nowey sockem
        cout << "-----------------------------------------------------\nCzekam przed recv\n";

        rcva_len = (socklen_t) sizeof(client_address);
        int flag = 0;
        ssize_t len = recvfrom(sock, &mess, sizeof(CMD), flag,
                               (struct sockaddr *) &client_address, &rcva_len);

        if (len < 0)
            syserr("error on datagram from client socket");
        else {
            cmd_seq = be64toh(mess.SIMPL.cmd_seq);


            //TODO czy ja nie musze skiopowac danych przy forku?

            cout << "Odebrałem:  { cmd:" << mess.SIMPL.cmd << "; bitow:" << len << "}\n";
            if (strcmp(mess.SIMPL.cmd, "HELLO") == 0)
                cmd_hello(sock, client_address, cmd_seq);
            else if (strcmp(mess.SIMPL.cmd, "LIST") == 0)
                cmd_list(sock, client_address, cmd_seq, mess.SIMPL.data);
            else if (strcmp(mess.SIMPL.cmd, "GET") == 0)
                cmd_get(sock, client_address, cmd_seq, mess.SIMPL.data); //TODO
            else if (strcmp(mess.SIMPL.cmd, "DEL") == 0)
                cmd_remove(mess.SIMPL.data);
            else if (strcmp(mess.SIMPL.cmd, "ADD") == 0) {
                param = be64toh(mess.CMPLX.param);
                cmd_add(sock, client_address, cmd_seq, param, mess.CMPLX.data);
            } else
                pckg_error(client_address, "Wrong command.");
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