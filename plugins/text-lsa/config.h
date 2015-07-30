#ifndef AAALGO_DONKEY_TEXT_LSA
#define AAALGO_DONKEY_TEXT_LSA

#include "../../src/donkey-common.h"

namespace donkey {
    static constexpr unsigned LSA_DIM = 2000; // number of topics in the latent semantic analysis

    struct Feature: public VectorFeature<float, LSA_DIM> {
    };

    struct FeatureSimilarity: public Cosine<float, LSA_DIM> {
    };

     struct Object: public SingleFeatureObject<Feature> {
    };

     class Extractor: public ExtractorBase {
    public:
        Extractor (Config const &config) {
        }
        void extract_path (const string &path, string const &type, Object *object) const;
        void extract (string const &content, string const &type, Object *object) const; 
    };

    class Matcher: public TrivialMatcher<Object, FeatureSimilarity> {
    public:
        Matcher (Config const &config): TrivialMatcher<Object, FeatureSimilarity>(config) {
        };
    };

}

#endif
