#ifndef AAALGO_DONKEY
#define AAALGO_DONKEY

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/timer/timer.hpp>
#include <boost/assert.hpp>
#include <kgraph.h>
#include "fnhack.h"

// common stuff
namespace donkey {

    using std::ios;
    using std::function;
    using std::string;
    using std::runtime_error;
    using std::fstream;
    using std::array;
    using std::vector;
    using std::unordered_map;
    using std::istream;
    using std::ostream;
    using std::ifstream;
    using std::ofstream;
    using boost::shared_mutex;
    using boost::unique_lock;
    using boost::shared_lock;
    using boost::upgrade_lock;
    using boost::upgrade_to_unique_lock;

    // maximal number of DBs
    static constexpr size_t DEFAULT_MAX_DBS = 256;
    static constexpr size_t MAX_FEATURES = 10000;
    static constexpr size_t MAX_BINARY = 128 * 1024 * 1024;

    // Configuration
    typedef boost::property_tree::ptree Config;

    void LoadConfig (string const &path, Config *);
    void OverrideConfig (std::vector<std::string> const &overrides, Config *);

    // Logging
#define LOG(x) BOOST_LOG_TRIVIAL(x)
    namespace logging = boost::log;
    void setup_logging (Config const &);

    //void DumpStack ();

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

    class Error: public runtime_error {
    public:
        explicit Error (string const &what): runtime_error(what) {}
        explicit Error (char const *what): runtime_error(what) {}
        virtual int64_t code () const = 0;
    };

#define DEFINE_ERROR(name, cc) \
    class name: public Error { \
    public: \
        explicit name (string const &what): Error(what) {} \
        explicit name (char const *what): Error(what) {} \
        virtual int64_t code () const { return cc;} \
    }

    static constexpr int64_t ErrorCode_Success = 0;
    static constexpr int64_t ErrorCode_Unknown = -1;
    DEFINE_ERROR(OutOfMemoryError, 0x0001);
    DEFINE_ERROR(FileSystemError, 0x0002);
    DEFINE_ERROR(RequestError, 0x0002);
    DEFINE_ERROR(ExternalError, 0x0002);
#undef DEFINE_ERROR

    struct Feature;
    struct Object;

    // matching of a single feature within object
    struct Hint {   
        uint32_t dtag;  // data tag, tag of a matched feature
        uint32_t qtag;  // query tag
        float value;    // = distance for k-nn search
    };

    struct Candidate {
        Object const *object;
        vector<Hint> hints;
    };

    struct Hit {
        string key;
        float score;
    };

    struct ObjectRequest {
        bool raw;
        string url;
        string content;
        string type;
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
    class Index {
    public:
        struct Match {
            uint32_t object;
            uint32_t tag;
            float distance;
        };
        virtual ~Index () {
        }
        virtual void search (Feature const &query, SearchRequest const &params, std::vector<Match> *) const = 0;
        virtual void insert (uint32_t object, uint32_t tag, Feature const *feature) = 0;
        virtual void clear () = 0;
        virtual void rebuild () = 0;
    };

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

    class ExtractorBase {
    public:
        virtual ~ExtractorBase () {}
        virtual void extract_path (string const &path, Object *object) const {
            string content;
            ReadFile(path, &content);
            extract(content, object);
        }
        virtual void extract (string const &content, Object *object) const; 
    };

}

// data-type-specific configuration
#include "plugin/config.h"

#include <kgraph.h>

namespace donkey {

    using kgraph::KGraph;

    // Index is not mutex-protected.
    class KGraphIndex: public Index {
        struct Entry {
            uint32_t object;
            uint32_t tag;
            Feature const *feature;
        };
        size_t indexed_size;
        vector<Entry> entries;

        friend class IndexOracle;
        friend class SearchOracle;

        class IndexOracle: public kgraph::IndexOracle {
            KGraphIndex *parent;
        public:
            IndexOracle (KGraphIndex *p): parent(p) {
            }
            virtual unsigned size () const {
                return parent->entries.size();
            }   
            virtual float operator () (unsigned i, unsigned j) const {
                return (-FeatureSimilarity::POLARITY) *
                       FeatureSimilarity::apply(*parent->entries[i].feature,
                                *parent->entries[j].feature);
            }   
        };  

        class SearchOracle: public kgraph::SearchOracle {
            KGraphIndex const *parent;
            Feature const &query;
        public:
            SearchOracle (KGraphIndex const *p, Feature const &q): parent(p), query(q) {
            }   
            virtual unsigned size () const {
                return parent->indexed_size;
            }   
            virtual float operator () (unsigned i) const {
                return FeatureSimilarity::apply(*parent->entries[i].feature, query);
            }   
        };

