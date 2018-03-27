#ifndef PROGRESS_H_A34EF856
#define PROGRESS_H_A34EF856

#include <string>
#include <iostream>

namespace hatkirby {

  class progress {
  public:

    progress(
      std::string message,
      unsigned long total) :
        message(message),
        total(total)
    {
      std::cout << message << "   0%" << std::flush;
    }

    void update(unsigned long val)
    {
      if (val <= total)
      {
        cur = val;
      } else {
        cur = total;
      }

      unsigned long pp = cur * 100 / total;
      if (pp != lprint)
      {
        lprint = pp;

        std::cout << "\b\b\b\b" << std::right;
        std::cout.width(3);
        std::cout << pp << "%" << std::flush;
      }
    }

    void update()
    {
      update(cur+1);
    }

    ~progress()
    {
      std::cout << "\b\b\b\b100%" << std::endl;
    }

  private:

    std::string message;
    unsigned long total;
    unsigned long cur = 0;
    unsigned long lprint = 0;
  };

}

#endif /* end of include guard: PROGRESS_H_A34EF856 */
