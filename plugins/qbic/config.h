#ifndef AAALGO_DONKEY_QBIC
#define AAALGO_DONKEY_QBIC

#include "donkey-common.h"

namespace donkey {

    static constexpr unsigned QBIC_DIM = 16;

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
        void extract_path (string const &path, Object *object) const;
    };

    class Matcher: public TrivialMatcher<FeatureSimilarity> {
    public:
        Matcher (Config const &config): TrivialMatcher<FeatureSimilarity>(config) {
        };
    };

}


#endif
