#ifndef AAALGO_DONKEY_QBIC
#define AAALGO_DONKEY_QBIC

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include "donkey-common.h"
#include "donkey-emd.h"

namespace donkey {

    static constexpr unsigned DIM = 14;

    struct Feature: public VectorFeature<float, DIM> {
    };

    struct FeatureSimilarity: public distance::L2<float, DIM> {
    };

    // no data, but has weight
    struct Object: public MultiPartObject<Feature, tag_no_data, float> {
    };


    class Extractor: public ExtractorBase {
    public:
        Extractor (Config const &config)
        {
        }
        void extract_path (string const &path, string const &type, Object *object) const;
    };

    typedef EMDMatcher<Object, FeatureSimilarity> Matcher;
}


#endif
