#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
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
#include <fcntl.h>
#include <sys/stat.h>
#include <thread>
#include "helper.h"
#include "err.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;
using namespace std;


string MCAST_ADDR;// -g
int CMD_PORT;// -p
string OUT_FLDR;// -o
short TIMEOUT = 5;// -t

int used_space = 0;
uint64_t cmd_seq = 5;
map<string, in_addr_t> filenames;

unsigned long tbs_addr = 0;
size_t tbs_space = 0;

/* Parser odpowiadający za flagi*/
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
            OUT_FLDR = vm["o"].as<string>() + "/";
        }

        if (vm.count("t")) {
            if (vm["t"].as<short>() <= 300) {
                TIMEOUT = vm["t"].as<short>();
            } else {
                cerr << "TIMEOUT must be less then 300.";
            }
        }
    }
    catch (exception &e) {
        cerr << "error: " << e.what() << "\n";
    }
    catch (...) {
        cerr << "Exception of unknown type!\n";
    }
}

/* Wypisanie na standardowe wyjście listę wszystkich węzłów serwerowych dostępnych aktualnie w grupie.*/
void discover(int sock, struct sockaddr_in &my_addr, bool print_message = true) {
    struct pollfd fd;
    struct timeval start, current;
    struct sockaddr_in srvr_addr;
    CMD mess;
    int flags = 0;
    tbs_addr = 0;
    tbs_space = 0;

    send_simpl_cmd(sock, my_addr, "HELLO", cmd_seq, "");
    gettimeofday(&start, 0);
    gettimeofday(&current, 0);
    time_t wait_ms = TIMEOUT * 1000; // Przez TIMEOUT sekund odbieram komunikaty.
    while (wait_ms > 0) {
        socklen_t rcva_len = (socklen_t) sizeof(srvr_addr);
        fd.fd = sock;
        fd.events = POLLIN;
        int ret = poll(&fd, 1, (int) wait_ms);
        switch (ret) {
            case -1:// ERROR
                break;
            case 0:// TIMEOUT
                break;
            default:
                // Udało się połączenie
                ssize_t rcv_len = recvfrom(sock, &mess, sizeof(CMD), flags,
                                           (struct sockaddr *) &srvr_addr, &rcva_len);// get your data
                if (rcv_len < 0) {
                    syserr("read");
                }
                if (cmd_seq == be64toh(mess.CMPLX.cmd_seq)) {
                    if (strcmp(mess.SIMPL.cmd, "GOOD_DAY") == 0) {
                        if (print_message) {
                            cout << "Found " << inet_ntoa(srvr_addr.sin_addr) << " (" << mess.CMPLX.data
                                 << ") with free space " << be64toh(mess.CMPLX.param)
                                 << "\n";
                        }
                        if (be64toh(mess.CMPLX.param) > tbs_space) {
                            tbs_addr = srvr_addr.sin_addr.s_addr;
                            tbs_space = be64toh(mess.CMPLX.param);
                        }
                    } else {
                        pckg_error(srvr_addr, "Wrong server command.");
                    }
                } else {
                    pckg_error(srvr_addr, "Wrong cmp_seq.");
                }
                break;
        }
        gettimeofday(&current, 0);
        wait_ms = (start.tv_sec + TIMEOUT - current.tv_sec) * 1000 +
                  (start.tv_usec - current.tv_usec) / 1000;
    }
}

