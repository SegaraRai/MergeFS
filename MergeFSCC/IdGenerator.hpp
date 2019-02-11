#pragma once

#include <cstddef>
#include <unordered_set>


template<typename Key, typename Less = std::less<Key>, typename Hash = std::hash<Key>, typename Pred = std::equal_to<Key>, typename Allocator = std::allocator<Key>>
class IdGenerator {
  Key minimumUnusedId;
  std::unordered_set<Key, Hash, Pred, Allocator> usedIdSet;

public:
  IdGenerator(const Key& minimumId) : minimumUnusedId(minimumId) {}

  Key allocate() {
    const Key id = minimumUnusedId;
    do {
      minimumUnusedId++;
    } while (usedIdSet.count(minimumUnusedId));
    return id;
  }

  bool release(const Key& id) {
    if (usedIdSet.count(id)) {
      return false;
    }
    if (Less(id, minimumUnusedId)) {
      minimumUnusedId = id;
    }
    usedIdSet.erase(id);
    return true;
  }

  bool has(const Key& id) const {
    return usedIdSet.count(id);
  }
};
