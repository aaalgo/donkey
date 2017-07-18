#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>
#include <boost/program_options.hpp>
#include "donkey.h"

using namespace std;
using namespace donkey;

int main (int argc, char *argv[]) {
    string config_path;
    string pid_path;
    vector<string> overrides;
    bool readonly;
    int nice;

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message.")
        ("config", po::value(&config_path)->default_value("donkey.xml"), "")
        ("override,D", po::value(&overrides), "override configuration.")
        ("readonly", "")
        ("nice", po::value(&nice)->default_value(10), "do not lower priority")
        ("pid", po::value(&pid_path), "pid file")
        ;

    po::positional_options_description p;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << "Usage:" << endl;
        cout << "\tserver ..." << endl;
        cout << desc;
        cout << endl;
        return 0;
    }

    if (pid_path.size()) {
        do {
            struct stat st;
            if (stat(pid_path.c_str(), &st) < 0) {
                if (errno == ENOENT) break; // pid file doesn't exist, good!
                cerr << "Bad status " << pid_path << ": " << strerror(errno) << endl;
            }
            else {
                cerr << "PID file already exists " << pid_path << endl;
            }
            return 1;   // exit if we are not happy with PID file status
        } while (false);
        pid_t pid = getpid();
        ofstream os(pid_path.c_str());
        os << pid;
    }

    readonly = vm.count("readonly") > 0;

    Config config;
    LoadConfig(config_path, &config);
    OverrideConfig(overrides, &config);

    setup_logging(config);

    if (nice > 0) {
        // pid 0 means the calling process, 
        setpriority(PRIO_PROCESS, 0, nice);
    }

    for (;;) {
        Server server(config, readonly);
        if (!run_server(config, &server)) break;
        LOG(info) << "restarting server.";
    }
    cleanup_logging();
    if (pid_path.size()) {
        unlink(pid_path.c_str());
    }

    return 0;
}
