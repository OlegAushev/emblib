#pragma once

namespace emb {

class noncopyable {
protected:
  noncopyable() = default;
  ~noncopyable() = default;
public:
  noncopyable(noncopyable const& other) = delete;
  noncopyable& operator=(noncopyable const& other) = delete;
};

} // namespace emb
