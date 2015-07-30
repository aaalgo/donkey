#include <signal.h>
#include <boost/program_options.hpp>
#include <thread>
#include <random>
#include "donkey.h"

using namespace std;
using namespace donkey;

void load_queries (istream &is, SearchRequest const &proto, bool content, vector<SearchRequest> *reqs) {
    string key;
    string url;
    cerr << "Loading queries ..." << endl;
    while (is >> key >> url) {
        reqs->push_back(proto);
        auto &last = reqs->back();
        last.expect_key = key;
        if (content) {
            ReadFile(url, &last.content);
        }
        else {
            last.url = url;
        }
    }
    cerr << reqs->size() << " queries loaded." << endl;

}

class SigHandler {

    static void sigsegv_handler (int) {
        /*
        logger->fatal("SIGSEGV, force exit.");
        logger->fatal(Stack().format());
        */
        exit(1);
    }

    static void handleSigSegV () {
        struct sigaction sa;
        memset(&sa, 0, sizeof(struct sigaction));
        sa.sa_handler = sigsegv_handler;
        sa.sa_flags = SA_RESETHAND;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGSEGV, &sa, NULL);
    }

public:
    SigHandler (bool debug = false) {
    }
};

bool sigint_flag = false;
static void sigint_handler (int) {
    sigint_flag = true;
}
void mask_sigint_once () {
    sigint_flag = false;
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = sigint_handler;
    sa.sa_flags = SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
}

class Stress {
    Config config;
    vector<string> servers;
    vector<SearchRequest> reqs;
    vector<SearchResponse> resps;
    //std::mutex mutex;
    unsigned threads;
    unsigned count;
    unsigned sleep;  // ms
    bool keepalive;   
    bool once;

    void worker (unsigned tid, unsigned *next, unsigned *cnt, unsigned *err, unsigned *miss, unsigned *load) {
        default_random_engine rng(tid);
        bool done = false;
        unsigned server_index = tid % servers.size();
        mask_sigint_once();
        while (!done && !sigint_flag) {
            Service *client = nullptr;
            Config local_config = config;
            if (servers.size()) {
                string server = servers[server_index];
                string host;
                int port;
                auto off = server.find(':');
                if (off == server.npos) {
                    host = server;
                    port = 50052;
                }
                else {
                    host = server.substr(0, off);
                    port = boost::lexical_cast<int>(server.substr(off+1));
                }
                local_config.put<string>("donkey.thrift.client.server", host);
                local_config.put<int>("donkey.thrift.client.port", port);
            }
            client = make_client(local_config);
            BOOST_VERIFY(client);
            uniform_int_distribution<unsigned> query_dist(0, reqs.size()-1);
            try {
                do {
                    unsigned n = __sync_fetch_and_add(next, 1);
                    if (count && (n >= count)) {
                        done = true;
                        break;
                    }
                    SearchResponse dummy;
                    SearchRequest const *preq;
                    SearchResponse *presp;
                    if (once) {
                        preq = &reqs[n];
                        presp = &resps[n];
                    }
                    else {
                        preq = &reqs[query_dist(rng)];
                        presp = &dummy;
                    }
                    client->search(*preq, presp);
                    __sync_fetch_and_add(cnt, 1);
                    __sync_fetch_and_add(&load[server_index], 1);
                    {
                        bool hit = false;
                        for (auto const &h: presp->hits) {
                            if (h.key == preq->expect_key) {
                                hit = true;
                                break;
                            }
                        }
                        if (!hit) {
                            __sync_fetch_and_add(miss, 1);
                        }
                    }
                    if (sigint_flag) break;
                    if (sleep) {
                        this_thread::sleep_for(chrono::milliseconds(sleep));
                        if (sigint_flag) break;
                    }
                } while (keepalive);
            } catch (exception& tx) {
                __sync_fetch_and_add(err, 1);
                cout << "ERROR: " << tx.what() << endl;
            }
            delete client;
        }
    };
public:
    Stress (Config const &config_,
            vector<string> servers_,
            vector<SearchRequest> &&reqs_, bool once_, bool keep = true)
        : config(config_),
        servers(servers_),
        reqs(reqs_),
        threads(config.get<unsigned>("donkey.stress.threads", 1)),
        count(config.get<unsigned>("donkey.stress.count", 0)),
        sleep(config.get<unsigned>("donkey.stress.sleep", 0)),
        once(once_),
        keepalive(keep)
    {
        if (once) {
            if (count == 0 || count > reqs.size()) {
                count = reqs.size();
            }
            resps.resize(reqs.size());
        }
        if ((count > 0) && (threads > count)) {
            threads = count;
        }
        if (servers.empty()) {
            string host = config.get<string>("donkey.thrift.client.server", "localhost");
            string port = config.get<string>("donkey.thrift.client.port", "50052");
            servers.push_back(host + ":" + port);
        }
    }
    void run () {
        thread th[threads];
        vector<unsigned> load(servers.size(), 0);
        if (load.empty()) {
            load.push_back(0);
        }
        unsigned next = 0, cnt = 0, err = 0, miss = 0;
        unsigned *pload = &load[0];
        for (unsigned i = 0; i < threads; ++i) {
            th[i] = thread([this,i,&next, &cnt,&err,&miss, pload](){this->worker(i, &next, &cnt, &err, &miss, pload);});
        }
        unsigned oldcnt = cnt;
        unsigned olderr = err;
        unsigned oldmiss = miss;

        unsigned sec = 1;
        while (((count == 0) || (cnt < count)) && (!sigint_flag)) {
            this_thread::sleep_for(chrono::seconds(sec));
            unsigned c = cnt;
            unsigned e = err;
            unsigned m = miss;
            cout << float(c - oldcnt) / sec << '/' << c
                << '\t' << float(e - olderr) / sec << '/' << e
                << '\t' << float(m - oldmiss) / sec << '/' << m << endl;
            oldcnt = c;
            olderr = e;
            oldmiss = m;
        }
        for (auto &t: th) {
            t.join();
        }
        cout << cnt << '\t' << err << '\t' << miss << endl;
        cout << "load:" << endl;
        for (unsigned i = 0; i < load.size(); ++i) {
            cout << i << ": " << load[i] << endl;
        }
        if (once) {
            for (unsigned i = 0; i < count; ++i) {
                auto const &req = reqs[i];
                auto const &resp = resps[i];
                for (auto const &h: resp.hits) {
                    cout << req.expect_key << " => " << h.key << '\t' << h.score << '\t' << h.meta << endl;
                }
                if (resp.hits.empty()) {
                    cout << req.expect_key << endl;
                }
            }
        }
    }
};

