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
//        cout << itr->path() << ' '; // display filename only //TODO
        if (is_regular_file(itr->status())) {
//            cout << " [" << file_size(itr->path()) << ']'; //TODO
            used_space += file_size(itr->path());
        }
//        cout << '\n';
    }
}

void set_sock_options(int &sock, struct ip_mreq &ip_mreq, char *multicast_dotted_address) {

    /* podpięcie się do grupy rozsyłania (ang. multicast) */
    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(multicast_dotted_address, &ip_mreq.imr_multiaddr) == 0)
        syserr("inet_aton");

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &ip_mreq, sizeof ip_mreq) < 0)
        syserr("setsockopt");

    int optVal = 1;

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &optVal, sizeof(optVal)) < 0)
        syserr("setsockopt");

    optVal = 4;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optVal, sizeof(optVal)) < 0)
        syserr("setsockopt");

}

int main(int ac, char *av[]) {
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t snda_len, rcva_len;

    std::cout << "SERVER! <--" << std::endl;
    parser(ac, av);
    CMD cos;

    cout << "CMD_SIZE: " << CMD_SIZE << "\n";
    cout << "sizeof(CMD): " << sizeof(CMD) << "\n";
    cout << "COS: " << sizeof(cos) << "\n";
    cout << "COS.SIMPL.data: " << sizeof(cos.SIMPL.data) << "\n";
    cout << "COS.CMPLS.data: " << sizeof(cos.CMPLX.data) << "\n";


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

    /* argumenty wywołania programu */
    char *multicast_dotted_address;
    in_port_t local_port;

    /* zmienne i struktury opisujące gniazda */
    int sock;
    struct sockaddr_in local_address;
    struct ip_mreq ip_mreq;

    /* zmienne obsługujące komunikację */
    ssize_t rcv_len;
    int i;

    /* parsowanie argumentów programu */

    local_port = CMD_PORT;
    multicast_dotted_address = (char *) MCAST_ADDR.c_str();

    /* otworzenie gniazda */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    set_sock_options(sock, ip_mreq, multicast_dotted_address);

    /* podpięcie się pod lokalny adres i port */
    local_address.sin_family = AF_INET;
    local_address.sin_addr.s_addr = htonl(INADDR_ANY);
    local_address.sin_port = htons(local_port);
    if (bind(sock, (struct sockaddr *) &local_address, sizeof local_address) < 0)
        syserr("bind");

    cout << "polaczonao";

//    char buffer[CMD_SIZE + 1000];
    CMD mess;



    /* czytanie tego, co odebrano */
    for (i = 0; i < 1000; ++i) {
        cout << "Czekam przed recv\n";

        rcva_len = (socklen_t) sizeof(client_address);
        snda_len = (socklen_t) sizeof(client_address);
        int flag = 0;
        ssize_t len = recvfrom(sock, &mess, sizeof(CMD), flag,
                               (struct sockaddr *) &client_address, &rcva_len);
        if (len < 0)
            syserr("error on datagram from client socket");
        else {
            printf("read %zd bytes: %.*s\n", len, 10, mess.SIMPL.cmd);

            strcpy(mess.CMPLX.cmd, "GOOD_DAY");
            mess.CMPLX.param = MAX_SPACE - used_space;
            strcpy(mess.CMPLX.data, MCAST_ADDR.c_str());

            cout << "Przed sendto\n";
//            printf("Napis: %.*s\n", 8, client_address.sin_zero);
//            cout << client_address.sin_zero[0] << "\n";

            ssize_t snd_len = sendto(sock, &mess, sizeof(CMD), flag,
                                     (struct sockaddr *) &client_address, snda_len);

            cout<<"wysłałem: "<<snd_len<<"\n";
            if (snd_len != sizeof(CMD))
                syserr("error on sending datagram to client socket");
        }
/*
        rcv_len = read(sock, &mess, CMD_SIZE);

        if (rcv_len < 0)
            syserr("read");
        else {
            printf("read %zd bytes: %.*s\n", rcv_len, 10, mess.SIMPL.cmd);
        }
        strcpy(mess.CMPLX.cmd, "GOOD_DAY");
        mess.CMPLX.param = MAX_SPACE - used_space;
        strcpy(mess.CMPLX.data, MCAST_ADDR.c_str());

        cout<<"HMMM??\n";

        if (write(sock, &mess, CMD_SIZE) != CMD_SIZE)
            syserr("write");

*/
    }
    /* w taki sposób można odpiąć się od grupy rozsyłania */
    if (setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &ip_mreq, sizeof ip_mreq) < 0)
        syserr("setsockopt");

    /* koniec */
    close(sock);
    exit(EXIT_SUCCESS);

    return 0;
}