/******************************************************************************
 *
 * A Toy Prolog Interpreter.
 * 2018, cubistolabs, inc.
 *
 *****************************************************************************/
#include <iostream>
#include <fstream>
#include <sstream>
#include "parser.h"

using namespace std;

int main(int argc, char **argv)
{
	vector<istream *> ios;
	for (int i = 1; i < argc; i ++) {
		ifstream *fs = new ifstream();
	       	fs->open(argv[i], fstream::in); 
		ios.push_back(fs);
	}
	ios.push_back(&cin);
	reverse(ios.begin(), ios.end());
	return program(ios) ? 0 : 1;
}
