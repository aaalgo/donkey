#include <iostream>
#include <boost/program_options.hpp>
#include "donkey.h"

using namespace std;
using namespace donkey;

namespace po = boost::program_options; 

class JournalStat {
    size_t objects;
    size_t features;
public:
    JournalStat ()
        : objects(0),
        features(0)
    {
    }

    ~JournalStat () {
        cout << "Objects: " << objects << endl;
        cout << "Features: " << features << endl;
        cout << "F/O: " << 1.0 * features / objects << endl;
    }

    void operator () (uint16_t dbid, string const &key, string const &meta, Object *object) {
        ++objects;
        object->enumerate([this](unsigned tag, Feature const *ft) {
                ++features;
        });
    }
};

int main (int argc, char *argv[]) {

    std::string input;
    bool verbose = false;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message.")
    ("input,I", po::value(&input), "input file")
    ("verbose,v", "")
    ;

    po::positional_options_description p;
    p.add("input", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                     options(desc).positional(p).run(), vm);
    po::notify(vm); 

    if (vm.count("help") || (vm.count("input") == 0)) {
        std::cerr << "usage: journal-stat <input>" << std::endl;
        std::cerr << desc;
        return 1;
    }

    if (vm.count("verbose")) verbose = true;

    Journal journal(input, true);

    {
        JournalStat stat;
        journal.recover(std::ref(stat));
    }

    return 0;
}

