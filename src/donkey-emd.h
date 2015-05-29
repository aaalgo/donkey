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

    template <typename O, typename T>
    class EMDMatcher {
    public:
        typedef O object_type;
        typedef T feature_similarity_type;
        static constexpr int POLARITY = -1;

        EMDMatcher (Config const &config):
            extra_mass_penalty(config.get<double>("donkey.emd.extra_mass_penalty", 0))
        {
            static_assert(feature_similarity_type::POLARITY < 0, "EMD only work with distance-like feature similarity.");
            static_assert(std::is_arithmetic<typename object_type::weight_type>::value, "EMD only work with weighted multi-part objects.");
        }

        float apply (object_type const &query, Candidate const &cc) const {
            object_type const &cand = *static_cast<object_type const *>(cc.object);
            unsigned N1 = query.parts.size();
            unsigned N2 = cand.parts.size();
            unsigned N = N1 + N2;
            vector<double> P(N, 0);
            vector<double> Q(N, 0);
            normalize(query, 0, &P);
            normalize(cand, N1, &Q);
            vector<vector<double>> C(N, vector<double>(N, 0));
            for (unsigned i = 0; i < N1; ++i) {
                for (unsigned j = 0; j < N2; ++j) {
                    double v = feature_similarity_type::apply(query.parts[i].feature, cand.parts[j].feature);
                    C[i][N1 + j] = C[N1 + j][i] = v;
                }
            }
            return emd_hat<double, NO_FLOW>()(P, Q, C, extra_mass_penalty);
        }
    private:

        static void normalize (object_type const &obj, unsigned off, vector<double> *v) {
            double sum = 0;
            for (auto const &p: obj.parts) {
                sum += p.weight;
            }
            for (unsigned i = 0; i < obj.parts.size(); ++i) {
                v->at(off + i) = obj.parts[i].weight / sum;
            }
        }

        double extra_mass_penalty;

    };
}

#endif
