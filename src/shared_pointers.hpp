/*
 * shared_pointers.hpp
 *
 *  Created on: Feb 22, 2014
 *      Author: elchaschab
 */

#include <memory>
#include <iostream>
#include <zmq.hpp>

#ifndef SHARED_POINTERS_HPP_
#define SHARED_POINTERS_HPP_
using std::shared_ptr;

typedef shared_ptr<std::ostream> ostream_ptr;
typedef shared_ptr<zmq::socket_t> socket_ptr;

#endif /* SHARED_POINTERS_HPP_ */
