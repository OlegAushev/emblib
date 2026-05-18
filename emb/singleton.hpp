#pragma once

#include <cstdint>
#include <cassert>
#include <cstddef>

namespace emb {

template<class Derived>
class singleton {
private:
  static inline Derived* instance_ = nullptr;
protected:
  singleton() {
    assert(!exists());
    instance_ = static_cast<Derived*>(this);
  }

  ~singleton() {
    instance_ = nullptr;
  }
public:
  [[nodiscard]] static Derived* instance() {
    assert(exists());
    return instance_;
  }

  [[nodiscard]] static bool exists() {
    return instance_ != nullptr;
  }
};

template<class Derived, std::size_t DerivedCount>
class singleton_array {
private:
  static inline Derived* instance_[DerivedCount]{};
  static inline bool initialized_[DerivedCount]{};
protected:
  singleton_array(Derived* self, std::size_t instance_idx) {
    assert(instance_idx < DerivedCount);
    assert(!initialized_[instance_idx]);
    instance_[instance_idx] = self;
    initialized_[instance_idx] = true;
  }
public:
  static Derived* instance(std::size_t instance_idx) {
    assert(instance_idx < DerivedCount);
    assert(initialized_[instance_idx]);
    return instance_[instance_idx];
  }

  static bool initialized(std::size_t instance_idx) {
    assert(instance_idx < DerivedCount);
    return initialized_[instance_idx];
  }
};

} // namespace emb
