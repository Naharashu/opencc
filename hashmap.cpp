// This is rewrite of API for unordered_map

#include "opencc.h"
#include <algorithm>

std::unordered_map<std::string, void*> map;

void *hashmap_get(const std::string& key) {
  auto it = map.find(key);
  if(it!=map.end()) {
    return it->second;
  }
  return nullptr;
}
void *hashmap_get2(std::string key, int keylen);
void hashmap_put(const std::string& key, void *val) {
  map[key] = val;
}
void hashmap_put2(std::string key, int keylen, void *val);
void hashmap_delete(std::string key) {
  auto it = map.find(key);
  if(it!=nullptr) map.erase(it);
}
void hashmap_delete2(std::string key, int keylen);
// dummy
void hashmap_test(void) {
  return;
}