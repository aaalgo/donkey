#ifdef _OPENMP
#include <omp.h>
#endif
#include <unordered_set>
#include <mutex>
#include <iostream>
#include <fstream>
#include <random>
#include <algorithm>
#include <stdexcept>
#include <boost/timer/timer.hpp>
#define timer timer_for_boost_progress_t
#include <boost/progress.hpp>
#undef timer
#include <boost/dynamic_bitset.hpp>
#include <boost/container/pmr/vector.hpp>
#include "fixed_monotonic_buffer_resource.hpp"
#include "kgraph.h"

namespace kgraph {

    using namespace std;
    using namespace boost;

    // generate size distinct random numbers < N
    template <typename RNG>
    static void GenRandom (RNG &rng, unsigned *addr, unsigned size, unsigned N) {
        if (N == size) {
            for (unsigned i = 0; i < size; ++i) {
                addr[i] = i;
            }
            return;
        }
        for (unsigned i = 0; i < size; ++i) {
            addr[i] = rng() % (N - size);
        }
        sort(addr, addr + size);
        for (unsigned i = 1; i < size; ++i) {
            if (addr[i] <= addr[i-1]) {
                addr[i] = addr[i-1] + 1;
            }
        }
        unsigned off = rng() % N;
        for (unsigned i = 0; i < size; ++i) {
            addr[i] = (addr[i] + off) % N;
        }
    }

    struct Neighbor {
        uint32_t id;
        float dist;
        bool flag;  // whether this entry is a newly found one
        Neighbor () {}
        Neighbor (unsigned i, float d, bool f = true): id(i), dist(d), flag(f) {
        }
    };

    // extended neighbor structure for search time
    struct NeighborX: public Neighbor {
        uint16_t m;
        uint16_t M; // actual M used
        NeighborX () {}
        NeighborX (unsigned i, float d): Neighbor(i, d, true), m(0), M(0) {
        }
    };

    static inline bool operator < (Neighbor const &n1, Neighbor const &n2) {
        return n1.dist < n2.dist;
    }

    static inline bool operator == (Neighbor const &n1, Neighbor const &n2) {
        return n1.id == n2.id;
    }

    // try insert nn into the list
    // the array addr must contain at least K+1 entries:
    //      addr[0..K-1] is a sorted list
    //      addr[K] is as output parameter
    // * if nn is already in addr[0..K-1], return K+1
    // * Otherwise, do the equivalent of the following
    //      put nn into addr[K]
    //      make addr[0..K] sorted
    //      return the offset of nn's index in addr (could be K)
    //
    // Special case:  K == 0
    //      addr[0] <- nn
    //      return 0
    template <typename NeighborT>
    unsigned UpdateKnnListHelper (NeighborT *addr, unsigned K, NeighborT nn) {
        // find the location to insert
        unsigned j;
        unsigned i = K;
        while (i > 0) {
            j = i - 1;
            if (addr[j].dist <= nn.dist) break;
            i = j;
        }
        // check for equal ID
        unsigned l = i;
        while (l > 0) {
            j = l - 1;
            if (addr[j].dist < nn.dist) break;
            if (addr[j].id == nn.id) return K + 1;
            l = j;
        }
        // i <= K-1
        j = K;
        while (j > i) {
            addr[j] = addr[j-1];
            --j;
        }
        addr[i] = nn;
        return i;
    }

    static inline unsigned UpdateKnnList (NeighborX *addr, unsigned K, NeighborX nn) {
        return UpdateKnnListHelper<NeighborX>(addr, K, nn);
    }

    class KGraphLite: public KGraph {
    protected:
        boost::container::pmr::fixed_monotonic_buffer_resource memory_resource;
        vector<unsigned> M;
        boost::container::pmr::vector_of<boost::container::pmr::vector<uint32_t>>::type graph;
        static const bool no_dist = true;   // Distance & flag information in Neighbor is not valid.


        // actual M for a node that should be used in search time
        unsigned actual_M (unsigned pM, unsigned i) const {
            return std::min(std::max(M[i], pM), unsigned(graph[i].size()));
        }

        void clear () {
            graph.clear();
            memory_resource.release();
        }