/* Wypisanie na standardowe wyjście listę serwerowych plików, które posiadają dany infix.*/
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
    time_t wait_ms = TIMEOUT * 1000; // Przez TIMEOUT sekund odbieram komunikaty.
    while (wait_ms > 0) {
        socklen_t rcva_len = (socklen_t) sizeof(srvr_addr);
        fd.fd = sock;
        fd.events = POLLIN;
        int ret = poll(&fd, 1, wait_ms);
        switch (ret) {
            case -1:// ERROR
                break;
            case 0:// TIMEOUT
                break;
            default: // Udało się połączenie
                ssize_t rcv_len = recvfrom(sock, &mess, sizeof(CMD), flags,
                                           (struct sockaddr *) &srvr_addr, &rcva_len);// get your data
                if (rcv_len < 0) {
                    syserr("read");
                }

                if (cmd_seq == be64toh(mess.SIMPL.cmd_seq)) {
                    if (strcmp(mess.SIMPL.cmd, "MY_LIST") == 0) {
                        string filename;
                        stringstream ss(mess.SIMPL.data);
                        while (ss >> filename) {
                            cout << filename << " (" << inet_ntoa(srvr_addr.sin_addr) << ")\n";
                            filenames.insert(pair<string, in_addr_t>(filename, srvr_addr.sin_addr.s_addr));
                        }
                    } else {
                        pckg_error(srvr_addr, "Wrong server command.");
                    }
                } else {
                    pckg_error(srvr_addr, "");
                }
                break;
        }
        gettimeofday(&current, 0);
        wait_ms = (start.tv_sec + TIMEOUT - current.tv_sec) * 1000 +
                  (start.tv_usec - current.tv_usec) / 1000;
    }
}

/* Usunięcie z serwerów pliku filename.*/
void remove(int sock, struct sockaddr_in &my_addr, const string &filename) {
    if (filename.empty()) {
        cout << "Filename must contain signs.\n";
    } else {
        send_simpl_cmd(sock, my_addr, "DEL", cmd_seq, filename);
    }
}

/* Łączy się po tcp oraz zwraca socket */
int
tcp_connect_with_server(uint64_t srvr_port, const struct in_addr &srvr_ip, const string &connect_info) {
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;
    int err;
    int sock;
    string err_info = "File " + connect_info + " (" + inet_ntoa(srvr_ip) + ":" + to_string(srvr_port) + ") ";

    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;


    err = getaddrinfo(inet_ntoa(srvr_ip), to_string(srvr_port).c_str(), &addr_hints, &addr_result);
    if (err == EAI_SYSTEM) { // system error
        syserr2(err_info, "getaddrinfo: %s", gai_strerror(err));
    } else if (err != 0) { // other error (host not found, etc.)
        fatal("getaddrinfo: %s", gai_strerror(err));
    }

    // initialize socket according to getaddrinfo results
    sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
    if (sock < 0)
        syserr2(err_info, "socket");

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT; // Przez max TIMEOUT czekam na send/recv.
    timeout.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
                   sizeof(timeout)) < 0)
        syserr("setsockopt failed");

    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
                   sizeof(timeout)) < 0)
        syserr2(err_info, "setsockopt failed");

    // connect socket to the server
    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0)
        syserr2(err_info, "connect");

    freeaddrinfo(addr_result);

    return sock;
}

/* Łączy się po tcp oraz zapisuje plik. */
void tcp_read_to_file(in_addr_t ip, uint64_t srvr_port, string filename) {
    string file_path = OUT_FLDR + filename;
    char buffer[512 * 1024];
    struct in_addr srvr_ip;
    srvr_ip.s_addr = ip;
    int sock;

    sock = tcp_connect_with_server(srvr_port, srvr_ip, filename + " downloading failed");

    ssize_t datasize = 0;
    FILE *fd = fopen(file_path.c_str(), "wb");
    do {
        fwrite(&buffer, sizeof(char), datasize, fd);
        datasize = read(sock, buffer, sizeof(buffer));
    } while (datasize > 0);
    if (datasize == 0) {
        cout << "File " << filename << " downloaded (" << inet_ntoa(srvr_ip) << ":" << srvr_port << ")\n";
    } else {
        cout << "File " << filename << " downloading failed (" << inet_ntoa(srvr_ip) << ":" << srvr_port
             << ") Read\n";
    }
    fclose(fd);
    close(sock);
}

