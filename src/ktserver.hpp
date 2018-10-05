/*
 * ktserver.hpp
 *
 *  Created on: Oct 4, 2018
 *      Author: elchaschab
 */

#ifndef SRC_KTSERVER_HPP_
#define SRC_KTSERVER_HPP_

#include <cstdint>
#include <kttimeddb.h>

namespace kt = kyototycoon;

std::vector<kt::TimedDB*> kt_run(int argc, char** argv);
bool kt_cleanup();

#endif /* SRC_KTSERVER_HPP_ */
