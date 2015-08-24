namespace donkey {
    // Index is not mutex-protected.
    class InvertedIndex: public Index {
        unordered_map<Feature, vector<uint32_t, uint32_t>> data;
    public:
        InvertedIndex (Config const &config_): Index(config_) {
        }

        ~InvertedIndex () {
        }

        virtual void search (Feature const &query, SearchRequest const &sp, std::vector<Match> *matches) const {
            auto const &bin = data[query];
            for (auto p: bin) {
                Match m;
                m.object = p.first;
                m.tag = p.second;
                m.distance = bin.size();
                matches->push_back(m);
            }
        }

        virtual void insert (uint32_t object, uint32_t tag, Feature const *feature) {
            data[*feature].push_back(std::make_pair(object, tag));
        }

        virtual void clear () {
            data.clear();
        }

        virtual void rebuild () {   // insert must not happen at this time
            LOG(info) << "rebuild: no need to rebuild.";
        }
    };

    Index *create_inverted_index (Config const &config) {
        return new InvertedIndex(config);
    }
}
