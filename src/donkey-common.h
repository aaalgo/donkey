#ifndef AAALGO_DONKEY_COMMON
#define AAALGO_DONKEY_COMMON

#include <array>
// common feature and objects 

namespace donkey {

    template <typename T, unsigned D>
    struct VectorFeature {
        static unsigned constexpr DIM = D;
        std::array<T, D> data;
    };


#if 0
    typedef vector<float> FloatVectorFeature;

    template <typename T>
    struct MultiFeatureObject {
        typedef T feature_type;
        vector<T> features;
    };

    template <typename T>
    struct SingleFeatureObject {
        typedef T feature_type;
        T feature;
    };

    template <typename T>
    class SingleFeatureMatcher {
    public:
        SingleFeatureObjectMatcher (Config const &config) {
        }

        void match (T const &query, Hit *hit) {
            if (hit->hints.empty()) {
                hit->good = false;
            }
            else {
                hit->good = true;
                hit->score = hit->hits[0].score;
            }
        }
    };

    template <typename T>
    class SimpleMultiFeatureMatcher {
    public:
        SimpleMultiFeatureMatcher (Config const &config) {
        }

        void match (T const &query, Hit *hit) {
        }
    };

#endif
}

#endif
