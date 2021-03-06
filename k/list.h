#ifndef K_LIST_H
#define K_LIST_H

#include "k/config.h"
#include "k/maybe.h"
#include "k/panic.h"

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
    List * container;

    Item(T * o) : owner{o}, container{nullptr} {}

    void unlink() {
      this->container = nullptr;

      this->next->prev = this->prev;
      this->prev->next = this->next;

      this->next = this->prev = this;
    }

    void reinsert() {
      auto c = this->container;
      PANIC_UNLESS(c, "reinsert without insert");
      unlink();
      c->insert(this);
    }

    bool is_linked() const {
      return this->container;
    }
  };

  /*
   * Creates an empty list.
   */
  List() = default;

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
   * If the list is non-empty, unlinks and returns the highest priority item.
   *
   * If the list is empty, returns nothing.
   *
   * Time: linear in n_priorities.
   */
  Maybe<T *> take() {
    for (auto const & r : _roots) {
      if (r.next != &r) {
        // Cast safe due to invariant that sublists only contain a single
        // Itemoid, and it is the root.
        auto item = static_cast<Item *>(r.next);
        item->unlink();
        return {item->owner};
      }
    }

    return nothing;
  }

  /*
   * If the list is non-empty, returns (but does not unlink!) the highest
   * priority item.
   *
   * If the list is empty, returns nothing.
   *
   * Time: linear in n_priorities.
   */
  Maybe<Item *> peek() const {
    for (auto const & r : _roots) {
      if (r.next != &r) {
        // Cast safe due to invariant that sublists only contain a single
        // Itemoid, and it is the root.
        return static_cast<Item *>(r.next);
      }
    }

    return nothing;
  }

  void insert(Item * it) {
    PANIC_IF(it->container, "node already in list");
    PANIC_IF(it->next != it || it->prev != it, "corrupt node on insert");

    auto p = it->owner->get_priority();
    PANIC_UNLESS(p < config::n_priorities, "bogus priority");

    it->prev = _roots[p].prev;
    it->next = &_roots[p];
    it->container = this;

    _roots[p].prev = _roots[p].prev->next = it;
  }

private:
  Itemoid _roots[config::n_priorities];
};

}  // namespace k

#endif  // K_LIST_H
