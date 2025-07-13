#pragma once

#if 0
#ifdef __cpp_variadic_templates

#include <emblib/array.hpp>
#include <emblib/core.hpp>

namespace emb {

namespace detail {

template<typename T>
struct node {
  T data;
  node* next;
};

template<typename T, size_t Size>
class node_pool {
public:
  using node_type = detail::node<T>;
private:
  alignas(node_type) std::array<char[sizeof(node_type)], Size> pool_;
  node_type* free_node;
public:
  node_pool() {
    for (auto& item : pool_) {
      node_type* node{reinterpret_cast<node_type*>(item)};
      node->next = nullptr;
    }
  }

  template<typename... Args>
  node_type* allocate(Args... args) {

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
