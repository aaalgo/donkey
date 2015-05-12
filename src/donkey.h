#ifndef AAALGO_DONKEY
#define AAALGO_DONKEY

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <array>
#include <vector>
#include <mutex>
#include <functional>
#include <boost/thread.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/timer/timer.hpp>
#include <boost/assert.hpp>
#include <kgraph.h>

// common stuff
namespace donkey {

    using std::function;
    using std::string;
    using std::runtime_error;
    using std::fstream;
    using std::array;
    using std::vector;
    using boost::shared_mutex;
    using boost::unique_lock;
    using boost::shared_lock;

    // maximal number of DBs
    static constexpr size_t DEFAULT_MAX_DBS = 256;

    // Configuration
    typedef boost::property_tree::ptree Config;

    void LoadConfig (string const &path, Config *);
    void OverrideConfig (std::vector<std::string> const &overrides, Config *);

    // Logging
#define LOG(x) BOOST_LOG_TRIVIAL(x)
    namespace logging = boost::log;
    void setup_logging (string const &path = string());

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
}

namespace donkey {

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

    struct Feature;
    struct Object;

    struct SearchParams {
        unsigned K;
        float R;
        unsigned hint_K;
        float hint_R;
    };

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
        virtual void insert (uint32_t object, uint32_t tag, Feature *feature) = 0;
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
            Feature *feature;
        };
        size_t indexed_size;
        vector<Entry> entries;

        friend class KGraphIndex::IndexOracle;
        friend class KGraphIndex::SearchOracle;

        class IndexOracle: public kgraph::IndexOracle {
            KGraphIndex *parent;
        public:
            IndexOracle (KGraphIndex *p): parent(p) {
            }

            virtual unsigned size () const {
                return parent->entries.size();
            }   
            virtual float operator () (unsigned i, unsigned j) const {
                return distance(*parent->entries[i].feature
                                *parent->entries[j].feature);
            }   
        };  

        class SearchOracle: public kgraph::SearchOracle {
            KGraphIndex *parent;
            Feature const &query;
        public:
            SearchOracle (KGraphIndex *p, Feature const &q): parent(p), query(q) {
            }   
            virtual unsigned size () const {
                return parent->indexed_size;
            }   
            virtual float operator () (unsigned i) const {
                return distance(*parent->entries[i].feature, query);
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

        ~KGraphIndex {
            if (kg_index) {
                delete kg_index;
            }
        }

        virtual void search (Feature const &query, SearchParams const &sp, std::vector<Match> *matches) const {
            BOOST_VERIFY(indexed_size == entries.size());
            if (entries.empty()) {
                matches->clear();
                return;
            }
            BOOST_VERIFY(kg_graph == 0);
            SearchOracle oracle(this, query.feature);
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
                m.distance = dist[i];
            }
        }

        virtual void insert (uint32_t object, uint32_t tag, Feature *feature) const {
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
        std::string path;           // path to journal file
        ofstream str;               // only opened after recover is invoked
        std::mutex mutex;

        struct __attribute__ ((__packed__)) RecordHead {
            uint32_t magic;
            uint16_t dbid;
            uint16_t key_size;
        };
    public:
        Journal (string const &path_)
              : path(path_)
        }

        ~Journal () {
            if (str.is_open()) {
                sync();
            }
        }

        // fastforward: directly jump to the given offset
        // return maxid
        int recover (std::function<void(uint16_t dbid, string const &key, Object *object)> callback) {
            size_t off = 0;
            int count = 0;
            do {
                std::ifstream is(path.c_str(), std::ios::binary);
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
                fd = ::open(path.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
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
            str.open(path.c_str(), ios::binary | ios.app);
            BOOST_VERIFY(str.is_open());
        }

        void append (uint16_t dbid, string const &key, Object const &object) {
            BOOST_VERIFY(str.is_open());
            std::lock_guard<std::mutex> lock(mutex); 
            RecordHead head;
            head.magic = MAGIC;
            head.dbid = dbid;
            head.key_size = key.size();
            size_t r = str.write(reinterpret_cast<char const *>(&head), sizeof(head));
            BOOST_VERIFY(r == sizeof(head));
            r = str.write(&key[0], key.size());
            BOOST_VERIFY(r == key.size());
            object.write(str);
        }

        void sync () {
            BOOST_VERIFY(str.is_open());
            std::lock_guard<std::mutex> lock(mutex); 
            str.flush();
            int handle = static_cast<std::filebuf *>(str.rdbuf())->_M_file.fd();
            ::fsync(fd);
        }
    };

    class DB {
        struct Record {
            string key;
            Object object;
        };
        vector<Record *> records;
        Index *index;
        shared_mutex mutex;
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
            unique_lock<shared_mutex> lock(mutex)
            size_t id = records.size();
            records.push_back(rec);
            rec->object.enumerate([this, id](unsigned tag, Feature const *ft) {
                index->insert(id, tag, ft);
            });
        }

        void search (Object const &object, SearchParams const &params, vector<Hit> *hits) const {
            unordered_map<unsigned, Candidate> hits;
            {
                shared_lock<shared_mutex> lock(mutex);
                object.enumerate([this, hits](unsigned qtag, Feature const *ft) {
                    vector<Match> matches;
                    index->search(*ft, params, &matches);
                    for (auto const &m: matches) {
                        auto &h = hits[m.object];
                        Hint hint;
                        hint.dtag = m.tag;
                        hint.qtag = qtag;
                        hint.value = m.distance;
                        h.hints.push_back(hint);
                    }
                });
            }
            hits->clear();
            for (auto &pair: hits) {
                unsigned id = pair.first;
                Candidate &cand = pair.second;
                cand.object = &records[id]->object;
                float score = matcher.match(object, hit);
                if (score  >= params.R) {
                    Hit hit;
                    hit.key = records[id]->key;
                    hit.score = score;
                    hits->push_back(hit);
                }
            }
        }

        void clear () {
            unique_lock<shared_mutex> lock(mutex)
            index->clear();
            for (auto record: records) {
                delete record;
            }
            records.clear();
        }

        void reindex () {
            unique_lock<shared_mutex> lock(mutex)
            index.rebuild();
        }
    };

    class Server {
        string root;
        Journal journal;
        vector<DB *> dbs;
        Extractor xtor;

        void check_dbid (uint16_t dbid) {
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

        void extract_url (string const &url, Object *object) {
            xtor.extract_url(url, object);
        }

        void extract (string const &content, Object *object) {
            xtor.extract(content, object);
        }

        void insert (uint16_t dbid, string const &key, Object *object) {
            check_dbid(dbid);
            // has to check first
            journal.append(dbid, key, *object);
            // because insert could swap object's content
            dbs[dbid]->insert(object);
        }

        void search (uint16_t dbid, Object const *object, SearchParams const &params, vector<Hit> *hits) const {
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

