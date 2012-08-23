#include "janosh.hpp"
#include <kcpolydb.h>

using namespace std;
using namespace kyotocabinet;

using std::cerr;
using std::cout;
using std::endl;
using std::string;

void printUsage() {
	std::cerr << "janosh [-s <value>] <json-file> <path>" << endl
			<< endl
			<< "<json-file>    the json file to query/manipulate" << endl
			<< "<path>         the json path (uses / as separator)" << endl
			<< endl
			<< "Options:" << endl
			<< endl
			<< "-s <value>     instead of querying for a path set its value" << endl
			<< endl;
	exit(1);
}

int main(int argc, char** argv) {
  using namespace std;
  int c;
  bool set_value = false;

  string key;
  string value;
  string filename;

  while ((c = getopt(argc, argv, "s:")) != -1) {
    switch (c) {
    case 's':
      set_value = true;
      value = optarg;
      break;
    case ':':
      printUsage();
      break;
    case '?':
      printUsage();
      break;
    }
  }

  if((argc - optind) != 2) {
    printUsage();
  }

  filename = argv[optind];
  key = argv[optind + 1];

  Janosh<std::map<string, Value *> > janosh(filename);
  janosh.load();
  janosh.dump();
  if(set_value) {
    janosh.set(key, value);
    janosh.save();
  } else {
    janosh.get(key,value);
    cout << value << endl;
  }
}
