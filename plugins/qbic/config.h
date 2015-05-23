#ifndef AAALGO_DONKEY_QBIC
#define AAALGO_DONKEY_QBIC

#include "../../src/donkey-common.h"

namespace donkey {

    static constexpr unsigned QBIC_CHANNEL_DIM = 16;
    static constexpr unsigned QBIC_DIM = 3*QBIC_CHANNEL_DIM;

    struct Feature: public VectorFeature<float, QBIC_DIM> {
    };

    struct FeatureSimilarity: public distance::L2<float, QBIC_DIM> {
    };

    struct Object: public SingleFeatureObject<Feature> {
    };

    class Extractor: public ExtractorBase {
    public:
        Extractor (Config const &config) {
        }
        void extract_path (const string &path, Object *object) const;
    };

    class Matcher: public TrivialMatcher<FeatureSimilarity> {
    public:
        Matcher (Config const &config): TrivialMatcher<FeatureSimilarity>(config) {
        };
    };

}


#endif
