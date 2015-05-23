#include <boost/filesystem.hpp>
#include <string>
#include <iostream>
#include <fstream>
#include "../../../src/donkey.h"
#include "../config.h"
using namespace donkey;
using namespace std;

int main(void)
{
	Extractor ex((Config()));
	Object obj;
	ex.extract_path("test/google.png",&obj);
for(auto &i:obj.feature.data){
	cout<<i<<endl;
}
	
    return 0;
}
