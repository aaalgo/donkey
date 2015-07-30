#ifndef AAALGO_DONKEY_TEXT_RAND
#define AAALGO_DONKEY_TEXT_RAND

#include "../../src/donkey-common.h"

namespace donkey {
    static constexpr unsigned RAND_VECTOR_NUM = 1024;
    static constexpr unsigned RAND_INT_BITS = 32;
    static constexpr unsigned RAND_DIM = RAND_VECTOR_NUM/RAND_INT_BITS;

    struct Feature: public VectorFeature<uint32_t, RAND_DIM> {
    };

    struct FeatureSimilarity: public distance::Hamming<uint32_t, RAND_DIM> {
    };

     struct Object: public SingleFeatureObject<Feature> {
    };

     class Extractor: public ExtractorBase {
    public:
        Extractor (Config const &config) {
        }
        void extract_path (const string &path, string const &type, Object *object) const;
    };

    class Matcher: public TrivialMatcher<Object, FeatureSimilarity> {
    public:
        Matcher (Config const &config): TrivialMatcher<Object, FeatureSimilarity>(config) {
        };
    };

}

#endif