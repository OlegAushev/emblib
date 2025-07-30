#pragma once

#if 0
#ifdef __cpp_variadic_templates

#include <emb/array.hpp>
#include <emb/core.hpp>

namespace emb {

namespace detail {

template<typename T>
struct node {
  T data;
  node* prev;
  node* next;
};

template<typename T, size_t Size>
class node_pool {
public:
  using node_type = detail::node<T>;
  using node_storage_type = unsigned char[sizeof(node_type)];
private:
  alignas(node_type) std::array<node_storage_type, Size> pool_;
  node_type* free_node_;
public:
  node_pool() {
    for (auto& item : pool_) {
      node_type* node{reinterpret_cast<node_type*>(item)};
      node->prev = nullptr;
      node->next = nullptr;
    }

    free_node_ = reinterpret_cast<node_type*>(pool_[0]); // ???
  }

  template<typename... Args>
  node_type* allocate(Args&&... args) {
    if (!free_node_) {
      return nullptr;
    }

    node_type* p{new(free_node_) node_type(std::forward<Args>(args)...)};

    // TODO ...

    return p;
  }

  void deallocate(node_type* p) {
    if (!p) {
      return;
    }

    p->~node_type();

  }
};

}

class static_forward_list {
private:
public:
};

}

#endif
#endif
