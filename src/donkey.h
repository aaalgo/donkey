#ifndef AAALGO_DONKEY
#define AAALGO_DONKEY

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <limits>
#include <functional>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/timer/timer.hpp>
#include <boost/assert.hpp>
#include "fnhack.h"

// common stuff
namespace donkey {

    using std::ios;
    using std::function;
    using std::string;
    using std::runtime_error;
    using std::numeric_limits;
    using std::fstream;
    using std::array;
    using std::vector;
    using std::unordered_map;
    using std::istream;
    using std::ostream;
    using std::ifstream;
    using std::ofstream;
    using std::isnormal;
    using boost::shared_mutex;
    using boost::unique_lock;
    using boost::shared_lock;
    using boost::upgrade_lock;
    using boost::upgrade_to_unique_lock;

    // maximal number of DBs
    static constexpr size_t DEFAULT_MAX_DBS = 256;
    static constexpr size_t MAX_FEATURES = 10000;   // maximal features per object
    static constexpr size_t MAX_BINARY = 128 * 1024 * 1024; // maximal raw object size, 128M

    // Configuration
    typedef boost::property_tree::ptree Config;

    void LoadConfig (string const &path, Config *);
    // we allow overriding configuration options in the form of "KEY=VALUE"
    void OverrideConfig (std::vector<std::string> const &overrides, Config *);

    // Logging
#define LOG(x) BOOST_LOG_TRIVIAL(x)
    namespace logging = boost::log;
    void setup_logging (Config const &);

    //void DumpStack ();

    // Timing a piece of code, usage
    // {
    //      double time;
    //      Timer timer(&time);
    //      // code to be timed
    // }
    class Timer {
        boost::timer::cpu_timer timer;
        double *result;
    public:
        Timer (double *r): result(r) {
            BOOST_VERIFY(r);
        }
        ~Timer () {
            *result = timer.elapsed().wall/1e9;
        }
    };

    static constexpr int64_t ErrorCode_Success = 0;
    static constexpr int64_t ErrorCode_Unknown = -1;

    class Error: public runtime_error {
        int32_t c;
    public:
        explicit Error (string const &what): runtime_error(what), c(ErrorCode_Unknown) {}
        explicit Error (char const *what): runtime_error(what), c(ErrorCode_Unknown) {}
        explicit Error (string const &what, int32_t code): runtime_error(what), c(code) {}
        explicit Error (char const *what, int32_t code): runtime_error(what), c(code) {}
        virtual int32_t code () const {
            return c;
        }
    };

#define DEFINE_ERROR(name, cc) \
    class name: public Error { \
    public: \
        explicit name (string const &what): Error(what) {} \
        explicit name (char const *what): Error(what) {} \
        virtual int32_t code () const { return cc;} \
    }

    DEFINE_ERROR(InternalError, 0x0001);
    DEFINE_ERROR(ExternalError, 0x0002);
    DEFINE_ERROR(OutOfMemoryError, 0x0004);
    DEFINE_ERROR(FileSystemError, 0x0008);
    DEFINE_ERROR(RequestError, 0x0016);
    DEFINE_ERROR(ConfigError, 0x0032);
    DEFINE_ERROR(PluginError, 0x0064);
#undef DEFINE_ERROR

    struct Feature;
    struct Object;

    // matching of a single feature within object
    struct Hint {   
        // tag: object may have multiple features, tag is the index of feature
        // tag could also have other meaning in a more generic setting.
        uint32_t dtag;  // data tag, tag of a matched feature
        uint32_t qtag;  // query tag
        float value;    // = distance for k-nn search, could be of other meanings
                        // that's why we don't call it distance
    };

    struct Candidate {
        Object const *object;
        vector<Hint> hints;
    };

    struct Hit {
        string key;
        string meta;
        string details;
        float score;
    };

    struct ObjectRequest {
        bool raw;           // whether the input is a raw object, or a feature file.
        string url;      // if url starts with http://, etc, it is downloaded from web
                            // otherwise it's treated as a local path.
        string content;     // the content of object
                            // one and only one of object and content may be set
        string type;        // passed to feature extraction plugin for extra info
    };

    struct SearchRequest: public ObjectRequest {
        uint16_t db;
        int32_t K;
        float R;
        int32_t hint_K;
        float hint_R;
    };

