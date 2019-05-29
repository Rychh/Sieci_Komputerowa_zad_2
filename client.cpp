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

namespace po = boost::program_options;
namespace fs = boost::filesystem;
using namespace std;

#define TTL_VALUE     4

string MCAST_ADDR;// -g
int CMD_PORT;// -p
string OUT_FLDR = ".";// -f
short TIMEOUT = 5;// -t

int used_space = 0;


void syserr(const char *fmt, ...) {
    fprintf(stderr, "ERROR: ");
    exit(EXIT_FAILURE);
}//TODO

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
                ("o", po::value<string>(),
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

        if (vm.count("o")) {
            OUT_FLDR = vm["o"].as<string>();
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

    /* cout << "MCAST_ADDR:" << MCAST_ADDR << "\n"
          << "CMD_PORT:" << CMD_PORT << "\n"
          << "OUT_FLDR:" << OUT_FLDR << "\n"
          << "TIMEOUT:" << TIMEOUT << "\n";
 */
}

void set_sock_options(int &sock) {
    int optval;
    /* uaktywnienie rozgłaszania (ang. broadcast) */
    optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void *) &optval, sizeof optval) < 0)
        syserr("setsockopt broadcast");

    /* ustawienie TTL dla datagramów rozsyłanych do grupy */
    optval = TTL_VALUE;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optval, sizeof optval) < 0)
        syserr("setsockopt multicast ttl");
}

int main(int ac, char *av[]) {
    std::cout << "CLIENT!" << std::endl;
    parser(ac, av);
    CMD cos;


    cout <<"CMD_SIZE: "<< CMD_SIZE <<"\n";
    cout<<"sizeof(CMD): "<< sizeof(CMD) << "\n";
    cout <<"COS: "<<  sizeof(cos)<< "\n";
    cout <<"COS.SIMPL.data: "<<  sizeof(cos.SIMPL.data)<< "\n";
    cout <<"COS.CMPLS.data: "<<  sizeof(cos.CMPLX.data)<< "\n";

    if (!fs::is_directory(OUT_FLDR)) {
        cerr << "There is no such directory.";
        exit(1);
    }

    /* argumenty wywołania programu */
    char *remote_dotted_address;
    in_port_t remote_port;
    char buffer[3000];
    int length;


    /* zmienne i struktury opisujące gniazda */
    int sock, optval;
//  struct sockaddr_in local_address;
    struct sockaddr_in remote_address;

    /* parsowanie argumentów programu */
    remote_dotted_address = (char *) MCAST_ADDR.c_str();
    remote_port = CMD_PORT;


    /* otworzenie gniazda */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    set_sock_options(sock);

    /* ustawienie adresu i portu odbiorcy */
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(remote_port);
    if (inet_aton(remote_dotted_address, &remote_address.sin_addr) == 0)
        syserr("inet_aton");
    if (connect(sock, (struct sockaddr *) &remote_address, sizeof remote_address) < 0)
        syserr("connect");

    CMD mess;
    strcpy(mess.SIMPL.cmd,"HELLO" );

    /* radosne rozgłaszanie czasu */
    cout << "SZMAL: " << sizeof(CMD) << "PLN\n";

    if (write(sock, &mess, sizeof(CMD)) != sizeof(CMD))
        syserr("write");

    cout << "Przelałem, czekam na: " <<  sizeof(CMD)<< "\n";

    ssize_t rcv_len = read(sock, &mess, 1);
    cout <<rcv_len<< " otrzymalem\n";

    if (rcv_len < 0)
        syserr("read");
    else {
        printf("read %zd bytes: %.*s\n", rcv_len, 10, mess.CMPLX.cmd);
        printf("wolne miejsca %d bytes: %s\n", mess.CMPLX.param, 10, mess.CMPLX.data);
    }


    /* koniec */
    close(sock);
    exit(EXIT_SUCCESS);

    return 0;
}