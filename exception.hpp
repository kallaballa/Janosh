#ifndef EXCEPTION_HPP_
#define EXCEPTION_HPP_

#include <string>
#include <exception>
#include <boost/exception/all.hpp>
#if 0
#include <boost/backtrace.hpp>
#endif

namespace janosh {
  typedef boost::error_info<struct tag_janosh_msg,std::string> msg_info;
  typedef boost::error_info<struct tag_janosh_string,std::vector<string> > string_info;

  struct janosh_exception :
#if 0
    public boost::backtrace,
#endif
    virtual boost::exception,
    virtual std::exception
  {};

    struct db_exception : virtual janosh_exception
    {};
    struct config_exception : virtual janosh_exception
    {};
}

#endif /* EXCEPTION_HPP_ */
