//#include <kgraph.h>
#include "donkey.h"

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
        KGraphIndex (Config const &config): Index(config), indexed_size(0), kg_index(nullptr) {
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
                int K = sp.hint_K;
                float R = sp.hint_R;
                if (K <= 0) K = default_K;
                if (!isnormal(R)) R = default_R;
                if (FeatureSimilarity::POLARITY >= 0) {
                    R *= -1;
                }
                params.K = K;
                params.epsilon = R;
                // update search params
                vector<unsigned> ids(K);
                vector<float> dists(K);
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

    Index *create_kgraph_index (Config const &config) {
        return new KGraphIndex(config);
    }
}
