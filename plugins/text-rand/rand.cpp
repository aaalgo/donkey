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

        for (int i = 0; i < RAND_DIM; ++i)
        {
            infile>>ar[i];
            //if (path == "/home/qunzi/donkey/static_text/0000008.txt")
            //    std::cout<<i<<"   "<<ar[i]<<endl;
        }
        infile.close();
    }

}