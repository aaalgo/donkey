#ifndef AAALGO_DONKEY_EMD
#define AAALGO_DONKEY_EMD

#include <array>
#include <algorithm>
#include <iostream>
#include <type_traits>
#include "donkey-common.h"
#include "emd_hat.hpp"
// common feature and objects 

namespace donkey {

    template <typename T>
    class EMDMatcher {
        float th;

        static void normalize (Object const &obj, unsigned off, vector<float> *v) {
            float sum = 0;
            for (auto const &p: obj.parts) {
                sum += p.weight;
            }
            for (unsigned i = 0; i < obj.parts.size(); ++i) {
                v->at(off + i) = obj.parts[i].weight / sum;
            }
        }

        float extra_mass_penalty;

    public:
        typedef T feature_similarity_type;
        static constexpr int POLARITY = -1;

        CountingMatcher (Config const &config):
            extra_mass_penalty(config.get<float>("donkey.emd.extra_mass_penalty", 0))
        {
            static_assert(FeatureSimilarity::POLARITY < 0);
            static_assert(std::is_arithmetic<Object::weight_type>::value);
        }

        float apply (Object const &query, Candidate const &cand) const {
            unsigned N1 = query.parts.size();
            unsigned N2 = cand.object->part.size();
            unsigned N = N1 + N2;
            vector<float> P(N, 0);
            vector<float> Q(N, 0);
            normalize(query, 0, &P);
            normalize(*cand->object, N1, &Q);
            vector<vector<float>> C(N, vector<float>(N, 0));
            for (unsigned i = 0; i < N1; ++i) {
                for (unsigned j = 0; j < N2; ++j) {
                    float v = FeatureSimilarity::apply(query.parts[i].feature, cand.object->parts[j].feature);
                    C[i][N1 + j] = C[N1 + j][i] = v;
                }
            }
            return emd_hat<float, NO_FLOW>()(P, Q, C, extra_mass_penalty);
        }
    };
}

#endif
