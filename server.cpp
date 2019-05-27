#include <iostream>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <string>
namespace po = boost::program_options;
namespace fs = boost::filesystem;
using namespace std;

constexpr size_t MAX_CMD_SIZE = 1 << (1 << (1 << (1 << 1)));


string MCAST_ADDR;// -g
int CMD_PORT;// -p
unsigned long long MAX_SPACE = 52428800;// -b
string SHRD_FLDR = ".";// -f
short TIMEOUT = 5;// -t
int used_space = 0;

struct SIMPL_CMD {
    char cmd[10];
    uint64_t cmd_seq;
    char data[MAX_CMD_SIZE - 10 * sizeof(char) - sizeof(uint64_t)];
};

struct CMPLX_CMD {
    char cmd[10];
    uint64_t cmd_seq;
    uint64_t param;
    char data[MAX_CMD_SIZE - 10 * sizeof(char) - 2 * sizeof(uint64_t)];
};

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
}

void load_files_names() {
    for (fs::directory_iterator itr(SHRD_FLDR); itr != fs::directory_iterator(); ++itr) {
        cout << itr->path() << ' '; // display filename only //TODO
        if (is_regular_file(itr->status())) {
            cout << " [" << file_size(itr->path()) << ']'; //TODO
            used_space += file_size(itr->path());
        }
        cout << '\n';
    }
}

int main(int ac, char *av[]) {
    std::cout << "Hello, World!" << std::endl;
    parser(ac, av);

    cout << "MCAST_ADDR:" << MCAST_ADDR << "\n"
         << "CMD_PORT:" << CMD_PORT << "\n"
         << "MAX_SPACE:" << MAX_SPACE << "\n"
         << "SHRD_FLDR:" << SHRD_FLDR << "\n"
         << "TIMEOUT:" << TIMEOUT << "\n";

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

    return 0;
}