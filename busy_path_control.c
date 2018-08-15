uint32_t list[24];

uint32_t jenkins_one_at_a_time_hash(const char *key)
{
    uint32_t hash, i;
    size_t len = strlen(key);
    for(hash = i = 0; i < len; ++i)
    {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

int find_hash(uint32_t entry_hash) {
  int res = -1;
  for (int i = 0; i < 24; i++) {
    if (list[i] == entry_hash) {
      res = i;
      break;
    }
  }
  return res;
}

int add_hash(uint32_t entry_hash) {
  int res = -1;
  if (find_hash(entry_hash) == -1) {
    for (int i = 0; i < 24; i++) {
      if (list[i] == 0) {
        list[i] = entry_hash;
        res = i;
        break;
      }
    }
  }  
  return res;
}

void remove_hash(uint32_t entry_hash) {
  int i = find_hash(entry_hash);
  if (i != -1) {
    list[i] = 0;
  }
}