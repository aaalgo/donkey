#ifndef AAALGO_DONKEY_NISE
#define AAALGO_DONKEY_NISE

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include "donkey-common.h"

namespace donkey {

    static constexpr unsigned SKETCH_BITS = 128;
    typedef uint32_t Chunk;
    static constexpr unsigned CHUNK_BITS = sizeof(Chunk) * 8;
    static constexpr unsigned DIM = SKETCH_BITS / CHUNK_BITS;

    struct Feature: public VectorFeature<Chunk, DIM> {
    };

    struct FeatureSimilarity: public distance::Hamming<Chunk, DIM> {
    };

    struct Region {
        float x, y, r, t;
    };

    // no data, but has weight
    struct Object: public MultiPartObject<Feature, Region> {
    };

    class ExtractorImpl;

    class Extractor: public ExtractorBase {
        ExtractorImpl *impl;
    public:
        Extractor (Config const &config);
        ~Extractor ();
        void extract_path (string const &path, string const &type, Object *object) const;
    };

    typedef CountingMatcher<Object, FeatureSimilarity> Matcher;
}


#endif
