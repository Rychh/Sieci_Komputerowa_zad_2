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
#include "helper.h"
#include <poll.h>
#include <sys/time.h>

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
        }
        cout << "KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___KONIEC___\n\n";
    }

    /* koniec */
    close(sock);
    exit(EXIT_SUCCESS);

    return 0;
}