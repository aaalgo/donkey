#include <set>
#include <map>
#include <atomic>
#include <sstream>
#include <boost/program_options.hpp>
#include <boost/algorithm/string/trim.hpp>
#include "donkey.h"
namespace donkey {

    struct BackendSet {
        string tag;
        vector<string> addresses;
        unsigned cnt;

        bool search_with_retry (SearchRequest const &request, SearchResponse *response, unsigned retry) {
            for (unsigned rr = 0; rr < retry; ++rr) {
                unsigned c = __sync_fetch_and_add(&cnt, 1);
                string const &address = addresses[c % addresses.size()];
                try {
                    std::unique_ptr<Service> client(make_client(address));
                    if (client == 0) continue;
                    client->search(request, response);
                    return true;
                }
                catch (std::exception const &e) {
                    LOG(error) << address << ": " << e.what();
                }
                catch (...) {
                    LOG(error) << address << ": unknown exception.";
                }
            }
            return false;
        }

    };

    class Proxy: public Service {
        // very simple round-robin request forwarding
        // no connection pooling
        unsigned retry;
        vector<BackendSet> backends;
    public:
        Proxy (Config const &config, string const &backends_config)
            : retry(config.get<unsigned>("donkey.proxy.retry", 3))
        {
            std::map<string, std::set<string>> dict;
            {   // load backends
                ifstream is(backends_config.c_str());
                string line;
                unsigned c = 0;
                while (getline(is, line)) {
                    {
                        auto o = line.find('#');
                        if (o != line.npos) {
                            line.resize(o);
                        }
                        boost::algorithm::trim(line);
                    }
                    if (line.empty()) continue;
                    std::istringstream ss(line);
                    string address;
                    string tag;
                    ss >> address >> tag;
                    if (dict[tag].insert(address).second) {
                        ++c;
                    }
                }
                LOG(info) <<  c << " backends in " << dict.size() << " categories loaded.";
                if (dict.size() > 1) {
                    throw NotImplementedError("only support one category");
                }
            }
            for (auto const &p: dict) {
                backends.emplace_back();
                auto &bs = backends.back();
                bs.tag = p.first;
                bs.addresses = vector<string>(p.second.begin(), p.second.end());
                bs.cnt = 0;
            }
            // create empty dbs
        }

        ~Proxy () { // close all dbs
        }

        void ping (PingResponse *) {
        }

        void insert (InsertRequest const &request, InsertResponse *response) {
            throw NotImplementedError("insert");
        }

        void search (SearchRequest const &request, SearchResponse *response) {
            Timer timer(&response->time);
            if (!backends[0].search_with_retry(request, response, retry)) {
                throw ProxyBackendError("");
            }
            /*
            check_dbid(request.db);
            Object object;
            {
                Timer timer1(&response->load_time);
                loadObject(request, &object);
            }
            dbs[request.db]->search(object, request, response);
            */
            // query all backends in parallel
        }

        void misc (MiscRequest const &request, MiscResponse *response) {
            throw NotImplementedError("insert");
        }
    };
}

using namespace std;
using namespace donkey;

int main (int argc, char *argv[]) {
    string config_path;
    string backends_path;
    vector<string> overrides;

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message.")
        ("config", po::value(&config_path)->default_value("donkey.xml"), "")
        ("override,D", po::value(&overrides), "override configuration.")
        ("backends", po::value(&backends_path), "")
        ;

    po::positional_options_description p;
    p.add("backends", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm);

    if (vm.count("help") || vm.count("backends") == 0) {
        cout << "Usage:" << endl;
        cout << "\tproxy <backends>" << endl;
        cout << desc;
        cout << endl;
        return 0;
    }


    Config config;
    LoadConfig(config_path, &config);
    OverrideConfig(overrides, &config);
    setup_logging(config);

    Proxy proxy(config, backends_path);
    run_server(config, &proxy);
    cleanup_logging();

    return 0;
}
