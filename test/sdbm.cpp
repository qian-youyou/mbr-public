#include <iostream>

using namespace std;

unsigned int
SDBMHash(const std::string& str){
	unsigned int hash = 0;
	unsigned int i = 0;
	unsigned int len = str.length();

	for (i = 0; i < len; i++){
		hash = (str[i]) + (hash << 6) + (hash << 16) - hash;
	}
	return hash & 0x7FFFFFFF;
}

int main(int argc, char* argv[]){
    cout << argv[1] << " " << SDBMHash(argv[1]) << endl;
}
