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
        struct Params {
            void decode (string const &) {}
            string encode () const {return "";}
        };
    };

    struct NegativeSimilarity {
        static constexpr int POLARITY = -1;
        struct Params {
            void decode (string const &) {}
            string encode () const {return "";}
        };
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

    template <typename T, unsigned D>
    struct Cosine: public PositiveSimilarity {
        typedef VectorFeature<T,D> feature_type;
        static float apply (feature_type const &v1, feature_type const &v2, Params const &params) {
            float v = 0.0f;
            float m1 = 0.0f, m2 = 0.0f;
            for (unsigned i = 0; i < D; ++i) {
                v  += v1.data[i] * v2.data[i];
                m1 += v1.data[i] * v1.data[i];
                m2 += v2.data[i] * v2.data[i];
             }
             v /= (std::sqrt(m1) * std::sqrt(m2));
             if(std::isnormal(v)) return v;
             else return -1.0f;
        }
    };

    namespace distance {

        typedef NegativeSimilarity Distance;

        template <typename T, unsigned D>
        struct L1: public Distance {
            typedef VectorFeature<T,D> feature_type;
            static float apply (feature_type const &v1, feature_type const &v2, Params const &params) {
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
            static float apply (feature_type const &v1, feature_type const &v2, Params const &params) {
                double v = 0.0;
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
            static float apply (feature_type const &v1, feature_type const &v2, Params const &params) {
                return hamming_with_popcount<D>(&v1.data[0], &v2.data[0]);
            }
        };

        template <typename T, unsigned D>
        struct TypeHamming: public Distance {
            typedef VectorFeature<T, D> feature_type;
            static float apply (feature_type const &v1, feature_type const &v2, Params const &params) {
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

    // !IMPORTANT: 
    template <typename T, typename FEATURE_DATA = tag_no_data, typename W = tag_no_weight, typename OBJECT_DATA = tag_no_data>
    struct MultiPartObject: public ObjectBase {
        typedef T feature_type;
        typedef FEATURE_DATA feature_data_type;
        typedef OBJECT_DATA object_data_type;
        typedef W weight_type;
        struct Part {
            feature_type feature;
            weight_type weight;
            feature_data_type data;
        };
        vector<Part> parts;
        object_data_type data;

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
                if (!std::is_same<feature_data_type, tag_no_data>::value) {
                    is.read(reinterpret_cast<char *>(&part.data), sizeof(feature_data_type));
                }
                part.feature.read(is);
            }
            if (!std::is_same<object_data_type, tag_no_data>::value) {
                is.read(reinterpret_cast<char *>(&data), sizeof(object_data_type));
            }
        }

        void write (std::ostream &os) const {
            uint16_t sz = parts.size();
            os.write(reinterpret_cast<char const *>(&sz), sizeof(sz));
            for (auto const &part: parts) {
                if (!std::is_same<weight_type, tag_no_weight>::value) {
                    os.write(reinterpret_cast<char const *>(&part.weight), sizeof(weight_type));
                }
                if (!std::is_same<feature_data_type, tag_no_data>::value) {
                    os.write(reinterpret_cast<char const *>(&part.data), sizeof(feature_data_type));
                }
                part.feature.write(os);
            }
            if (!std::is_same<object_data_type, tag_no_data>::value) {
                os.write(reinterpret_cast<char const *>(&data), sizeof(object_data_type));
            }
        }

        void swap (MultiPartObject<T, FEATURE_DATA, W, OBJECT_DATA> &v) {
            std::swap(parts, v.parts);
            std::swap(data, v.data);
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

        float apply (object_type const &query, Candidate const &cand, string *details) const {
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

        float apply (object_type const &query, Candidate const &cand, string *details) const {
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
