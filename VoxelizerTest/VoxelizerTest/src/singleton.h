#pragma once

#include <memory>

#include "non_copyable.h"

template<typename T>
class Singleton : public NonCopyable
{
public:
  ~Singleton() {}

  static T& GetInstance()
  {
    if(s_instance == nullptr)
      s_instance = std::unique_ptr<T>(new T());
    return *s_instance;
  }
  static T& GI() { return GetInstance(); }

  static void Delete() { s_instance.reset(); }

protected:
  Singleton() {}
private:

  static std::unique_ptr<T> s_instance;
};

template<typename T>
std::unique_ptr<T> Singleton<T>::s_instance;

