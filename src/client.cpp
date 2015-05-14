#include <boost/program_options.hpp>
#include "donkey.h"

using namespace std;
using namespace donkey;

int main (int argc, char *argv[]) {
    string config_path;
    vector<string> overrides;
    string method;
    uint32_t db;
    bool raw;
    bool content;

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message.")
        ("config", po::value(&config_path)->default_value("donkey.xml"), "")
        ("override,D", po::value(&overrides), "override configuration.")
        ("method", po::value(&method)->default_value("ping"), "")
        ("db", po::value(&db)->default_value(0), "")
        ("raw", "")
        ;

    po::positional_options_description p;
    p.add("method", 1);

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

    raw = vm.count("raw");
    content = vm.count("content");

    Config config;
    LoadConfig(config_path, &config);
    OverrideConfig(overrides, &config);

    Service *client = make_client(config);
    if (method == "ping") {
        client->ping();
    }
    else if (method == "insert") {
        string key;
        string url;
        while (cin >> key >> url) {
            InsertRequest req;
            InsertResponse resp;
            req.db = db;
            req.raw = raw;
            req.key = key;
            if (content) {
                ReadFile(url, &req.content);
            }
            else {
                req.url = url;
            }
            client->insert(req, &resp);
            cout << resp.time << '\t' << resp.load_time << '\t' << resp.journal_time << '\t' << resp.index_time << endl;
        }
    }
    else if (method == "search") {
    }
    else {
        MiscRequest req;
        MiscResponse resp;
        req.method = method;
        req.db = db;
        client->misc(req, &resp);
        cout << "code: " << resp.code << endl;
        cout << "text: " << resp.text << endl;
    }
    delete client;
    return 0;
}