        KGraph::IndexParams index_params;
        KGraph::SearchParams search_params;
        KGraph *kg_index;
    public:
        KGraphIndex (Config const &config): indexed_size(0), kg_index(nullptr) {
            index_params.iterations = config.get<unsigned>("donkey.kgraph.index.iterations", index_params.iterations);
            index_params.L = config.get<unsigned>("donkey.kgraph.index.L", index_params.L);
            index_params.K = config.get<unsigned>("donkey.kgraph.index.K", index_params.K);
            index_params.S = config.get<unsigned>("donkey.kgraph.index.S", index_params.S);
            index_params.R = config.get<unsigned>("donkey.kgraph.index.R", index_params.R);
            index_params.controls = config.get<unsigned>("donkey.kgraph.index.controls", index_params.controls);
            index_params.seed = config.get<unsigned>("donkey.kgraph.index.seed", index_params.seed);
            index_params.delta = config.get<float>("donkey.kgraph.index.delta", index_params.delta);
            index_params.recall = config.get<float>("donkey.kgraph.index.recall", index_params.recall);
            index_params.prune = config.get<unsigned>("donkey.kgraph.index.prune", index_params.prune);

            search_params.K = config.get<unsigned>("donkey.kgraph.search.K", search_params.K);
            search_params.M = config.get<unsigned>("donkey.kgraph.search.M", search_params.M);
            search_params.P = config.get<unsigned>("donkey.kgraph.search.P", search_params.P);
            search_params.T = config.get<unsigned>("donkey.kgraph.search.T", search_params.T);
            search_params.epsilon = config.get<float>("donkey.kgraph.search.epsilon", search_params.epsilon);
            search_params.seed = config.get<unsigned>("donkey.kgraph.search.seed", search_params.seed);
        }

        ~KGraphIndex () {
            if (kg_index) {
                delete kg_index;
            }
        }

        virtual void search (Feature const &query, SearchRequest const &sp, std::vector<Match> *matches) const {
            matches->clear();
            if (kg_index) {
                SearchOracle oracle(this, query);
                KGraph::SearchParams params(search_params);
                params.K = sp.hint_K;
                params.epsilon = sp.hint_R;
                // update search params
                vector<unsigned> ids(params.K);
                vector<float> dists(params.K);
                //unsigned L = kg_index->search(oracle, params, &ids[0], &dists[0], nullptr);
                unsigned L = oracle.search(params.K, params.epsilon, &ids[0], &dists[0]);
                matches->resize(L);
                for (unsigned i = 0; i < L; ++i) {
                    auto &m = matches->at(i);
                    auto const &e = entries[ids[i]];
                    m.object = e.object;
                    m.tag = e.tag;
                    m.distance = dists[i];
                }
            }
            BOOST_VERIFY(indexed_size == entries.size());
        }

        virtual void insert (uint32_t object, uint32_t tag, Feature const *feature) {
            Entry e;
            e.object = object;
            e.tag = tag;
            e.feature = feature;
            entries.push_back(e);
        }

        virtual void clear () {
            if (kg_index) {
                delete kg_index;
                kg_index = nullptr;
            }
            entries.clear();
        }

        virtual void rebuild () {   // insert must not happen at this time
            if (entries.size() == indexed_size) return;
            KGraph *kg = KGraph::create();
            LOG(info) << "Rebuilding index for " << entries.size() << " features.";
            IndexOracle oracle(this);
            kg->build(oracle, index_params, NULL);
            LOG(info) << "Swapping on new index...";
            indexed_size = entries.size();
            std::swap(kg, kg_index);
            if (kg) {
                delete kg;
            }
        }
    };

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
        int recover (function<void(uint16_t dbid, string const &key, Object *object)> callback) {
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
                    key.resize(head.key_size);
                    is.read((char *)&key[0], key.size());
                    Object object;
                    object.read(is);
                    if (!is) break;
                    callback(head.dbid, key, &object);
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
                int fd = ::open(path.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
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

        void append (uint16_t dbid, string const &key, Object const &object) {
            BOOST_VERIFY(str.is_open());
            std::lock_guard<std::mutex> lock(mutex); 
            RecordHead head;
            head.magic = MAGIC;
            head.dbid = dbid;
            head.key_size = key.size();
            str.write(reinterpret_cast<char const *>(&head), sizeof(head));
            BOOST_VERIFY(str);
            str.write(&key[0], key.size());
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
            Object object;
        };
        vector<Record *> records;
        Index *index;
        mutable shared_mutex mutex;
        Matcher matcher;
    public:
        DB (Config const &config) 
            : index(new KGraphIndex(config)),
            matcher(config)
        {
            BOOST_VERIFY(index);
        }

        ~DB () {
            clear();
            delete index;
        }

        void insert (string const &key, Object *object) {
            Record *rec = new Record;
            rec->key = key;
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
                float R = params.R * Matcher::POLARITY;
                for (auto &pair: candidates) {
                    unsigned id = pair.first;
                    Candidate &cand = pair.second;
                    cand.object = &records[id]->object;
                    float score = matcher.apply(object, cand) * Matcher::POLARITY;
                    if (score  >= R) {
                        Hit hit;
                        hit.key = records[id]->key;
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
                if (response->hits.size() > params.K) {
                    response->hits.resize(params.K);
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
        virtual ~Service () {
        }
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
            journal.recover([this](uint16_t dbid, string const &key, Object *object){
                try {
                    dbs[dbid]->insert(key, object);
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
                journal.append(request.db, request.key, object);
            }
            {
                Timer timer3(&response->index_time);
                // must come after journal, as db insert could change object content
                dbs[request.db]->insert(request.key, &object);
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

