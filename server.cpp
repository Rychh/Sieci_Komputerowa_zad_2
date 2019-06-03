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
#include <thread>
#include "helper.h"

namespace po = boost::program_options;
namespace fs = boost::filesystem;
using namespace std;


string MCAST_ADDR;// -g
int CMD_PORT;// -p
unsigned long long MAX_SPACE = 52428800;// -b
string SHRD_FLDR = "";// -f
short TIMEOUT = 5;// -t
unsigned long long used_space = 0;

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
            SHRD_FLDR = vm["f"].as<string>() + "/";
        }

        if (vm.count("t")) {
            if (vm["t"].as<short>() <= 300) {
                TIMEOUT = vm["t"].as<short>();
            } else {
                cerr << "TIMEOUT must be less then 300.";
                exit(1);
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

/* Zlicza rozmiar plików w folderze */
void load_files_names() {
    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        if (is_regular_file(itr->status())) {
            used_space += file_size(itr->path());
        }
    }
}

/* Inicjuje socket dla udp */
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

    optVal = 4;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *) &optVal, sizeof(optVal)) < 0)
        syserr("setsockopt");

    if (bind(sock, (struct sockaddr *) &server_address, sizeof server_address) < 0)
        syserr("bind");
    return sock;
}

/* Sprawdza czy dany plik znajduje się w folderze */
bool filename_exist(const string &filename) {
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

    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        if (is_regular_file(itr->status()) &&
            (data.empty() ||
             itr->path().filename().string().find(data) != string::npos)) {
            if (send_data.size() + itr->path().filename().string().size() + 1 > CMD_CMPLX_DATA_SIZE) {
                send_simpl_cmd(sock, client_address, "MY_LIST", cmd_seq, send_data);
            }
            send_data += itr->path().filename().string() + "\n";
        }
    }
    if (!send_data.empty()) {
        send_simpl_cmd(sock, client_address, "MY_LIST", cmd_seq, send_data);
    }
}

/* Wysłanie pliku przez tcp. */
void tcp_send_file(int new_sock, string filename) {
    struct sockaddr_in new_client_address;
    socklen_t new_client_address_len;
    int queue_length = 5;
    int msg_sock;

    // switch to listening (passive open)
    if (listen(new_sock, queue_length) < 0)
        syserr("listen");

    msg_sock = accept(new_sock, (struct sockaddr *) &new_client_address, &new_client_address_len);
    if (msg_sock < 0)
        syserr("accept");
    string file_path = SHRD_FLDR + filename;
    FILE *fd = fopen(file_path.c_str(), "rb");
    char buffer[512 * 1024];

    // wysyłanie
    size_t bytes_read;
    size_t bytes_write;
    while (!feof(fd)) {
        if ((bytes_read = fread(&buffer, 1, sizeof(buffer), fd)) > 0) {
            bytes_write = 0;
            while (bytes_write != bytes_read)
                bytes_write += write(msg_sock, buffer + bytes_write, bytes_read - bytes_write);
        } else {
            break;
        }
    }
    fclose(fd);
    close(msg_sock);
}

/* Odpowiedź serwera na komunikat "GET" */
void cmd_get(int sock, struct sockaddr_in &client_address, uint64_t cmd_seq, string filename) {
    if (filename_exist(filename)) {
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
        timeout.tv_sec = TIMEOUT; // Przez max TIMEOUT czekam na send/recv.
        timeout.tv_usec = 0;

        if (setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
                       sizeof(timeout)) < 0)
            syserr("setsockopt failed");

        if (setsockopt(new_sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
                       sizeof(timeout)) < 0)
            syserr("setsockopt failed");

        // bind the socket to a concrete address
        if (bind(new_sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
            syserr("bind");

        struct sockaddr_in sin;
        socklen_t len = sizeof(sin);
        if (getsockname(new_sock, (struct sockaddr *) &sin, &len) == -1)
            syserr("getsockname");
        else {
            port = ntohs(sin.sin_port);
            send_cmplx_cmd(sock, client_address, "CONNECT_ME", cmd_seq, port, filename);
            std::thread t{[new_sock, filename] { tcp_send_file(new_sock, filename); }};
            t.detach();
        }
    } else {
        pckg_error(client_address, "We don't have file: \"" + filename + "\"");
    }
}

/* Odpowiedź serwera na komunikat "REMOVE" */
void cmd_remove(string filename) {
    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        if (is_regular_file(itr->status())
            && itr->path().filename().compare(filename) == 0) {
            used_space -= file_size(itr->path());
            remove(itr->path());
        }
    }
}