/* Pobranie pliku z serwera.*/
void fetch(int sock, const string &filename) {
    struct sockaddr_in srvr_addr;
    struct pollfd fd;
    string address;
    bool rcv_b = false;
    int flags = 0;

    if (filenames.find(filename) != filenames.end()) {
        CMD mess;
        srvr_addr.sin_family = AF_INET; // IPv4
        srvr_addr.sin_addr.s_addr = filenames[filename]; // address IP
        srvr_addr.sin_port = htons((uint16_t) CMD_PORT); // port from the command line
        address = inet_ntoa(srvr_addr.sin_addr);
        send_simpl_cmd(sock, srvr_addr, "GET", cmd_seq, filename);

        socklen_t rcva_len = (socklen_t) sizeof(srvr_addr);
        fd.fd = sock;
        fd.events = POLLIN;
        int ret = poll(&fd, 1, TIMEOUT * 1000);// Przez max TIMEOUT sekund czekam na odbiór komunikatu.
        switch (ret) {
            case -1:// Error
                break;
            case 0:// TIMEOUT
                break;
            default: // Udało się połączenie
                ssize_t rcv_len = recvfrom(sock, &mess, sizeof(CMD), flags,
                                           (struct sockaddr *) &srvr_addr, &rcva_len);// get your data
                if (rcv_len < 0) {
                    syserr("read");
                }
                rcv_b = true;
                break;
        }
        if (rcv_b) {
            if (cmd_seq == be64toh(mess.SIMPL.cmd_seq)) {
                if (strncmp(mess.SIMPL.cmd, "CONNECT_ME", 10) == 0) {
                    sleep(1);
                    in_addr_t ip = filenames[filename];
                    uint64_t port = be64toh(mess.CMPLX.param);
                    std::thread t{[ip, port, filename] {
                        tcp_read_to_file(ip, port, filename);
                    }};
                    t.detach();
                } else {
                    pckg_error(srvr_addr, "Wrong server command. Fetch was stopped.");
                }
            } else {
                pckg_error(srvr_addr, "Wrong cmd_seq. Fetch was stopped.");
            }
        } else {
            cout << "File " << filename << " downloading failed (" <<
                 address << ":" << ntohs(srvr_addr.sin_port)
                 << "). No answer was received.\n";
        }
    } else {
        cout << "File doesn't exist. Write 'search " << filename << "'.\n";
    }

}

/* Łączy się po tcp oraz wysyła plik. */
void tcp_write_from_file(in_addr_t ip, uint64_t srvr_port, string filename) {
    string file_path = OUT_FLDR + filename;
    char buffer[512 * 1024];
    struct in_addr srvr_ip;
    srvr_ip.s_addr = ip;
    int sock;

    sock = tcp_connect_with_server(srvr_port, srvr_ip, filename + " uploading failed");

    size_t bytes_read;
    size_t bytes_send;
    FILE *fd = fopen(file_path.c_str(), "rb");
    while (!feof(fd)) {
        if ((bytes_read = fread(&buffer, 1, sizeof(buffer), fd)) > 0) {
            bytes_send = 0;
            while (bytes_send != bytes_read)
                bytes_send += write(sock, buffer + bytes_send, bytes_read - bytes_send);
        } else {
            break;
        }
    }
    if (feof(fd)) {
        cout << "File " << filename << " uploaded (" << inet_ntoa(srvr_ip) << ":" << srvr_port << ")\n";
    }
    fclose(fd);
    close(sock);
}

