#include <boost/program_options.hpp>
#include "donkey.h"

using namespace std;
using namespace donkey;

int main (int argc, char *argv[]) {
    string config_path;
    vector<string> overrides;

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message.")
        ("config", po::value(&config_path)->default_value("donkey.xml"), "")
        ("override,D", po::value(&overrides), "override configuration.")
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

    Config config;
    LoadConfig(config_path, &config);
    OverrideConfig(overrides, &config);

    Server server(config);
    run_server(config, &server);

    return 0;
}
