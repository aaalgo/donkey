#include <boost/filesystem.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include "../../../src/donkey.h"
#include "../config.h"
using namespace donkey;
using namespace std;

int main(int argc,char *argv[])
{
	Extractor ex((Config()));
	Object obj;
	ex.extract_path(argv[1],"",&obj);
	
    return 0;
}