    struct SearchResponse {
        vector<Hit> hits;
        double time;
        double load_time;
        double filter_time;
        double rank_time;
    };

    struct InsertRequest: public ObjectRequest {
        uint16_t db;
        string key;
        string meta;
    };

    struct InsertResponse {
        double time;
        double load_time;
        double journal_time;
        double index_time;
    };

    struct MiscRequest {
        string method;
        int16_t db;
    };

    struct MiscResponse {
        int code;
        string text;
    };

    // Index is not synchronized -- usually insertions are done in batch,
    // even adding a single object could involve multiple insertions.
    // synchronizing individual insertion could be too expensive.
    // Synchronization is done at DB level.
    class Index {
    protected:
        int default_K;      // hint_K, if input hint_K is <= 0, must replace with this
        float default_R;    // hint_R, if input hint_R is not normal, must replace with this
    public:
        struct Match {
            uint32_t object;
            uint32_t tag;
            float distance;
        };
        Index (Config const &config);
        virtual ~Index () = default;
        virtual void search (Feature const &query, SearchRequest const &params, std::vector<Match> *) const = 0;
        virtual void insert (uint32_t object, uint32_t tag, Feature const *feature) = 0;
        virtual void clear () = 0;
        virtual void rebuild () = 0;
    };

    Index *create_kgraph_index (Config const &);
    Index *create_lsh_index (Config const &);

    // utility functions
    
    static inline void ReadFile (const std::string &path, std::string *binary) {
        // file could not be too big
        binary->clear();
        ifstream is(path.c_str(), std::ios::binary);
        if (!is) return;
        is.seekg(0, std::ios::end);
        size_t size = size_t(is.tellg());
        if (size > MAX_BINARY) return;
        binary->resize(size);
        if (size) {
            is.seekg(0, std::ios::beg);
            is.read((char *)&binary->at(0), size);
        }
        if (!is) binary->clear();
    }

    static inline void WriteFile (const std::string &path, std::string const &binary) {
        // file could not be too big
        ofstream os(path.c_str(), ios::binary);
        os.write(&binary[0], binary.size());
    }

    struct ObjectBase {
        string meta;
    };

    // Feature extractor interface.
    class ExtractorBase {
    public:
        virtual ~ExtractorBase () {}
        virtual void extract_path (string const &path, string const &type, Object *object) const {
            string content;
            ReadFile(path, &content);
            extract(content, type, object);
        }
        virtual void extract (string const &content, string const &type, Object *object) const; 
    };
}

// data-type-specific configuration
#include "plugin/config.h"

namespace donkey {

    static inline float default_R () {
        if (Matcher::POLARITY >= 0) {
            return -numeric_limits<float>::max();
        }
        else {
            return numeric_limits<float>::max();
        }
    }

    static inline float default_hint_R () {
        if (FeatureSimilarity::POLARITY > 0) {
            return -numeric_limits<float>::max();
        }
        else {
            return numeric_limits<float>::max();
        }
    }

    // append & sync are protected.
    class Journal {
        static uint32_t const MAGIC = 0xdeadface;
        string path;           // path to journal file
        ofstream str;               // only opened after recover is invoked
        std::mutex mutex;

        struct __attribute__ ((__packed__)) RecordHead {
            uint32_t magic;
            uint16_t dbid;
            uint16_t key_size;
            uint32_t meta_size;
        };

    public:
        Journal (string const &path_)
              : path(path_) {
        }

        ~Journal () {
            if (str.is_open()) {
                sync();
            }
        }

