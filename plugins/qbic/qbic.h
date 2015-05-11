#ifndef AAALGO_DONKEY_QBIC
#define AAALGO_DONKEY_QBIC

namespace donkey {

    class Object {
    public:
        void read ();
        void write ();
    };

    class Extractor {
    public:
        Extractor (Config const &config) {
        }

        void extract (string const &path, Object *);
        void extract (
    };

    class Ranker {
    public:
        Ranker (Config const &config) {
        };
    };

}


#endif
