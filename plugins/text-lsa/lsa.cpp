#include "../../src/donkey.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

namespace donkey{
    void Extractor::extract_path (string const &path, string const &type, Object *object) const {

        ifstream infile(path.c_str());

        if( !infile ){
            throw PluginError("cannot open file");
        }

        auto & ar = object->feature.data;

        for (int i = 0; i < LSA_DIM; ++i)
        {
            infile>>ar[i];
        }
        if (!infile) throw PluginError("corrupt file");
    }

    void Extractor::extract (string const &content, string const &type, Object *object) const {
        istringstream ss(content);
        auto & ar = object->feature.data;
        for (int i = 0; i < LSA_DIM; ++i)
        {
            ss >> ar[i];
        }
        if (!ss) throw PluginError("bad data format");
    }

}
