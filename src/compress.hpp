/*
 * compress.h
 *
 *  Created on: Oct 5, 2018
 *      Author: elchaschab
 */

#ifndef SRC_COMPRESS_HPP_
#define SRC_COMPRESS_HPP_

#include <string>

namespace janosh {

std::string compress_string(const std::string &data);
std::string decompress_string(const std::string &data);

} /* namespace janosh */

#endif /* SRC_COMPRESS_HPP_ */
