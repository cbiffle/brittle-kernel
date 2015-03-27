#ifndef K_LIST_H
#define K_LIST_H

#include "k/config.h"

namespace k {

/*
 * List data structure used for a variety of purposes.
 *
 * A List actually contains several lists, one for each priority level.  This
 * allows for inexpensive access to the highest-priority item.
 */
template <typename T>
class List {
public:
  struct Itemoid {
    Itemoid * next;
    Itemoid * prev;

    Itemoid() : next{this}, prev{this} {}
  };

  struct Item : Itemoid {
    T * owner;

    Item(T * o) : owner{o} {}
  };

  /*
   * Creates an empty list.
   */
  List() {
    for (unsigned i = 0; i < config::n_priorities; ++i) {
      _roots[i] = { &_roots[i], &_roots[i] };
    }
  }

private:
  Itemoid _roots[config::n_priorities];
};

}  // namespace k

#endif  // K_LIST_H