int main (int argc, char *argv[]) {
    string config_path;
    vector<string> overrides;
    vector<string> servers;
    string server_list_path;
    string query_list_path;
    unsigned threads;
    unsigned count;
    bool content;
    bool once;
    bool keepalive;
    SearchRequest search;

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message.")
        ("config", po::value(&config_path)->default_value("donkey.xml"), "")
        ("override,D", po::value(&overrides), "override configuration.")
        ("server", po::value(&servers), "")
        ("server-list,S", po::value(&server_list_path), "")
        ("query-list,Q", po::value(&query_list_path), "read from stdin if not provided")
        ("threads,t", po::value(&threads), "")
        ("count,c", po::value(&count), "")
        ("db", po::value(&search.db)->default_value(0), "")
        ("type", po::value(&search.type), "")
        ("feature", "")
        ("content", "")
        (",K", po::value(&search.K)->default_value(-1), "")
        (",R", po::value(&search.R)->default_value(NAN), "")
        ("hint_K", po::value(&search.hint_K)->default_value(-1), "")
        ("hint_R", po::value(&search.hint_R)->default_value(NAN), "")
        ("once", "")
        ("no-keepalive", "")
        ;

    po::positional_options_description p;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        cout << "Usage:" << endl;
        cout << "\tstress ..." << endl;
        cout << desc;
        cout << endl;
        return 0;
    }

    search.raw = !vm.count("feature");
    content = vm.count("content");
    once = vm.count("once");
    keepalive = vm.count("no-keepalive") == 0;


    Config config;
    LoadConfig(config_path, &config);
    OverrideConfig(overrides, &config);

    if (vm.count("threads")) config.put("donkey.stress.threads", threads);
    if (vm.count("count")) config.put("donkey.stress.count", count);

    // load servers from file
    if (server_list_path.size()) {
        string line;
        ifstream is(server_list_path.c_str());
        while (is >> line) {
            servers.push_back(line);
        }
    }

    vector<SearchRequest> reqs;

    if (query_list_path.size()) {
        ifstream is(query_list_path.c_str());
        load_queries(is, search, content, &reqs);
    }
    else {
        cerr << "No query list file provided, loading from stdin." << endl;
        load_queries(cin, search, content, &reqs);
    }

    Stress stress(config, servers, std::move(reqs), once, keepalive);
    stress.run();

    return 0;
}
