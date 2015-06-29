#ifndef AAALGO_DONKEY_COMMON
#define AAALGO_DONKEY_COMMON

#include <array>
#include <algorithm>
#include <iostream>
#include <type_traits>
// common feature and objects 

namespace donkey {

    struct PositiveSimilarity {
        static constexpr int POLARITY = 1;
    };

    struct NegativeSimilarity {
        static constexpr int POLARITY = -1;
    };

    template <typename T, unsigned D>
    struct VectorFeature {
        typedef T value_type;
        static unsigned constexpr DIM = D;
        static unsigned constexpr BYTES = DIM * sizeof(value_type);
        std::array<T, D> data;

        void read (std::istream &is) {
            is.read(reinterpret_cast<char *>(&data[0]), sizeof(value_type) * DIM);
        }

        void write (std::ostream &os) const {
            os.write(reinterpret_cast<char const *>(&data[0]), sizeof(value_type) * DIM);
        }
    };

    template <unsigned B, typename C = uint32_t>
    struct BitVectorFeature: public VectorFeature<C, B / sizeof(C) / 8> {
        static unsigned constexpr BITS = B;
    };

    namespace distance {

        typedef NegativeSimilarity Distance;

        template <typename T, unsigned D>
        struct L1: public Distance {
            typedef VectorFeature<T,D> feature_type;
            static float apply (feature_type const &v1, feature_type const &v2) {
                double v = 0;
                for (unsigned i = 0; i < D; ++i) {
                    double a = std::abs(v1.data[i] - v2.data[i]);
                    v += a;
                }
                return v;
            }
        };

        template <typename T, unsigned D>
        struct L2: public Distance {
            typedef VectorFeature<T,D> feature_type;
            static float apply (feature_type const &v1, feature_type const &v2) {
                double v = 0;
                for (unsigned i = 0; i < D; ++i) {
                    double a = v1.data[i] - v2.data[i];
                    v += a * a;
                }
                return std::sqrt(v);
            }
        };

        template <unsigned D>
        int hamming_with_popcount (uint32_t const *v1, uint32_t const *v2) {
            int v = 0;
            for (unsigned i = 0; i < D; ++i) {
                v += __builtin_popcount(v1[i] ^ v2[i]);
            }
            return v;
        }

        template <unsigned D>
        int hamming_with_popcount (uint64_t const *v1, uint64_t const *v2) {
            int v = 0;
            for (unsigned i = 0; i < D; ++i) {
                v += __builtin_popcountll(v1[i] ^ v2[i]);
            }
            return v;
        }

        template <typename T, unsigned D>
        struct Hamming: public Distance {
            typedef VectorFeature<T, D> feature_type;
            static float apply (feature_type const &v1, feature_type const &v2) {
                return hamming_with_popcount<D>(&v1.data[0], &v2.data[0]);
            }
        };

        template <typename T, unsigned D>
        struct TypeHamming: public Distance {
            typedef VectorFeature<T, D> feature_type;
            static float apply (feature_type const &v1, feature_type const &v2) {
                int v = 0;
                for (unsigned i = 0; i < D; ++i) {
                    if (v1.data[i] != v2.data[i]) {
                        ++v;
                    }
                }
                return v;
            }
        };
    }

    template <typename T>
    struct SingleFeatureObject: public ObjectBase {
        typedef T feature_type;
        feature_type feature;
        void enumerate (function<void(unsigned tag, feature_type const *)> callback) const {
            callback(0, &feature);
        }

        void read (std::istream &is) {
            feature.read(is);
        }

        void write (std::ostream &os) const {
            feature.write(os);
        }

        void swap (SingleFeatureObject<T> &v) {
            std::swap(feature, v.feature);
        }
    };

    struct tag_no_data {
    };

    struct tag_no_weight {
    };

    template <typename T, typename D = tag_no_data, typename W = tag_no_weight>
    struct MultiPartObject: public ObjectBase {
        typedef T feature_type;
        typedef D data_type;
        typedef W weight_type;
        struct Part {
            feature_type feature;
            weight_type weight;
            data_type data;
        };
        vector<Part> parts;


        void enumerate (function<void(unsigned tag, feature_type const *)> callback) const {
            for (unsigned i = 0; i < parts.size(); ++i) {
                callback(i, &parts[i].feature);
            }
        }

        void read (std::istream &is) {
            uint16_t sz;
            is.read(reinterpret_cast<char *>(&sz), sizeof(sz));
            BOOST_VERIFY(sz <= MAX_FEATURES);
            parts.resize(sz);
            for (auto &part: parts) {
                if (!std::is_same<weight_type, tag_no_weight>::value) {
                    is.read(reinterpret_cast<char *>(&part.weight), sizeof(weight_type));
                }
                if (!std::is_same<data_type, tag_no_data>::value) {
                    is.read(reinterpret_cast<char *>(&part.data), sizeof(data_type));
                }
                part.feature.read(is);
            }
        }

        void write (std::ostream &os) const {
            uint16_t sz = parts.size();
            os.write(reinterpret_cast<char const *>(&sz), sizeof(sz));
            for (auto const &part: parts) {
                if (!std::is_same<weight_type, tag_no_weight>::value) {
                    os.write(reinterpret_cast<char const *>(&part.weight), sizeof(weight_type));
                }
                if (!std::is_same<data_type, tag_no_data>::value) {
                    os.write(reinterpret_cast<char const *>(&part.data), sizeof(data_type));
                }
                part.feature.write(os);
            }
        }

        void swap (MultiPartObject<T, D, W> &v) {
            std::swap(parts, v.parts);
        }
    };

    template <typename O, typename T>
    class TrivialMatcher {
    public:
        typedef O object_type;
        typedef T feature_similarity_type;
        static constexpr int POLARITY = feature_similarity_type::POLARITY;

        TrivialMatcher (Config const &config) {
        }

        float apply (object_type const &query, Candidate const &cand) const {
            BOOST_VERIFY(cand.hints.size());
            return cand.hints[0].value;
        }
    };

    template <typename O, typename T>
    class CountingMatcher {
    public:
        typedef O object_type;
        typedef T feature_similarity_type;
        static constexpr int POLARITY = 1;

        CountingMatcher (Config const &config)
        {
        }

        float apply (object_type const &query, Candidate const &cand) const {
            BOOST_VERIFY(cand.hints.size());
            unsigned v = 0;
            for (auto const &h: cand.hints) {
                ++v;
            }
            return v;
        }
    };
}

#endif
