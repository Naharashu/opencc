// This is rewrite of API for unordered_map

#include "opencc.h"


void *hashmap_get(HashMap& map, const std::string& key) {
  auto it = map.find(key);
  if(it!=map.end()) {
    return it->second;
  }
  return nullptr;
}
void *hashmap_get2(HashMap& map, const std::string& key, int keylen) {
  std::string k = key.substr(0, keylen);
  return hashmap_get(map, k);
}
void hashmap_put(HashMap& map, const std::string& key, void *val) {
  map[key] = val;
}
void hashmap_put2(HashMap& map, const std::string& key, int keylen, void *val) {
  map[key.substr(0, keylen)] = val;
}
void hashmap_delete(HashMap& map, const std::string& key) {
  auto it = map.find(key);
  if(it!=nullptr) map.erase(it);
}
void hashmap_delete2(HashMap& map, const std::string& key, int keylen) {
  hashmap_delete(map, key.substr(0, keylen));
}
// dummy
void hashmap_test(void) {
  return;
}