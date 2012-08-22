#include "janosh.hpp"

using std::cerr;
using std::cout;
using std::endl;
using std::string;

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
			break;
		case '?':
			break;
		}
	}

	filename = argv[optind];
	key = argv[optind + 1];
	Janosh janosh(filename);
	janosh.load();

	if(set_value) {
		if (janosh.set(key, value)) {
			janosh.save();
	        return 0;
		} else {
			cerr << "Not found: " << key << endl;
			return 1;
		}
	} else if (janosh.get(key, value)) {
		cout << value << endl;
		return 0;
	} else {
		cerr << "Not found: " << key << endl;
		return 1;
	}
}
