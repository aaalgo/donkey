#include <omp.h>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "donkey.h"

using namespace std;
using namespace donkey;

string format_hit (string const &fmt, Hit const &hit) {
    string v;
    unsigned o = 0;
    while (o < fmt.size()) {
        if (fmt[o] != '%') {
            v.push_back(fmt[o]);
            ++o;
            continue;
        }
        ++o;
        if (o >= fmt.size()) break;
        char c = fmt[o++];
        if (c == '%') {
            v.push_back('%');
        }
        else if (c == 'k') {
            v += hit.key;
        }
        else if (c == 'm') {
            v += hit.meta;
        }
        else if (c == 'd') {
            v += hit.details;
        }
        else if (c == 's') {
            v += boost::lexical_cast<string>(hit.score);
        }
        else BOOST_VERIFY(0);
    }
    return v;
}

int main (int argc, char *argv[]) {
    string config_path;
    vector<string> overrides;
    string method;
    string type;
    string hfmt;
    uint32_t db;
    bool raw;
    bool content;
    bool verbose;
    SearchRequest search;
    

    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "produce help message.")
        ("config", po::value(&config_path)->default_value("donkey.xml"), "")
        ("override,D", po::value(&overrides), "override configuration.")
        ("method", po::value(&method)->default_value("ping"), "")
        ("db", po::value(&db)->default_value(0), "")
        ("feature", "")
        ("content", "")
        ("type", po::value(&type), "")
        (",K", po::value(&search.K)->default_value(-1), "")
        (",R", po::value(&search.R)->default_value(NAN), "")
        ("hint_K", po::value(&search.hint_K)->default_value(-1), "")
        ("hint_R", po::value(&search.hint_R)->default_value(NAN), "")
        ("hfmt", po::value(&hfmt)->default_value("%k\t%s\t%m"), "hit format")
        ("embed", "")
        ("no-sync", "")
        ("no-reindex", "")
        ("verbose,v", "")
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

    raw = !vm.count("feature");
    content = vm.count("content");
    verbose = vm.count("verbose");

    Config config;
    LoadConfig(config_path, &config);
    OverrideConfig(overrides, &config);

    setup_logging(config);

    Service *client = nullptr;
    if (vm.count("embed")) {
        client = new Server(config);
    }
    else {
        client = make_client(config);
    }
    if (method == "ping") {
        client->ping();
    }
    else if (method == "insert") {
        vector<string> lines;
        {   // load all jobs
            string line;
            while (getline(cin, line)) {
                lines.push_back(line);
            }
        }
        vector<InsertRequest> reqs(lines.size());
        vector<InsertResponse> resps(lines.size());
#pragma omp parallel
        {
            Service *th_client = client;
            int th = omp_get_thread_num();
            if ((vm.count("embed") == 0) && (th != 0)) { // create clients for new threads
#pragma omp critical
                th_client = make_client(config);
            }
#pragma omp for
            for (unsigned i = 0; i < lines.size(); ++i) {
                string const &line = lines[i];
                size_t off = line.find('\t');
                if (off == string::npos || (off + 1) >= line.size()) {
                    cerr << "Bad line: " << line << endl;
                    continue;
                }
                size_t off2 = line.find('\t', off+1);
                if (off2 == string::npos) {
                    off2 = line.size();
                }
                InsertRequest &req = reqs[i];
                InsertResponse &resp = resps[i];
                req.db = db;
                req.raw = raw;
                req.key = line.substr(0, off);
                req.type = type;
                req.url = line.substr(off+1, off2 - off-1);
                if (off2 + 1 < line.size()) {
                    req.meta = line.substr(off2 + 1);
                }
                if (content) {
                    ReadFile(req.url, &req.content);
                    req.url.clear();
                }
                try {
                    th_client->insert(req, &resp);
                }
                catch (Error const &e) {
                    cerr << "Error " << e.code() << ": " << e.what() << endl;
                }
                req.content.clear();
            }
            if (th_client != client) {
                delete th_client;
            }
        }
        for (unsigned i = 0; i < lines.size(); ++i) {
            auto &req = reqs[i];
            auto &resp = resps[i];
            cout << req.key;
            if (verbose) {
                cout << '\t' << resp.time << '\t' << resp.load_time << '\t' << resp.journal_time << '\t' << resp.index_time << endl;
            }
            cout << endl;
        }
        {
            MiscRequest req;
            MiscResponse resp;
            req.db = db;
            if (vm.count("no-reindex") == 0) {
                req.method = "reindex";
                client->misc(req, &resp);
            }
            if (vm.count("no-sync") == 0) {
                req.method = "sync";
                client->misc(req, &resp);
                client->misc(req, &resp);
            }
        }
    }
    else if (method == "search") {
        vector<string> keys;
        vector<string> urls;
        {
            string key;
            string url;
            while (cin >> key >> url) {
                keys.push_back(key);
                urls.push_back(url);
            }
        }
        vector<SearchRequest> reqs(keys.size(), search);
        vector<SearchResponse> resps(keys.size());
#pragma omp parallel
        {
            Service *th_client = client;
            int th = omp_get_thread_num();
            if ((vm.count("embed") == 0) && (th != 0)) { // create clients for new threads
#pragma omp critical
                th_client = make_client(config);
            }
#pragma omp for
            for (unsigned i = 0; i < keys.size(); ++i) {
                string const &url = urls[i];
                SearchRequest &req = reqs[i];
                SearchResponse &resp = resps[i];
                req.db = db;
                req.raw = raw;
                req.type = type;
                if (content) {
                    ReadFile(url, &req.content);
                }
                else {
                    req.url = url;
                }
                try {
                    th_client->search(req, &resp);
                }
                catch (Error const &e) {
                    cerr << "Error " << e.code() << ": " << e.what() << endl;
                }
                req.content.clear();
            }
            if (th_client != client) {
                delete th_client;
            }
        }
        for (unsigned i = 0; i < keys.size(); ++i) {
            string const &key = keys[i];
            auto &resp = resps[i];
            if (verbose) {
                cout << key << ": " << resp.time << '\t' << resp.load_time << '\t' << resp.filter_time << '\t' << resp.rank_time << endl;
            }
            for (auto const &h: resp.hits) {
                cout << key << " => " << format_hit(hfmt, h) << endl;
            }
            if (resp.hits.empty()) {
                cout << key << endl;
            }
        }
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
    cleanup_logging();
    return 0;
}
