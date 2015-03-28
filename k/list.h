#ifndef K_LIST_H
#define K_LIST_H

#include "etl/assert.h"

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

    void unlink() {
      this->next->prev = this->prev;
      this->prev->next = this->next;

      this->next = this->prev = this;
    }
  };

  /*
   * Creates an empty list.
   */
  List() {
    for (unsigned i = 0; i < config::n_priorities; ++i) {
      _roots[i] = { &_roots[i], &_roots[i] };
    }
  }

  /*
   * Checks whether the list is currently empty.
   *
   * Time: linear in n_priorities.
   */
  bool is_empty() const {
    for (auto const & it : _roots) {
      if (it.next != &it) return false;
    }
    return true;
  }

  /*
   * Unlinks the highest priority item from the list and returns its
   * owner.
   *
   * Precondition: list is not empty.
   *
   * TODO: merging this with the empty check and returning a Maybe would
   * improve performance.
   *
   * Time: linear in n_priorities.
   */
  T * take() {
    for (auto const & r : _roots) {
      if (r.next != &r) {
        // Cast safe due to invariant that sublists only contain a single
        // Itemoid, and it is the root.
        auto it = static_cast<Item *>(r.next);
        it->unlink();
        return it->owner;
      }
    }

    ETL_ASSERT(false);
  }


private:
  Itemoid _roots[config::n_priorities];
};

}  // namespace k

#endif  // K_LIST_H