/* Dodanie pliku do serwera.*/
void upload(int sock, const string &filename) {
    struct sockaddr_in srvr_addr;
    struct pollfd fd;
    FILE *file;
    bool rcv_b = false;
    int flags = 0;
    uint64_t srvr_port = 0;
    string path = filename;

    file = fopen((path).c_str(), "rb");
    if (file == nullptr) {
        path = OUT_FLDR + filename;
        file = fopen((path).c_str(), "rb");
    }
    if (file != nullptr) {
        fclose(file);
        if (fs::file_size(path) <= tbs_space) {
            CMD mess;
            string address;
            srvr_addr.sin_family = AF_INET; // IPv4
            srvr_addr.sin_addr.s_addr = filenames[filename]; // address IP
            srvr_addr.sin_port = htons((uint16_t) CMD_PORT); // port from the command line
            address = inet_ntoa(srvr_addr.sin_addr);
            send_cmplx_cmd(sock, srvr_addr, "ADD", cmd_seq, fs::file_size(path), filename);

            socklen_t rcva_len = (socklen_t) sizeof(srvr_addr);

            fd.fd = sock;
            fd.events = POLLIN;
            int ret = poll(&fd, 1, TIMEOUT * 1000); // Przez max TIMEOUT sekund czekam na odbiór komunikatu.
            switch (ret) {
                case -1:// ERROR
                    break;
                case 0:// TIMEOUT
                    break;
                default:// Udało się połączenie
                    ssize_t rcv_len = recvfrom(sock, &mess, sizeof(CMD), flags,
                                               (struct sockaddr *) &srvr_addr, &rcva_len);// get your data
                    if (rcv_len < 0) {
                        syserr("read");
                    }
                    rcv_b = true;
                    break;
            }
            if (rcv_b) {
                if (cmd_seq == be64toh(mess.SIMPL.cmd_seq)) {
                    srvr_port = be64toh(mess.CMPLX.param);
                    if (strcmp(mess.SIMPL.cmd, "CAN_ADD") == 0) {
                        sleep(1);
                        in_addr_t ip = filenames[filename];
                        std::thread t{[ip, srvr_port, filename] {
                            tcp_write_from_file(ip, srvr_port, filename);
                        }};
                        t.detach();
                    } else if (strcmp(mess.SIMPL.cmd, "NO_WAY") == 0) {
                        cout << "File " << filename << " uploading failed (" <<
                             address << ":" << ntohs(srvr_addr.sin_port)
                             << "). The server does not want to download.\n";
                    } else {
                        pckg_error(srvr_addr, "Wrong server command. Upload was stopped.");
                    }
                } else {
                    pckg_error(srvr_addr, "Wrong cmd_seq. Upload was stopped.");
                }
            } else {
                cout << "File " << filename << " uploading failed (" <<
                     address << ":" << ntohs(srvr_addr.sin_port)
                     << "). No answer was received.\n";
            }
        } else {
            cout << "File " << filename << " too big\n";
        }
    } else {
        cout << "File " << filename << " does not exist\n";
    }
}

int main(int ac, char *av[]) {
    parser(ac, av);
    struct sockaddr_in my_address;
    int sock;
    bool program_exit = false;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;
    char *remote_dotted_address = (char *) MCAST_ADDR.c_str();

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

    int counter;
    string response, tmp, first, second;
    while (!program_exit) {
        istringstream iss;
        counter = -1;
        getline(cin, response);
        iss.str(response);
        if (iss) {
            iss >> first;
            counter++;
        }
        while (iss) {
            iss >> second;
            counter++;
        }
        if (counter == 1) {
            if (boost::iequals(first, "discover")) {
                discover(sock, my_address);
            }
            if (boost::iequals(first, "search")) {
                search(sock, my_address, "");
            }
        } else if (counter == 2) {
            if (boost::iequals(first, "search")) {
                search(sock, my_address, second);
            }
            if (boost::iequals(first, "fetch")) {
                fetch(sock, second);
            }
            if (boost::iequals(first, "upload")) {
                discover(sock, my_address, false);
                upload(sock, second);
            }
            if (boost::iequals(first, "remove")) {
                remove(sock, my_address, second);
            }
            if (boost::iequals(first, "exit")) {
                program_exit = true;
            }
        }
    }
    /* koniec */
    close(sock);
    exit(EXIT_SUCCESS);

    return 0;
}