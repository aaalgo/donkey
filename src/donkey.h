#ifndef AAALGO_DONKEY
#define AAALGO_DONKEY

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <functional>
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
    using std::vector;

    static constexpr size_t DEFAULT_MAX_DBS = 256;

    // Configuration
    typedef boost::property_tree::ptree Config;

    void LoadConfig (string const &path, Config *);
    void OverrideConfig (std::vector<std::string> const &overrides, Config *);

    // Logging
#define LOG(x) BOOST_LOG_TRIVIAL(x)
    namespace logging = boost::log;
    void setup_logging (string const &path = string());

    void DumpStack ();

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

// data-type-specific configuration
#include "plugin/config.h"

namespace donkey {

    // user will have to ensure serial access
    class Journal {
        static uint32_t const MAGIC = 0xdeadbeef;
        std::string path;           // path to journal file
        ofstream str;
        bool recovered;

        static void sync_fstream (fstream &str) {
        }
    public:
        Journal (string const &path_)
              : path(path_),
              recovered(false) {
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
                    LOG(warning) << "Fail to open journal file." << endl;
                    LOG(warning) << "Overwriting..." << endl;
                    break;
                }
                for (;;) {
                    uint32_t magic;
                    uint16_t dbid;
                    uint16_t key_size;
                    is.read(reinterpret_cast<char *>(&magic), sizeof(magic));
                    if (!is) break;
                    if (magic != MAGIC) {
                        cerr << "Corrupted journal, truncating at " << off << endl;
                        break;
                    }
                    is.read(reinterpret_cast<char *>(&dbid), sizeof(dbid));
                    is.read(reinterpret_cast<char *>(&key_size), sizeof(key_size));
                    if (!is) break;
                    string key;
                    key.resize(key_size);
                    is.read((char *)&key[0], key.size());
                    if (!is) break;
                    Object object;
                    object.read(is);
                    callback(dbid, key, &object);
                    ++count;
                    off = is.tellg();
                }
                is.close();
                cerr << "Journal recovered." << endl;
                cerr << count << " items loaded." << endl;
                cerr << "Offset is " << off << "." << endl;
            
            }
            while (false);

            {
                fd = ::open(path.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
                BOOST_VERIFY(fd >= 0);
                int r = ::ftruncate(fd, off);
                BOOST_VERIFY(r == 0);
                close(fd);
            }

            str.open(path.c_str(), ios::binary | ios.app);
        }

        void append (uint16_t dbid, string const &key, Object const &object) {
            BOOST_VERIFY(recovered);
            uint32_t magic = MAGIC;
            size_t r = str.write(reinterpret_cast<char const *>(&magic), sizeof(magic));
            BOOST_VERIFY(r == sizeof(magic));
            r = str.write(reinterpret_cast<char const *>(&dbid), sizeof(dbid));
            BOOST_VERIFY(r == sizeof(dbid));
            uint16_t key_size = key.size();
            r = str.write(reinterpret_cast<char const *>(&key_size), sizeof(key_size));
            BOOST_VERIFY(r == sizeof(key_size));
            r = str.write(&key[0], key_size);
            BOOST_VERIFY(r == key_size);
            object.write(str);
        }

        void sync () {
            str.flush();
            int handle = static_cast<std::filebuf *>(str.rdbuf())->_M_file.fd();
            ::fsync(fd);
        }
    };

    class Index {
        struct Entry {
            uint32_t oid;
            uint32_t fid;
            Feature const *feature;
        };
        vector<Entry> entries;

    public:
        Index (Config const &config) {
        }

        virtual void search (Feature const &, std::vector<IndexKey> *) {
        }

        // search many features at the same time
        virtual void search (std::vector<Feature> const &fv, std::vector<std::vector<IndexKey>> *keys) {
        }

        virtual void append (uint32_t id, Object const *object) {
            BOOST_VERIFY(0);
        }

        void clear () {
        }

        virtual void rebuild () {
        }
    };

    class DB {
        vector<Object> objects;
        Index index;
    public:
        DB (Config const &config)
            : index(config)
        {
        }

        void insert (string const &key, Object *object) {
            size_t id = objects.size();
            objects.emplace_back();
            object->swap(objects.back());
            index->append(id, objects.back());
        }

        void search (Object const *object) const {
        }

        void clear () {
            index.clear();
            objects.clear();
        }

        void reindex () {
            index.rebuild();
        }
    };

    class Server {
        string root;
        Journal journal;
        vector<DB *> dbs;

        void check_dbid (uint16_t dbid) {
            BOOST_VERIFY(dbid < dbs.size());
        }
    public:
        Server (Config const &config)
            : root(config.get<string>("donkey.root")),
            journal(root + "/journal"),
            dbs(config.get<size_t>("donkey.max_dbs", DEFAULT_MAX_DBS), nullptr)
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

        void insert (uint16_t dbid, string const &key, Object *object) {
            check_dbid(dbid);
            // has to check first
            journal.append(dbid, key, *object);
            // because insert could swap object's content
            dbs[dbid]->insert(object);
        }

        void search (uint16_t dbid, Object const *object) const {
            check_dbid(dbid);
            dbs[dbid]->search(object);
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
}

#endif

