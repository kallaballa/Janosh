#ifndef JANOSH_XDO_HPP_
#define JANOSH_XDO_HPP_

#include <string>
extern "C" {
#include <xdo.h>
}

namespace janosh {
namespace lua {
  using std::string;

  class XDO {
  private:
    static XDO* instance_;
    xdo_t *xdo_;
  public:
    XDO();
    ~XDO();

    std::pair<unsigned int, unsigned int> getScreenSize();
    void mouseMove(size_t x, size_t y);
    void mouseDown(int button);
    void mouseUp(int button);
    void mouseClick(int button);
    void keyDown(const string& sequence);
    void keyUp(const string& sequence);
    void keyType(const string& sequence);
    static XDO* getInstance();
  };
}
}

#endif