    public:
        KGraphLite (): graph(&memory_resource) {
        }
        virtual ~KGraphLite () {
            clear();
        }
        virtual void load (char const *path) {
            static char const *KGRAPH_MAGIC = "KNNGRAPH";
            static unsigned constexpr KGRAPH_MAGIC_SIZE = 8;
            static uint32_t constexpr SIGNATURE_VERSION = 2;
            static_assert(sizeof(unsigned) == sizeof(uint32_t), "unsigned must be 32-bit");
            ifstream is(path, ios::binary);
            if (!is) throw runtime_error("failed to open index");
            char magic[KGRAPH_MAGIC_SIZE];
            uint32_t sig_version;
            uint32_t sig_cap;
            uint32_t N;
            is.read(magic, sizeof(magic));
            is.read(reinterpret_cast<char *>(&sig_version), sizeof(sig_version));
            is.read(reinterpret_cast<char *>(&sig_cap), sizeof(sig_cap));
            if (sig_version != SIGNATURE_VERSION) throw runtime_error("data version not supported.");
            is.read(reinterpret_cast<char *>(&N), sizeof(N));
            if (!is) runtime_error("error reading index file.");
            for (unsigned i = 0; i < KGRAPH_MAGIC_SIZE; ++i) {
                if (KGRAPH_MAGIC[i] != magic[i]) runtime_error("index corrupted.");
            }
            bool load_no_dist = sig_cap & FORMAT_NO_DIST;
            graph.resize(N);
            M.resize(N);
            vector<uint32_t> nids;
            vector<Neighbor> nns;
            for (unsigned i = 0; i < graph.size(); ++i) {
                auto &knn = graph[i];
                unsigned K;
                is.read(reinterpret_cast<char *>(&M[i]), sizeof(M[i]));
                is.read(reinterpret_cast<char *>(&K), sizeof(K));
                if (!is) runtime_error("error reading index file.");
                knn.resize(K);
                if (load_no_dist) {
                    nids.resize(K);
                    is.read(reinterpret_cast<char *>(&nids[0]), K * sizeof(nids[0]));
                    for (unsigned k = 0; k < K; ++k) {
                        knn[k] = nids[k];
                    }
                }
                else {
                    nns.resize(K);
                    is.read(reinterpret_cast<char *>(&nns[0]), K * sizeof(nns[0]));
                    for (unsigned k = 0; k < K; ++k) {
                        knn[k] = nns[k].id;
                    }
                }
            }
        }

        virtual void save (char const *path, int format) const  {
            throw runtime_error("not implemented.");
        }

        virtual void build (IndexOracle const &oracle, IndexParams const &param, IndexInfo *info) {
            throw runtime_error("not implemented.");
        }

        /*
        virtual void prune (unsigned K) {
            for (auto &v: graph) {
                if (v.size() > K) {
                    v.resize(K);
                }
            }
            return;
            vector<vector<unsigned>> pruned(graph.size());
            vector<set<unsigned>> reachable(graph.size());
            vector<bool> added(graph.size());
            for (unsigned k = 0; k < K; ++k) {
#pragma omp parallel for
                for (unsigned n = 0; n < graph.size(); ++n) {
                    vector<unsigned> const &from = graph[n];
                    if (from.size() <= k) continue;
                    unsigned e = from[k];
                    if (reachable[n].count(e)) {
                        added[n] = false;
                    }
                    else {
                        pruned[n].push_back(e);
                        added[n] = true;
                    }
                }
                // expand reachable
#pragma omp parallel for
                for (unsigned n = 0; n < graph.size(); ++n) {
                    vector<unsigned> const &to = pruned[n];
                    set<unsigned> &nn = reachable[n];
                    if (added[n]) {
                        for (unsigned v: pruned[to.back()]) {
                            nn.insert(v);
                        }
                    }
                    for (unsigned v: to) {
                        if (added[v]) {
                            nn.insert(pruned[v].back());
                        }
                    }
                }
            }
            graph.swap(pruned);
        }
        */

