#pragma once

class NonCopyable
{
protected:
  NonCopyable() {}
  //don't copy or move
  NonCopyable(const NonCopyable&) = delete;
  NonCopyable(NonCopyable&&) = delete;
  const NonCopyable& operator=(const NonCopyable&) = delete;
  const NonCopyable& operator=(NonCopyable&&) = delete;
};

