#include <boost/algorithm/string.hpp>
#include <iostream>
#include "bash.hpp"

namespace janosh {
BashPrintVisitor::BashPrintVisitor(std::ostream& out) : PrintVisitor(out) {
}

void BashPrintVisitor::begin() {
  out << "( ";
}

void BashPrintVisitor::close() {
  out << ")" << '\n';
}

void BashPrintVisitor::record(const Path& p, const Value& value, bool array, bool first) {
  string stripped = value.str();
  replace(stripped.begin(), stripped.end(), '\n', ' ');
  boost::algorithm::replace_all(stripped, "&", "&#38;");
  boost::algorithm::replace_all(stripped, "'", "&#39;");

  out << '[' << p.pretty() << "]='" << stripped << "' ";
}
}
