#include <kgraph.h>
#include "donkey.h"

namespace donkey {

    using kgraph::KGraph;

    bool operator < (Index::Match const &m1, Index::Match const &m2) {
        if (Matcher::POLARITY > 0) {
            return m1.distance > m2.distance;
        }
        else {
            return m1.distance < m2.distance;
        }
    }

    // Index is not mutex-protected.
    class KGraphIndex: public Index {
        struct Entry {
            uint32_t object;
            uint32_t tag;
            Feature const *feature;
        };
        bool linear;
        size_t min_index_size;
        size_t indexed_size;
        vector<Entry> entries;

        friend class IndexOracle;
        friend class SearchOracle;

        class IndexOracle: public kgraph::IndexOracle {
            KGraphIndex *parent;
            FeatureSimilarity::Params params_l1;
        public:
            IndexOracle (KGraphIndex *p, FeatureSimilarity::Params const p1): parent(p), params_l1(p1) {
            }
            virtual unsigned size () const {
                return parent->entries.size();
            }   
            virtual float operator () (unsigned i, unsigned j) const {
                return (-FeatureSimilarity::POLARITY) *
                       FeatureSimilarity::apply(*parent->entries[i].feature,
                                *parent->entries[j].feature, params_l1);
            }   
        };  

        class SearchOracle: public kgraph::SearchOracle {
            KGraphIndex const *parent;
            Feature const &query;
            unsigned offset, sz;
            FeatureSimilarity::Params params_l1;
        public:
            SearchOracle (KGraphIndex const *p, Feature const &q, unsigned begin, unsigned end, FeatureSimilarity::Params params): parent(p), query(q), offset(begin), sz(end-begin), params_l1(params) {
            }   
            virtual unsigned size () const {
                return sz;
            }   
            virtual float operator () (unsigned i) const {
                return FeatureSimilarity::apply(*parent->entries[offset+i].feature, query, params_l1);
            }   
        };

        KGraph::IndexParams index_params;
        KGraph::SearchParams search_params;
        FeatureSimilarity::Params index_params_l1;
        FeatureSimilarity::Params search_params_l1;
        KGraph *kg_index;

    public:
        KGraphIndex (Config const &config, bool linear_ = false): 
            Index(config),
            linear(linear_),
            min_index_size(config.get<size_t>("donkey.kgraph.min", 10000)),
            indexed_size(0),
            kg_index(nullptr) {
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

            string l1 = config.get<string>("donkey.kgraph.index.params_l1", "");
            index_params_l1.decode(l1);

            search_params.K = config.get<unsigned>("donkey.kgraph.search.K", search_params.K);
            search_params.M = config.get<unsigned>("donkey.kgraph.search.M", search_params.M);
            search_params.P = config.get<unsigned>("donkey.kgraph.search.P", search_params.P);
            search_params.T = config.get<unsigned>("donkey.kgraph.search.T", search_params.T);
            search_params.epsilon = config.get<float>("donkey.kgraph.search.epsilon", search_params.epsilon);
            search_params.seed = config.get<unsigned>("donkey.kgraph.search.seed", search_params.seed);

            l1 = config.get<string>("donkey.kgraph.search.params_l1", "");
            search_params_l1.decode(l1);
        }

        ~KGraphIndex () {
            if (kg_index) {
                delete kg_index;
            }
        }

        virtual void search (Feature const &query, SearchRequest const &sp, std::vector<Match> *matches) const {
            matches->clear();

            KGraph::SearchParams params(search_params);
            int K = sp.hint_K;
            float R = sp.hint_R;
            if (K <= 0) K = default_K;
            if (!isnormal(R)) R = default_R;
            if (FeatureSimilarity::POLARITY >= 0) {
                R *= -1;
            }
            vector<unsigned> ids(K*2);
            vector<float> dists(K*2);
            unsigned L = 0;
            params.K = K;
            params.epsilon = R;
            if (kg_index) {
                SearchOracle oracle(this, query, 0, indexed_size, sp.params_l1);
                // update search params
                L += kg_index->search(oracle, params, &ids[L], &dists[L], nullptr);
            }
            else {
                BOOST_VERIFY(indexed_size == 0);
            }
            if (indexed_size < entries.size()) {
                SearchOracle oracle(this, query, indexed_size, entries.size(), sp.params_l1);
                unsigned L0 = L;
                L += oracle.search(params.K, params.epsilon, &ids[L0], &dists[L0]);
                for (unsigned l = L0; l < L; ++l) {
                    ids[l] += indexed_size;
                }
            }
            BOOST_VERIFY(L <= 2*K);
            matches->resize(L);
            for (unsigned i = 0; i < L; ++i) {
                auto &m = matches->at(i);
                auto const &e = entries[ids[i]];
                m.object = e.object;
                m.tag = e.tag;
                m.distance = dists[i];
            }
            sort(matches->begin(),
                 matches->end());
            if (matches->size() > K) {
                matches->resize(K);
            }
            BOOST_VERIFY(indexed_size <= entries.size());
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
            if (linear) {
                BOOST_VERIFY(indexed_size == 0);
                return;
            }
            if (entries.size() == indexed_size) return;
            KGraph *kg = nullptr;
            if (entries.size() >= min_index_size) {
                kg = KGraph::create();
                LOG(info) << "Rebuilding index for " << entries.size() << " features.";
                IndexOracle oracle(this, index_params_l1);
                kg->build(oracle, index_params, NULL);
                LOG(info) << "Swapping on new index...";
            }
            indexed_size = entries.size();
            std::swap(kg, kg_index);
            if (kg) {
                delete kg;
            }
        }

        virtual void recover (string const &path) {
            KGraph *kg = nullptr;
            kg = KGraph::create();
            size_t sz = 0;
            try {
                kg->load(path.c_str());
                string meta_path = path + ".meta";
                std::ifstream is(meta_path.c_str());
                if (!is) throw 0;
                is >> sz;
            }
            catch (...) {
                delete kg;
                kg = nullptr;
            }
            if (kg) {
                indexed_size = sz;
                std::swap(kg, kg_index);
                if (kg) {
                    delete kg;
                }
            }
            else {
                // fail to load, rebuild
                rebuild();
            }
        }

        virtual void snapshot (string const &path) const {
            if (kg_index) {
                kg_index->save(path.c_str(), KGraph::FORMAT_NO_DIST);
                string meta_path = path + ".meta";
                std::ofstream os(meta_path.c_str());
                os << indexed_size << std::endl;
            }
        }
    };

    Index *create_kgraph_index (Config const &config) {
        return new KGraphIndex(config, false);
    }

    Index *create_linear_index (Config const &config) {
        return new KGraphIndex(config, true);
    }
}
