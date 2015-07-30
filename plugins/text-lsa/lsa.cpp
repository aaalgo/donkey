#include "../../src/donkey.h"
#include <iostream>
#include <fstream>

using namespace std;

namespace donkey{
    void Extractor::extract_path (string const &path, string const &type, Object *object) const {

        ifstream infile(path.c_str());

        if( !infile ){
            cerr<<"file open failed " << errno << ": '"<< path << "'" << endl;
            return ;
        }

        auto & ar = object->feature.data;

        for (int i = 0; i < LSA_DIM; ++i)
        {
            infile>>ar[i];
        }
        infile.close();
    }

}