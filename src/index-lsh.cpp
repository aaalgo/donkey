#include <utility>
#include "donkey.h"
#include "lsh.h"

namespace donkey {

    // Index is not mutex-protected.
    class LSHIndex: public Index {

        struct Key {
            uint32_t object;
            uint32_t tag;
        };

        struct Entry {
            Key key;
            Feature const *feature;
        };

        // lsh.h requires a config structure.
        struct LSHConfig {
            typedef Feature QUERY_TYPE;
            typedef Entry RECORD_TYPE;
            typedef Key KEY_TYPE;
            typedef FeatureSimilarity::Params SEARCH_PARAMS_TYPE;
            static int constexpr POLARITY = FeatureSimilarity::POLARITY;

            // TODO, implement hash
            static void hash (QUERY_TYPE const &q, unsigned tables, unsigned bits, uint32_t *hash) {
                std::fill(hash, hash + tables, 0);
            }

            // TODO, implement hash
            static void hash (RECORD_TYPE const &rec, unsigned tables, unsigned bits, uint32_t *hash) {
                std::fill(hash, hash + tables, 0);
            }

            static KEY_TYPE key (RECORD_TYPE const &r) {
                return r.key;
            }

            static float dist (RECORD_TYPE const &r, QUERY_TYPE const &q, SEARCH_PARAMS_TYPE const &params) {
                return FeatureSimilarity::apply(*r.feature, q, params);
            }
        };

        typedef lsh::Index<LSHConfig> LSHImpl;

        Config config;
        size_t indexed_size;
        LSHImpl *lsh_index;
        FeatureSimilarity::Params search_params_l1;

        void create_index () {
            unsigned num_tables = config.get("donkey.lsh.tables", 8);
            unsigned hash_bits = config.get("donkey.lsh.bits", 24);
            size_t allocate = config.get("donkey.lsh.allocate", 16LL * 1024 * 1024 * 1024);
            lsh_index = new LSHImpl(num_tables, hash_bits, allocate);
            if (!lsh_index) {
                throw InternalError("Cannot create LSH index.");
            }
        }

    public:
        LSHIndex (Config const &config_): Index(config_), config(config_), indexed_size(0), lsh_index(nullptr) {
            string l1 = config.get<string>("donkey.lsh.search.params_l1", "");
            search_params_l1.decode(l1);
            create_index();
        }

        ~LSHIndex () {
            if (lsh_index) {
                delete lsh_index;
            }
        }

        virtual void search (Feature const &query, SearchRequest const &sp, std::vector<Match> *matches) const {
            matches->clear();
            if (lsh_index) {
                int K = sp.hint_K;
                if (K <= 0) K = default_K;
                float R = sp.hint_R;
                if (!isnormal(R)) R = default_R;
                vector<std::pair<Key, float>> m;
                lsh_index->search(query, R, &m, sp.params_l1);
                //TODO: use sp.hint_K, too
                matches->resize(m.size());
                for (unsigned i = 0; i < m.size(); ++i) {
                    auto &to = matches->at(i);
                    auto const &from = m[i];
                    to.object = from.first.object;
                    to.tag = from.first.tag;
                    to.distance = from.second;
                }
                if (matches->size() > K) {
                    if (FeatureSimilarity::POLARITY >= 0) {
                        sort(matches->begin(),
                             matches->end(),
                             [](Match const &h1, Match const &h2) { return h1.distance > h2.distance;});
                    }
                    else {
                        sort(matches->begin(),
                             matches->end(),
                             [](Match const &h1, Match const &h2) { return h1.distance < h2.distance;});
                    }
                    matches->resize(K);
                }
            }
        }

        virtual void insert (uint32_t object, uint32_t tag, Feature const *feature) {
            Entry e;
            e.key.object = object;
            e.key.tag = tag;
            e.feature = feature;
            lsh_index->append(e);
            ++indexed_size;
        }

        virtual void clear () {
            if (lsh_index) {
                delete lsh_index;
                lsh_index = nullptr;
            }
            indexed_size = 0;
        }

        virtual void rebuild () {   // insert must not happen at this time
            LOG(info) << "rebuild: we can do without rebuild for now.";
        }
    };

    Index *create_lsh_index (Config const &config) {
        return new LSHIndex(config);
    }
}
