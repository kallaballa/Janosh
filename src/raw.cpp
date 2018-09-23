/*
 * raw.cpp
 *
 *  Created on: Feb 16, 2014
 *      Author: elchaschab
 */

#include "raw.hpp"

namespace janosh {
RawPrintVisitor::RawPrintVisitor(std::ostream& out) :
    PrintVisitor(out) {
}

void RawPrintVisitor::record(const Path& p, const Value& value, bool array, bool first) {
  string stripped = value.str();
  replace(stripped.begin(), stripped.end(), '\n', ' ');
  out << stripped << '\n';
}
} /* namespace janosh */