        // fastforward: directly jump to the given offset
        // return maxid
        int recover (function<void(uint16_t dbid, string const &key, string const &meta, Object *object)> callback) {
            size_t off = 0;
            int count = 0;
            do {
                ifstream is(path.c_str(), std::ios::binary);
                if (!is) {
                    LOG(warning) << "Fail to open journal file.";
                    LOG(warning) << "Overwriting...";
                    break;
                }
                for (;;) {
                    RecordHead head;
                    is.read(reinterpret_cast<char *>(&head), sizeof(head));
                    if (!is) break;
                    if (head.magic != MAGIC) {
                        LOG(warning) << "Corrupted journal, truncating at " << off;
                        break;
                    }
                    string key;
                    string meta;
                    key.resize(head.key_size);
                    is.read((char *)&key[0], key.size());
                    meta.resize(head.meta_size);
                    is.read((char *)&meta[0], meta.size());
                    Object object;
                    object.read(is);
                    if (!is) break;
                    callback(head.dbid, key, meta, &object);
                    ++count;
                    off = is.tellg();
                }
                is.close();
                LOG(info) << "Journal recovered.";
                LOG(info) << count << " items loaded.";
                LOG(info) << "Offset is " << off << ".";
            
            }
            while (false);

            {
                int fd = ::open(path.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
                if (fd < 0) {
                    LOG(fatal) << "Cannot open journal file.";
                    BOOST_VERIFY(0);
                }
                int r = ::ftruncate(fd, off);
                if (r) {
                    LOG(error) << "Cannot truncate journal file, appending anyway.";
                }
                close(fd);
            }
            str.open(path.c_str(), ios::binary | ios::app);
            BOOST_VERIFY(str.is_open());
        }

        void append (uint16_t dbid, string const &key, string const &meta, Object const &object) {
            BOOST_VERIFY(str.is_open());
            std::lock_guard<std::mutex> lock(mutex); 
            RecordHead head;
            head.magic = MAGIC;
            head.dbid = dbid;
            head.key_size = key.size();
            head.meta_size = meta.size();
            str.write(reinterpret_cast<char const *>(&head), sizeof(head));
            BOOST_VERIFY(str);
            str.write(&key[0], key.size());
            BOOST_VERIFY(str);
            str.write(&meta[0], meta.size());
            BOOST_VERIFY(str);
            object.write(str);
        }

        void sync () {
            BOOST_VERIFY(str.is_open());
            std::lock_guard<std::mutex> lock(mutex); 
            str.flush();
            ::fsync(fileno_hack(str));
        }
    };

    class DB {
        struct Record {
            string key;
            string meta;
            Object object;
        };
        vector<Record *> records;
        Index *index;
        mutable shared_mutex mutex;
        Matcher matcher;
        SearchRequest defaults;
        int default_K;
        float default_R;
    public:
        DB (Config const &config) 
            : index(nullptr),
            matcher(config),
            default_K(config.get<int>("donkey.defaults.K", 1)),
            default_R(config.get<float>("donkey.defaults.R", donkey::default_R()))
        {
            if (default_K <= 0) throw ConfigError("invalid defaults.K");
            if (!isnormal(default_R)) throw ConfigError("invalid defaults.R");

            string algo = config.get<string>("donkey.index.algorithm", "lsh");
            if (algo == "lsh") {
                index = create_lsh_index(config);
            }
            else if (algo == "kgraph") {
                index = create_kgraph_index(config);
            }
            else throw ConfigError("unknown index algorithm");
            BOOST_VERIFY(index);
        }

        ~DB () {
            clear();
            delete index;
        }

        void insert (string const &key, string const &meta, Object *object) {
            Record *rec = new Record;
            rec->key = key;
            rec->meta = meta;
            object->swap(rec->object);
            unique_lock<shared_mutex> lock(mutex);
            size_t id = records.size();
            records.push_back(rec);
            rec->object.enumerate([this, id](unsigned tag, Feature const *ft) {
                index->insert(id, tag, ft);
            });
        }

