#pragma once

#include <unordered_map>
#include <string>
#include <cstdint>

class unique_id {
private:
	std::unordered_map<std::string, uint64_t> id_map;
	uint64_t magic;
public:
	unique_id() : magic{0} {}
	void clear() { magic = 0; id_map.clear(); }
	uint64_t max() const {return magic;}
	uint64_t get_id(const std::string &name) {
		auto i = id_map.find(name);
		if (i == id_map.end()) {
			magic ++;
			id_map.insert(make_pair(name, magic));
			return magic;
		} else
			return i->second;
	}
};
