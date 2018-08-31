#ifndef RECPTR_H_BCF778F0
#define RECPTR_H_BCF778F0

namespace hatkirby {

  /**
   * This class provides a pointer wrapper for an object. The difference between
   * it and std::unique_ptr is that it has a defined copy constructor, which
   * uses the object's copy constructor to make a new object. This class is
   * useful for the situation in which an object needs to have a member variable
   * of the same type as itself.
   */
  template <typename T>
  class recptr {
  public:

    recptr() = default;

    recptr(T* ptr) : ptr_(ptr)
    {
    }

    ~recptr()
    {
      delete ptr_;
    }

    recptr(const recptr& other)
    {
      if (other.ptr_)
      {
        ptr_ = new T(*other.ptr_);
      }
    }

    recptr(recptr&& other)
    {
      std::swap(ptr_, other.ptr_);
    }

    recptr& operator=(const recptr& other)
    {
      if (ptr_)
      {
        delete ptr_;

        ptr_ = nullptr;
      }

      if (other.ptr_)
      {
        ptr_ = new T(*other.ptr_);
      }

      return *this;
    }

    recptr& operator=(recptr&& other)
    {
      if (ptr_)
      {
        delete ptr_;

        ptr_ = nullptr;
      }

      std::swap(ptr_, other.ptr_);

      return *this;
    }

    T& operator*()
    {
      return *ptr_;
    }

    T* operator->()
    {
      return ptr_;
    }

    const T& operator*() const
    {
      return *ptr_;
    }

    const T* operator->() const
    {
      return ptr_;
    }

    operator bool() const
    {
      return (ptr_ != nullptr);
    }

  private:

    T* ptr_ = nullptr;
  };

}

#endif /* end of include guard: RECPTR_H_BCF778F0 */