/* Pobranie pliku przez tcp. */
void tcp_download_file(int new_sock, string filename) {
    struct sockaddr_in new_client_address;
    socklen_t new_client_address_len;
    int queue_length = 5;
    int msg_sock;

    if (listen(new_sock, queue_length) < 0)
        syserr("listen");

    msg_sock = accept(new_sock, (struct sockaddr *) &new_client_address, &new_client_address_len);
    if (msg_sock < 0)
        syserr("accept");

    // pobieranie
    string file_path = SHRD_FLDR + filename;
    char buffer[512 * 1024];
    ssize_t datasize = 0;
    FILE *file = fopen(file_path.c_str(), "wb");
    do {
        fwrite(&buffer, sizeof(char), datasize, file);
        datasize = read(msg_sock, buffer, sizeof(buffer));
    } while (datasize > 0);

    fclose(file);
    close(msg_sock);
}


/* Odpowiedź serwera na komunikat "ADD" */
void cmd_add(int sock, struct sockaddr_in &client_address, uint64_t cmd_seq, uint64_t param, const string &filename) {

    /* Sprawdzenie czy możemy dodać plik */
    if (param > MAX_SPACE - used_space ||
        filename.empty() || (filename.find('/') != string::npos) ||
        filename_exist(filename)) {
        send_simpl_cmd(sock, client_address, "NO_WAY", cmd_seq, filename);
    } else {
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
        timeout.tv_sec = TIMEOUT; // Przez max TIMEOUT czekam na send/recv.
        timeout.tv_usec = 0;

        if (setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout,
                       sizeof(timeout)) < 0)
            syserr("setsockopt failed\n");

        if (setsockopt(new_sock, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout,
                       sizeof(timeout)) < 0)
            syserr("setsockopt failed\n");

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

        if (port != 0) {
            used_space += param;
            send_cmplx_cmd(sock, client_address, "CAN_ADD", cmd_seq, port, filename);

            std::thread t{[new_sock, filename] { tcp_download_file(new_sock, filename); }};
            t.detach();
        }
    }
}

int main(int ac, char *av[]) {
    struct sockaddr_in client_address;
    socklen_t rcva_len;
    CMD mess;
    char *multicast_dotted_address;
    int sock;
    struct ip_mreq ip_mreq;

    parser(ac, av);

    if (fs::is_directory(SHRD_FLDR)) {
        load_files_names();
    } else {
        cerr << "There is no such directory.";
        exit(1);
    }

    multicast_dotted_address = (char *) MCAST_ADDR.c_str();
    sock = initSock(ip_mreq, multicast_dotted_address, CMD_PORT);
    uint64_t cmd_seq;
    uint64_t param;

    /* czytanie tego, co odebrano */
    while (true) {
        rcva_len = (socklen_t) sizeof(client_address);
        int flag = 0;
        ssize_t len = recvfrom(sock, &mess, sizeof(CMD), flag,
                               (struct sockaddr *) &client_address, &rcva_len);
        if (len < 0)
            syserr("error on datagram from client socket");
        else {
            cmd_seq = be64toh(mess.SIMPL.cmd_seq);

            if (strcmp(mess.SIMPL.cmd, "HELLO") == 0)
                cmd_hello(sock, client_address, cmd_seq);
            else if (strcmp(mess.SIMPL.cmd, "LIST") == 0)
                cmd_list(sock, client_address, cmd_seq, mess.SIMPL.data);
            else if (strcmp(mess.SIMPL.cmd, "GET") == 0)
                cmd_get(sock, client_address, cmd_seq, mess.SIMPL.data);
            else if (strcmp(mess.SIMPL.cmd, "DEL") == 0)
                cmd_remove(mess.SIMPL.data);
            else if (strcmp(mess.SIMPL.cmd, "ADD") == 0) {
                param = be64toh(mess.CMPLX.param);
                cmd_add(sock, client_address, cmd_seq, param, mess.CMPLX.data);
            } else
                pckg_error(client_address, "Wrong command.");
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