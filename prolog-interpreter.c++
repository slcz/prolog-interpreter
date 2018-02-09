/******************************************************************************
 *
 * A Toy Prolog Interpreter.
 * 2018, cubistolabs, inc.
 *
 *****************************************************************************/
#include <iostream>
#include "parser.h"

using namespace std;

int main()
{
	interp_context context(cin);
	return parser(context) ? 0 : 1;
}
