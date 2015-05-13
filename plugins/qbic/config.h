#ifndef AAALGO_DONKEY_QBIC
#define AAALGO_DONKEY_QBIC

#include "donkey-common.h"

namespace donkey {

    class Feature: public vector<float> {
    };

    struct Distance {
        static float apply (Feature const &f1, Feature const &f2) {
            return 0;
        }
    };

    class Object {
    public:
        void enumerate (function<void(unsigned tag, Feature const *ft)> callback) const {
        }
        void read (std::istream &is) {
        }
        void write (std::ostream &os) const {
        }

        void swap (Object &o) {
        }
    };

    class Extractor {
    public:
        Extractor (Config const &config) {
        }

        void extract_url (string const &url, Object *object) const {
        }

        void extract (string const &content, Object *object) const {
        }
    };

    class Matcher {
    public:
        Matcher (Config const &config) {
        };

        float match (Object const &query, Candidate const &cand) const {
            return 0;
        }
    };

}


#endif
