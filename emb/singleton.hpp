#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace emb {

// The class template `singleton` is a base class that registers the object
// of the class `Derived`, so that other code can obtain a pointer to that
// object with `instance()`. `Derived` inherits publicly from
// `singleton<Derived>` and must have at most one object at a time. The
// behavior is undefined if an object of a class that is neither `Derived`
// nor derived from `Derived` has a `singleton<Derived>` base.
//
// The object is registered by the constructor of its `singleton<Derived>`
// base and unregistered by the destructor of that base. It is therefore
// already registered while the members of `Derived` are initialized and the
// constructor body of `Derived` runs, and still registered while the
// destructor body runs and the members are destroyed: `instance()` can return
// a pointer to an object whose construction has not completed or whose
// destruction has begun.
//
// `instance()` and `exists()` may be called concurrently with each other,
// e.g. from interrupt handlers, but calling either of them concurrently with
// the construction or destruction of an object of `Derived` results in a data
// race.
template<class Derived>
class singleton {
private:
  static inline Derived* instance_ = nullptr;
protected:
  singleton()
  {
    assert(!exists());
    instance_ = static_cast<Derived*>(this);
  }

  ~singleton()
  {
    instance_ = nullptr;
  }
public:
  singleton(singleton const&) = delete;
  singleton& operator=(singleton const&) = delete;

  [[nodiscard]] static Derived* instance()
  {
    assert(exists());
    return instance_;
  }

  [[nodiscard]] static bool exists()
  {
    return instance_ != nullptr;
  }
};

// The class template `singleton_array` is a base class that registers objects
// of the class `Derived` under indices in [0, `DerivedCount`), at most one
// object per index, so that other code can obtain a pointer to an object by
// its index with `instance`. `Derived` inherits publicly from
// `singleton_array<Derived, DerivedCount>` and passes the index of the object
// to the constructor of that base. The behavior is undefined if an object of a
// class that is neither `Derived` nor derived from `Derived` has a
// `singleton_array<Derived, DerivedCount>` base.
//
// As with `singleton`, the object is registered by the constructor of its
// `singleton_array` base and unregistered by the destructor of that base, so
// `instance` can return a pointer to an object whose construction has not
// completed or whose destruction has begun.
//
// `instance` and `initialized` may be called concurrently with each other,
// e.g. from interrupt handlers, but calling either of them for an index
// concurrently with the construction or destruction of the object with that
// index results in a data race.
template<class Derived, std::size_t DerivedCount>
class singleton_array {
private:
  static inline Derived* instance_[DerivedCount]{};
  std::size_t instance_idx_;
protected:
  explicit singleton_array(std::size_t instance_idx)
      : instance_idx_(instance_idx)
  {
    assert(!initialized(instance_idx));
    instance_[instance_idx] = static_cast<Derived*>(this);
  }

  ~singleton_array()
  {
    instance_[instance_idx_] = nullptr;
  }
public:
  singleton_array(singleton_array const&) = delete;
  singleton_array& operator=(singleton_array const&) = delete;

  static Derived* instance(std::size_t instance_idx)
  {
    assert(initialized(instance_idx));
    return instance_[instance_idx];
  }

  static bool initialized(std::size_t instance_idx)
  {
    assert(instance_idx < DerivedCount);
    return instance_[instance_idx] != nullptr;
  }
};

} // namespace emb