        void search (Object const &object, SearchRequest const &params, SearchResponse *response) const {
            unordered_map<unsigned, Candidate> candidates;
            {
                Timer timer(&response->filter_time);
                shared_lock<shared_mutex> lock(mutex);
                object.enumerate([this, &params, &candidates](unsigned qtag, Feature const *ft) {
                    vector<Index::Match> matches;
                    index->search(*ft, params, &matches);
                    for (auto const &m: matches) {
                        auto &c = candidates[m.object];
                        Hint hint;
                        hint.dtag = m.tag;
                        hint.qtag = qtag;
                        hint.value = m.distance;
                        c.hints.push_back(hint);
                    }
                });
            }
            {
                Timer timer(&response->rank_time);
                response->hits.clear();
                float R = params.R;
                if (!std::isnormal(R)) {
                    R = default_R;
                }

                int K = params.K;
                if (K <= 0) {
                    K = default_K;
                }

                for (auto &pair: candidates) {
                    unsigned id = pair.first;
                    Candidate &cand = pair.second;
                    cand.object = &records[id]->object;
                    float score = matcher.apply(object, cand);
                    bool good = false;
                    if (Matcher::POLARITY >= 0) {
                        good = score >= R;
                    }
                    else {
                        good = score <= R;
                    }
                    if (good) {
                        Hit hit;
                        hit.key = records[id]->key;
                        hit.meta = records[id]->meta;
                        hit.score = score;
                        response->hits.push_back(hit);
                    }
                }
                if (Matcher::POLARITY < 0) {
                    sort(response->hits.begin(),
                         response->hits.end(),
                         [](Hit const &h1, Hit const &h2) { return h1.score < h2.score;});
                }
                else {
                    sort(response->hits.begin(),
                         response->hits.end(),
                         [](Hit const &h1, Hit const &h2) { return h1.score > h2.score;});
                }
                if (response->hits.size() > K) {
                    response->hits.resize(K);
                }
            }
        }

        void clear () {
            unique_lock<shared_mutex> lock(mutex);
            index->clear();
            for (auto record: records) {
                delete record;
            }
            records.clear();
        }

        void reindex () {
            unique_lock<shared_mutex> lock(mutex);
            // TODO: this can be improved
            // The index building part doesn't requires read-lock only
            index->rebuild();
        }
    };

    class Service {
    public:
        virtual ~Service () = default;
        virtual void ping () const = 0;
        virtual void insert (InsertRequest const &request, InsertResponse *response) = 0;
        virtual void search (SearchRequest const &request, SearchResponse *response) const = 0;
        virtual void misc (MiscRequest const &request, MiscResponse *response) = 0;
    };

    class Server: public Service {
        string root;
        Journal journal;
        vector<DB *> dbs;
        Extractor xtor;

        void check_dbid (uint16_t dbid) const {
            BOOST_VERIFY(dbid < dbs.size());
        }

        void loadObject (ObjectRequest const &request, Object *object) const; 

    public:
        Server (Config const &config)
            : root(config.get<string>("donkey.root")),
            journal(root + "/journal"),
            dbs(config.get<size_t>("donkey.max_dbs", DEFAULT_MAX_DBS), nullptr),
            xtor(config)
        {
            // create empty dbs
            for (auto &db: dbs) {
                db = new DB(config);
            }
            // recover journal 
            journal.recover([this](uint16_t dbid, string const &key, string const &meta, Object *object){
                try {
                    dbs[dbid]->insert(key, meta, object);
                }
                catch (...) {
                }
            });
            // reindex all dbs
            for (auto db: dbs) {
                db->reindex();
            }
        }

        ~Server () { // close all dbs
            for (DB *db: dbs) {
                delete db;
            }
        }

        void ping () const {
        }

        void insert (InsertRequest const &request, InsertResponse *response) {
            Timer timer(&response->time);
            check_dbid(request.db);
            Object object;
            {
                Timer timer1(&response->load_time);
                loadObject(request, &object);
            }
            {
                Timer timer2(&response->journal_time);
                journal.append(request.db, request.key, request.meta, object);
            }
            {
                Timer timer3(&response->index_time);
                // must come after journal, as db insert could change object content
                dbs[request.db]->insert(request.key, request.meta, &object);
            }
        }

        void search (SearchRequest const &request, SearchResponse *response) const {
            Timer timer(&response->time);
            check_dbid(request.db);
            Object object;
            {
                Timer timer1(&response->load_time);
                loadObject(request, &object);
            }
            dbs[request.db]->search(object, request, response);
        }

        void misc (MiscRequest const &request, MiscResponse *response) {
            LOG(info) << "misc operation: " << request.method;
            if (request.method == "reindex") {
                check_dbid(request.db);
                dbs[request.db]->reindex();
            }
            else if (request.method == "clear") {
                check_dbid(request.db);
                dbs[request.db]->clear();
            }
            else if (request.method == "sync") {
                journal.sync();
            }
            response->code = 0;
        }
    };

    void run_server (Config const &, Server *);

    Service *make_client (Config const &);

}

#endif

