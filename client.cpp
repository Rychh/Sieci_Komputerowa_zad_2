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
#include <netdb.h>
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
uint64_t cmd_seq = 5;


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

void operation_discover() {

}

void send_cmd(int sock, struct sockaddr_in &addr, const string &cmd, const string &data, uint64_t param = 0) {
    socklen_t addr_len;
    ssize_t snd_len;
    int flag = 0;
    CMD mess;

    memset(mess.CMPLX.cmd, 0, 10);
    strncpy(mess.CMPLX.cmd, cmd.c_str(), 10);
    mess.CMPLX.cmd_seq = htobe64(cmd_seq);
    if (param != 0) {
        mess.CMPLX.param = param;
        strncpy(mess.CMPLX.data, data.c_str(), CMD_CMPLX_DATA_SIZE);
    } else {
        strncpy(mess.SIMPL.data, data.c_str(), CMD_SIMPL_DATA_SIZE);
    }

    addr_len = (socklen_t) sizeof(addr);
    snd_len = sendto(sock, &mess, sizeof(CMD), flag,
                     (struct sockaddr *) &addr, addr_len);

    if (snd_len != (ssize_t) sizeof(CMD)) {
        syserr("partial / failed write");
    }

    cout << "Wyslałem: { cmd:" << mess.SIMPL.cmd << " ; bitow:" << snd_len << "}\n";
}


int main(int ac, char *av[]) {
    std::cout << "CLIENT!" << std::endl;
    parser(ac, av);
    CMD cos;
    struct sockaddr_in my_address;


    if (!fs::is_directory(OUT_FLDR)) {
        cerr << "There is no such directory.";
        exit(1);
    }

    cout << "wyszedlem z podejscia 2";
    int sock;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;
    char *remote_dotted_address = (char *) MCAST_ADDR.c_str();

    int i, flags, sflags;
//    char buffer[BUFFER_SIZE];
    size_t len;
    ssize_t snd_len, rcv_len;
    struct sockaddr_in srvr_address;
    socklen_t rcva_len;

    // 'converting' host/port in string to struct addrinfo
    (void) memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_DGRAM;
    addr_hints.ai_protocol = IPPROTO_UDP;
    addr_hints.ai_flags = 0;
    addr_hints.ai_addrlen = 0;
    addr_hints.ai_addr = NULL;
    addr_hints.ai_canonname = NULL;
    addr_hints.ai_next = NULL;
    if (getaddrinfo(remote_dotted_address, NULL, &addr_hints, &addr_result) != 0) {
        syserr("getaddrinfo");
    }

    my_address.sin_family = AF_INET; // IPv4
    my_address.sin_addr.s_addr =
            ((struct sockaddr_in *) (addr_result->ai_addr))->sin_addr.s_addr; // address IP
    my_address.sin_port = htons((uint16_t) CMD_PORT); // port from the command line

    freeaddrinfo(addr_result);

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    for (int i = 0; i < 100; i++) {
        string a, b;
        cin >> a;
        if (a != "d")
            cin >> b;
        if (a == "d")
            send_cmd(sock, my_address, "HELLO", b);
        if (a == "s") {
            if (b == "0")
                b = "";
            send_cmd(sock, my_address, "LIST", b);
        }
        if (a == "f")
            send_cmd(sock, my_address, "GET", b);
        if (a == "u")
            send_cmd(sock, my_address, "ADD", b);
        if (a == "r")
            send_cmd(sock, my_address, "DEL", b);
        else {
            /*     sflags = 0;
                 rcva_len = (socklen_t) sizeof(my_address);
                 snd_len = sendto(sock, &mess, sizeof(CMD), sflags,
                                  (struct sockaddr *) &my_address, rcva_len);
                 if (snd_len != (ssize_t) sizeof(mess)) {
                     syserr("partial / failed write");
                 }*/
            CMD mess;
            flags = 0;
            rcva_len = (socklen_t) sizeof(srvr_address);
            rcv_len = recvfrom(sock, &mess, sizeof(CMD), flags,
                               (struct sockaddr *) &srvr_address, &rcva_len);

            if (rcv_len < 0) {
                syserr("read");
            }

            cout << "Odebrałem: { cmd:" << mess.CMPLX.cmd << "; data:" << mess.CMPLX.data << "; bitow:" << rcv_len
                 << "}\n";
            if (cmd_seq != be64toh(mess.SIMPL.cmd_seq)) {
                cout << "ZLE!!! cmd_seq!!!!!!!!!!\n";
                cout << "ORGINAL: " << cmd_seq << "; SIMPL: " << be64toh(mess.SIMPL.cmd_seq) << "; CMPLx:"
                     << be64toh(mess.SIMPL.cmd_seq) << "\n";
            }
        }
        cout << "KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___\n\n";
    }

    /* koniec */
    close(sock);
    exit(EXIT_SUCCESS);

    return 0;
}