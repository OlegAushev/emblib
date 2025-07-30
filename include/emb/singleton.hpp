#pragma once

#if __cplusplus >= 201100
#include <cstdint>
#else
#include <stdint.h>
#endif
#include <cassert>
#include <cstddef>

namespace emb {

#if __cplusplus >= 201100

template<class Derived>
class singleton {
private:
  static inline Derived* instance_{nullptr};
  static inline bool initialized_{false};
protected:
  singleton(Derived* self) {
    assert(!initialized_);
    instance_ = self;
    initialized_ = true;
  }

  ~singleton() {
    initialized_ = false;
    instance_ = nullptr;
  }
public:
  static Derived* instance() {
    assert(initialized_);
    return instance_;
  }

  static bool initialized() { return initialized_; }
};

template<class Derived, size_t DerivedCount>
class singleton_array {
private:
  static inline Derived* instance_[DerivedCount]{};
  static inline bool initialized_[DerivedCount]{};
protected:
  singleton_array(Derived* self, size_t instance_idx) {
    assert(instance_idx < DerivedCount);
    assert(!initialized_[instance_idx]);
    instance_[instance_idx] = self;
    initialized_[instance_idx] = true;
  }
public:
  static Derived* instance(size_t instance_idx) {
    assert(instance_idx < DerivedCount);
    assert(initialized_[instance_idx]);
    return instance_[instance_idx];
  }

  static bool initialized(size_t instance_idx) {
    assert(instance_idx < DerivedCount);
    return initialized_[instance_idx];
  }
};

#else

template<class Derived>
class singleton {
private:
  static Derived* instance_;
  static bool initialized_;
protected:
  singleton(Derived* self) { register_object(self); }

  ~singleton() { deregister_object(); }
public:
  static Derived* instance() {
    assert(initialized_);
    return instance_;
  }

  static bool initialized() { return initialized_; }

  static void register_object(Derived* obj) {
    assert(!initialized_);
    instance_ = obj;
    initialized_ = true;
  }

  static void deregister_object() {
    initialized_ = false;
    instance_ = static_cast<Derived*>(NULL);
  }
};

template<class Derived>
Derived* singleton<Derived>::instance_ = static_cast<Derived*>(NULL);
template<class Derived>
bool singleton<Derived>::initialized_ = false;

template<class Derived, size_t DerivedCount>
class singleton_array {
private:
  static Derived* instance_[DerivedCount];
  static bool initialized_[DerivedCount];
protected:
  singleton_array(Derived* self, size_t instance_idx) {
    assert(instance_idx < DerivedCount);
    assert(!initialized_[instance_idx]);
    instance_[instance_idx] = self;
    initialized_[instance_idx] = true;
  }
public:
  static Derived* instance(size_t instance_idx) {
    assert(instance_idx < DerivedCount);
    assert(initialized_[instance_idx]);
    return instance_[instance_idx];
  }

  static bool initialized(size_t instance_idx) {
    assert(instance_idx < DerivedCount);
    return initialized_[instance_idx];
  }
};

template<class Derived, size_t DerivedCount>
Derived* singleton_array<Derived, DerivedCount>::instance_[DerivedCount];
template<class Derived, size_t DerivedCount>
bool singleton_array<Derived, DerivedCount>::initialized_[DerivedCount];

#endif

} // namespace emb
