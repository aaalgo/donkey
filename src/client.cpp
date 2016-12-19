#include <omp.h>
#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include "donkey.h"

using namespace std;
using namespace donkey;

struct Task {
    string key;
    string url;
    string meta;
    // return whether successful
    bool parse (string const &line) {
        size_t off = line.find('\t');
        if (off == string::npos || (off + 1) >= line.size()) {
            return false;
        }
        size_t off2 = line.find('\t', off+1);
        if (off2 == string::npos) {
            off2 = line.size();
        }
        key = line.substr(0, off);
        url = line.substr(off+1, off2-off-1);
        if (off2 + 1 < line.size()) {
            meta = line.substr(off2 + 1);
        }
        return true;
    }
};

class Tasks: public vector<Task> {
public:
    Tasks () {
        // load all jobs from cin
        string line;
        while (getline(cin, line)) {
            Task task;
            if (task.parse(line)) {
                push_back(task);
            }
            else {
                cerr << "Bad line: " << line << endl;
            }
        }
    }
};

string format_hit (string const &fmt, Task const &task, Hit const &hit) {
    string v;
    unsigned o = 0;
    while (o < fmt.size()) {
        if (fmt[o] == '%') {
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
            else if (c == 'K') {
                v += task.key;
            }
            else if (c == 'U') {
                v += task.url;
            }
            else if (c == 'M') {
                v += task.meta;
            }
            else BOOST_VERIFY(0);
        }
        else if (fmt[o] == '\\') {
            ++o;
            if (o >= fmt.size()) break;
            char c = fmt[o++];
            if (c == 't') {
                v.push_back('\t');
            }
            else if (c == '\\') {
                v.push_back('\\');
            }
            else BOOST_VERIFY(0);
        }
        else {
            v.push_back(fmt[o]);
            ++o;
        }
    }
    return v;
}

int main (int argc, char *argv[]) {
    string config_path;
    vector<string> overrides;
    string method;
    string type;
    string rfmt;
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
        ("rfmt", po::value(&rfmt)->default_value("%k\t%s\t%m"), "response format")
        ("hfmt", po::value(&hfmt)->default_value("%K => %k\t%s\t%m"), "hit format")
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
    if (method == "extract") {
        ;
    }
    else if (vm.count("embed")) {
        client = new Server(config);
    }
    else {
        try {
            client = make_client(config);
        }
        catch (exception const &e) {
            cerr << "Failed to create client: " << e.what() << endl;
            return -1;
        }

    }
    if (method == "ping") {
        PingResponse resp;
        client->ping(&resp);
        cout << resp.restart_count << endl;
        cout << resp.last_start_time << endl;
        cout << resp.first_start_time << endl;
    }
    else if (method == "extract") {
        Tasks tasks;
        Extractor xtor(config);
#pragma omp parallel
        {
#pragma omp for schedule(dynamic, 1)
            for (unsigned i = 0; i < tasks.size(); ++i) {
                Task const &task = tasks[i];
                Object object;
                xtor.extract_path(task.url, type, &object);
                if (task.meta.size()) {
                    ofstream os(task.meta, std::ios::binary);
                    object.write(os);
                }
            }
        }
    }
    else if (method == "insert") {
        Tasks tasks;
        vector<InsertRequest> reqs(tasks.size());
        vector<InsertResponse> resps(tasks.size());
#pragma omp parallel
        {
            Service *th_client = client;
            int th = omp_get_thread_num();
            if ((vm.count("embed") == 0) && (th != 0)) { // create clients for new threads
#pragma omp critical
                th_client = make_client(config);
            }
#pragma omp for schedule(dynamic, 1)
            for (unsigned i = 0; i < tasks.size(); ++i) {
                Task const &task = tasks[i];
                InsertRequest &req = reqs[i];
                InsertResponse &resp = resps[i];
                req.db = db;
                req.raw = raw;
                req.key = task.key;
                req.type = type;
                req.url = task.url;
                req.meta = task.meta;
                if (content) {
                    ReadURL(req.url, &req.content);
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
        for (unsigned i = 0; i < tasks.size(); ++i) {
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
                //client->misc(req, &resp);
                client->misc(req, &resp);
            }
        }
    }
    else if (method == "search") {
        Tasks tasks;
        vector<SearchRequest> reqs(tasks.size(), search);
        vector<SearchResponse> resps(tasks.size());
#pragma omp parallel
        {
            Service *th_client = client;
            int th = omp_get_thread_num();
            if ((vm.count("embed") == 0) && (th != 0)) { // create clients for new threads
#pragma omp critical
                th_client = make_client(config);
            }
#pragma omp for schedule(dynamic, 1)
            for (unsigned i = 0; i < tasks.size(); ++i) {
                Task const &task = tasks[i];
                SearchRequest &req = reqs[i];
                SearchResponse &resp = resps[i];
                req.db = db;
                req.raw = raw;
                req.type = type;
                if (content) {
                    ReadFile(task.url, &req.content);
                }
                else {
                    req.url = task.url;
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
        for (unsigned i = 0; i < tasks.size(); ++i) {
            Task const &task = tasks[i];
            auto &resp = resps[i];
            /*
            if (verbose) {
                cout << task.key << ": " << resp.time << '\t' << resp.load_time << '\t' << resp.filter_time << '\t' << resp.rank_time << endl;
            }
            */
            for (auto const &h: resp.hits) {
                cout << format_hit(hfmt, task, h) << endl;
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
    if (client) delete client;
    cleanup_logging();
    return 0;
}