        virtual unsigned search (SearchOracle const &oracle, SearchParams const &params, unsigned *ids, float *dists, SearchInfo *pinfo) const {
            if (graph.size() > oracle.size()) {
                throw runtime_error("dataset larger than index");
            }
            if (params.P >= graph.size()) {
                if (pinfo) {
                    pinfo->updates = 0;
                    pinfo->cost = 1.0;
                }
                return oracle.search(params.K, params.epsilon, ids, dists);
            }
            vector<NeighborX> knn(params.K + params.P +1);
            vector<NeighborX> results;
            // flags access is totally random, so use small block to avoid
            // extra memory access
            boost::dynamic_bitset<> flags(graph.size(), false);

            if (params.init && params.T > 1) {
                throw runtime_error("when init > 0, T must be 1.");
            }

            unsigned seed = params.seed;
            unsigned updates = 0;
            if (seed == 0) seed = time(NULL);
            mt19937 rng(seed);
            unsigned n_comps = 0;
            for (unsigned trial = 0; trial < params.T; ++trial) {
                unsigned L = params.init;
                if (L == 0) {   // generate random starting points
                    vector<unsigned> random(params.P);
                    GenRandom(rng, &random[0], random.size(), graph.size());
                    for (unsigned s: random) {
                        if (!flags[s]) {
                            knn[L++].id = s;
                            //flags[s] = true;
                        }
                    }
                }
                else {          // user-provided starting points.
                    if (!ids) throw invalid_argument("no initial data provided via ids");
                    if (!(L < params.K)) throw invalid_argument("L < params.K");
                    for (unsigned l = 0; l < L; ++l) {
                        knn[l].id = ids[l];
                    }
                }
                for (unsigned k = 0; k < L; ++k) {
                    auto &e = knn[k];
                    flags[e.id] = true;
                    e.flag = true;
                    e.dist = oracle(e.id);
                    e.m = 0;
                    e.M = actual_M(params.M, e.id);
                }
                sort(knn.begin(), knn.begin() + L);

                unsigned k =  0;
                while (k < L) {
                    auto &e = knn[k];
                    if (!e.flag) { // all neighbors of this node checked
                        ++k;
                        continue;
                    }
                    unsigned beginM = e.m;
                    unsigned endM = beginM + params.S; // check this many entries
                    if (endM > e.M) {   // we are done with this node
                        e.flag = false;
                        endM = e.M;
                    }
                    e.m = endM;
                    // all modification to knn[k] must have been done now,
                    // as we might be relocating knn[k] in the loop below
                    auto const &neighbors = graph[e.id];
                    for (unsigned m = beginM; m < endM; ++m) {
                        unsigned id = neighbors[m];
                        //BOOST_VERIFY(id < graph.size());
                        if (flags[id]) continue;
                        flags[id] = true;
                        ++n_comps;
                        float dist = oracle(id);
                        NeighborX nn(id, dist);
                        unsigned r = UpdateKnnList(&knn[0], L, nn);
                        BOOST_VERIFY(r <= L);
                        //if (r > L) continue;
                        if (L + 1 < knn.size()) ++L;
                        if (r < L) {
                            knn[r].M = actual_M(params.M, id);
                            if (r < k) {
                                k = r;
                            }
                        }
                    }
                }
                if (L > params.K) L = params.K;
                if (results.empty()) {
                    results.reserve(params.K + 1);
                    results.resize(L + 1);
                    copy(knn.begin(), knn.begin() + L, results.begin());
                }
                else {
                    // update results
                    for (unsigned l = 0; l < L; ++l) {
                        unsigned r = UpdateKnnList(&results[0], results.size() - 1, knn[l]);
                        if (r < results.size() /* inserted */ && results.size() < (params.K + 1)) {
                            results.resize(results.size() + 1);
                        }
                    }
                }
            }
            results.pop_back();
            // check epsilon
            {
                for (unsigned l = 0; l < results.size(); ++l) {
                    if (results[l].dist > params.epsilon) {
                        results.resize(l);
                        break;
                    }
                }
            }
            unsigned L = results.size();
            /*
            if (!(L <= params.K)) {
                cerr << L << ' ' << params.K << endl;
            }
            */
            if (!(L <= params.K)) throw runtime_error("L <= params.K");
            // check epsilon
            if (ids) {
                for (unsigned k = 0; k < L; ++k) {
                    ids[k] = results[k].id;
                }
            }
            if (dists) {
                for (unsigned k = 0; k < L; ++k) {
                    dists[k] = results[k].dist;
                }
            }
            if (pinfo) {
                pinfo->updates = updates;
                pinfo->cost = float(n_comps) / graph.size();
            }
            return L;
        }

        virtual void get_nn (unsigned id, unsigned *nns, float *dist, unsigned *pM, unsigned *pL) const {
            throw runtime_error("not implemented.");
        }

        virtual void prune (IndexOracle const &oracle, unsigned level) {
            throw runtime_error("not implemented.");
        }

        void reverse (int rev_k) {
            throw runtime_error("not implemented.");
        }
    };

    KGraph *create_kgraph_lite () {
        return new KGraphLite;
    }
}

