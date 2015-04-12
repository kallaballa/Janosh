#include "xdo.hpp"

namespace janosh {
namespace lua {

XDO::XDO() :
    xdo_(xdo_new(NULL)) {
}

XDO::~XDO() {
  xdo_free(xdo_);
}

std::pair<unsigned int, unsigned int> XDO::getScreenSize() {
  unsigned int w;
  unsigned int h;
  xdo_get_viewport_dimensions(xdo_, &w, &h, 0);
  return std::make_pair(w,h);
}

void XDO::mouseMove(size_t x, size_t y) {
  xdo_move_mouse(xdo_, x, y, 0);
}

void XDO::mouseMoveRelative(int x, int y) {
  xdo_move_mouse_relative(xdo_, x, y);
}

void XDO::mouseDown(int button) {
  xdo_mouse_down(xdo_, CURRENTWINDOW, button);
}

void XDO::mouseUp(int button) {
  xdo_mouse_up(xdo_, CURRENTWINDOW, button);
}


void XDO::keyDown(const string& sequence) {
  xdo_send_keysequence_window_down(xdo_, CURRENTWINDOW, sequence.c_str(), 0);
}

void XDO::keyUp(const string& sequence) {
  xdo_send_keysequence_window_up(xdo_, CURRENTWINDOW, sequence.c_str(), 0);
}

void XDO::keyType(const string& sequence) {
  xdo_send_keysequence_window(xdo_, CURRENTWINDOW, sequence.c_str(), 0);
}

XDO* XDO::getInstance() {
  if (instance_ == NULL) {
    instance_ = new XDO();
  }
  return instance_;
}

XDO* XDO::instance_ = NULL;
}
}

