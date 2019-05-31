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
#include <cstdarg>
#include <poll.h>
#include <sys/time.h>
#include <map>
#include "helper.h"
#include "err.h"

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

map<string, string> filenames;


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

void discover(int sock, struct sockaddr_in &my_addr) {
    struct pollfd fd;
    struct timeval start, current;
    struct sockaddr_in srvr_addr;
    CMD mess;
    int flags = 0;

    send_simpl_cmd(sock, my_addr, "HELLO", cmd_seq, "");
    gettimeofday(&start, 0);
    gettimeofday(&current, 0);
    __time_t wait_ms = TIMEOUT * 1000;
    while (wait_ms > 0) {
        socklen_t rcva_len = (socklen_t) sizeof(srvr_addr);
        fd.fd = sock; // your socket handler
        fd.events = POLLIN;
        int ret = poll(&fd, 1, wait_ms); // 1 second for timeout
        switch (ret) {
            case -1:
                cout << "\n\nERROR_________ERROR_________ERROR_________ERROR_________ERROR_________\n\n";
                // Error
                break;
            case 0:
                cout << "TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__\n";
                break;
            default:
                ssize_t rcv_len = recvfrom(sock, &mess, sizeof(CMD), flags,
                                           (struct sockaddr *) &srvr_addr, &rcva_len);// get your data
                if (rcv_len < 0) {
                    syserr("read");
                }

                if (cmd_seq == be64toh(mess.CMPLX.cmd_seq)) {
                    cout << "Found " << inet_ntoa(srvr_addr.sin_addr) << " (" << mess.CMPLX.data
                         << ") with free space " << be64toh(mess.CMPLX.param)
                         << "\n";
                } else {
                    cout << "\nNOPE\n\n"; //TODO jakis komunikat
                }
                break;
        }
        gettimeofday(&current, 0);
        wait_ms = (start.tv_sec + TIMEOUT - current.tv_sec) * 1000 +
                  (start.tv_usec - current.tv_usec) / 1000;
    }
}

void search(int sock, struct sockaddr_in &my_addr, const string &infix) {
    struct pollfd fd;
    struct timeval start, current;
    struct sockaddr_in srvr_addr;
    CMD mess;
    int flags = 0;

    filenames.clear();
    send_simpl_cmd(sock, my_addr, "LIST", cmd_seq, infix);
    gettimeofday(&start, 0);
    gettimeofday(&current, 0);
    __time_t wait_ms = TIMEOUT * 1000;
    while (wait_ms > 0) {
        socklen_t rcva_len = (socklen_t) sizeof(srvr_addr);
        fd.fd = sock; // your socket handler
        fd.events = POLLIN;
        int ret = poll(&fd, 1, wait_ms); // 1 second for timeout
        switch (ret) {
            case -1:
                cout << "\n\nERROR_________ERROR_________ERROR_________ERROR_________ERROR_________\n\n";
                // Error
                break;
            case 0:
                cout << "TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__\n";
                break;
            default:
                ssize_t rcv_len = recvfrom(sock, &mess, sizeof(CMD), flags,
                                           (struct sockaddr *) &srvr_addr, &rcva_len);// get your data
                if (rcv_len < 0) {
                    syserr("read");
                }

                if (cmd_seq == be64toh(mess.SIMPL.cmd_seq)) {
                    string filename;
                    stringstream ss(mess.SIMPL.data);
                    while (ss >> filename) {
                        cout << filename << " (" << inet_ntoa(srvr_addr.sin_addr) << ")\n";
                        filenames.insert(pair<string, string>(filename, inet_ntoa(srvr_addr.sin_addr)));
                    }
                } else {
                    cout << "\nNOPE\n\n"; //TODO jakis komunikat
                }
                break;
        }
        gettimeofday(&current, 0);
        wait_ms = (start.tv_sec + TIMEOUT - current.tv_sec) * 1000 +
                  (start.tv_usec - current.tv_usec) / 1000;
    }
}

void remove(int sock, struct sockaddr_in &my_addr, const string &filename) {
    if (filename.empty()) {
        cout << "Nazwa musi byc ne pusta\n";
        //TODO
    } else {
        send_simpl_cmd(sock, my_addr, "DEL", cmd_seq, filename);
    }
}


int main(int ac, char *av[]) {
    std::cout << "CLIENT!" << std::endl;
    parser(ac, av);
    CMD cos;
    struct sockaddr_in my_address;
    size_t N = 1;
    struct timeval current, start;
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

    struct pollfd fds[N];

    for (int i = 0; i < N; ++i) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }


    if (!fs::is_directory(OUT_FLDR)) {
        cerr << "There is no such directory.";
        exit(1);
    }
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
        cout << "\nCzekam na nowe polecenie\n";
        string a, b;
        cin >> a;
        if (a != "d")
            cin >> b;
        if (a == "d") {
            discover(sock, my_address);
            continue;
        }
        if (a == "s") {
            if (b == "0")
                b = "";
            search(sock, my_address, b);
            continue;
        }
        if (a == "r") {
            if (b == "0")
                b = "";
            remove(sock, my_address, b);
            continue;

        }
        if (a == "f")
            send_simpl_cmd(sock, my_address, "GET", cmd_seq, b);
        if (a == "u")
            send_simpl_cmd(sock, my_address, "ADD", cmd_seq, b);
        gettimeofday(&start, 0);
        gettimeofday(&current, 0);
        __time_t wait_ms = TIMEOUT * 1000;
        while (wait_ms > 0) {
            struct pollfd fd;
            int ret;
            CMD mess;
            flags = 0;
            rcva_len = (socklen_t) sizeof(srvr_address);

            fd.fd = sock; // your socket handler
            fd.events = POLLIN;
            ret = poll(&fd, 1, wait_ms); // 1 second for timeout
            switch (ret) {
                case -1:
                    cout << "\n\nERROR_________ERROR_________ERROR_________ERROR_________ERROR_________\n\n";
                    // Error
                    break;
                case 0:
                    cout << "TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__TIMEOUT__\n";
                    break;
                default:
                    rcv_len = recvfrom(sock, &mess, sizeof(CMD), flags,
                                       (struct sockaddr *) &srvr_address, &rcva_len);// get your data
                    if (rcv_len < 0) {
                        syserr("read");
                    }
                    cout << "Odebrałem: { cmd:" << mess.CMPLX.cmd << "; data:" << mess.CMPLX.data << "; bitow:"
                         << rcv_len << "}\n";
                    if (cmd_seq != be64toh(mess.SIMPL.cmd_seq)) {
                        cout << "ZLE!!! cmd_seq!!!!!!!!!!\n";
                        cout << "ORGINAL: " << cmd_seq << "; SIMPL: " << be64toh(mess.SIMPL.cmd_seq) << "; CMPLx:"
                             << be64toh(mess.SIMPL.cmd_seq) << "\n";
                    }
                    break;

            }
            gettimeofday(&current, 0);
            wait_ms = (start.tv_sec + TIMEOUT - current.tv_sec) * 1000 +
                      (start.tv_usec - current.tv_usec) / 1000;

        }
        cout << "KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___\n\n";
    }

    /* koniec */
    close(sock);
    exit(EXIT_SUCCESS);

    return 0;
}