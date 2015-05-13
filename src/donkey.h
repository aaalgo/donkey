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
    using std::ifstream;
    using std::ofstream;
    using boost::shared_mutex;
    using boost::unique_lock;
    using boost::shared_lock;
    using boost::upgrade_lock;
    using boost::upgrade_to_unique_lock;

    // maximal number of DBs
    static constexpr size_t DEFAULT_MAX_DBS = 256;

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
#undef DEFINE_ERROR

}

namespace donkey {

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

    struct SearchParams {
        unsigned K;
        float R;
        unsigned hint_K;
        float hint_R;
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
        virtual void search (Feature const &query, SearchParams const &params, std::vector<Match> *) const = 0;
        virtual void insert (uint32_t object, uint32_t tag, Feature const *feature) = 0;
        virtual void clear () = 0;
        virtual void rebuild () = 0;
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
                return Distance::apply(*parent->entries[i].feature,
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
                return Distance::apply(*parent->entries[i].feature, query);
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

        virtual void search (Feature const &query, SearchParams const &sp, std::vector<Match> *matches) const {
            matches->clear();
            if (kg_index) {
                SearchOracle oracle(this, query);
                KGraph::SearchParams params(search_params);
                params.K = sp.hint_K;
                params.epsilon = sp.hint_R;
                // update search params
                vector<unsigned> ids(params.K);
                vector<float> dists(params.K);
                unsigned L = kg_index->search(oracle, params, &ids[0], &dists[0], nullptr);
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
            LOG(info) << "Rebuilding index...";
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
                    if (!is) break;
                    Object object;
                    object.read(is);
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

        void search (Object const &object, SearchParams const &params, vector<Hit> *hits) const {
            unordered_map<unsigned, Candidate> candidates;
            {
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
            hits->clear();
            for (auto &pair: candidates) {
                unsigned id = pair.first;
                Candidate &cand = pair.second;
                cand.object = &records[id]->object;
                float score = matcher.match(object, cand);
                if (score  >= params.R) {
                    Hit hit;
                    hit.key = records[id]->key;
                    hit.score = score;
                    hits->push_back(hit);
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

    class Server {
        string root;
        Journal journal;
        vector<DB *> dbs;
        Extractor xtor;

        void check_dbid (uint16_t dbid) const {
            BOOST_VERIFY(dbid < dbs.size());
        }
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
                    this->insert(dbid, key, object);
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

        void extract_url (string const &url, Object *object) const {
            xtor.extract_url(url, object);
        }

        void extract (string const &content, Object *object) const {
            xtor.extract(content, object);
        }

        void insert (uint16_t dbid, string const &key, Object *object) {
            check_dbid(dbid);
            // has to check first
            journal.append(dbid, key, *object);
            // because insert could swap object's content
            dbs[dbid]->insert(key, object);
        }

        void search (uint16_t dbid, Object const &object, SearchParams const &params, vector<Hit> *hits) const {
            check_dbid(dbid);
            dbs[dbid]->search(object, params, hits);
        }

        void reindex (uint16_t dbid) {
            check_dbid(dbid);
            dbs[dbid]->reindex();
        }

        void clear (uint16_t dbid) {
            check_dbid(dbid);
            dbs[dbid]->clear();
        }

        void sync () {
            journal.sync();
        }

        void status () const {
        }
    };

    void run_service (Config const &config, Server *);
}

#endif

