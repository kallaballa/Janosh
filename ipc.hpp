#ifndef IPC_HPP_
#define IPC_HPP_

#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/function_output_iterator.hpp>
#include <boost/generator_iterator.hpp>
#include <boost/foreach.hpp>
#include <functional>
#include <iostream>
#include <sstream>
#include <boost/interprocess/containers/string.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

using std::ostringstream;
using std::istringstream;
using boost::interprocess::shared_memory_object;
using boost::interprocess::interprocess_semaphore;
using boost::interprocess::mapped_region;
using boost::interprocess::create_only;
using boost::interprocess::open_only;
using boost::interprocess::read_write;
class shared_ring_buffer {
private:
  static const size_t _size = 4096;

  //Semaphores to protect and synchronize access
  boost::interprocess::interprocess_semaphore
     _mutex, _nempty, _nstored;

  size_t rear = 0, front = 0;
  char _buffer[_size];
public:

  typedef boost::interprocess::string string;

  typedef boost::generator_iterator_generator<std::function<string()> >::type InputIterator;
  typedef boost::function_output_iterator<std::function<void(const string&)> > OutputIterator;

  shared_ring_buffer() :
        _mutex(1),
        _nempty(_size),
        _nstored(0) {
   }

   ~shared_ring_buffer() {
   }

  void writeln(const string& str) {

    size_t n = 0;

    if(str.empty()) {
      this->_nempty.wait();
      this->_mutex.wait();

      this->_buffer[++front % shared_ring_buffer::_size] = 0;

      this->_mutex.post();
      this->_nstored.post();
    }

    while(n < str.size()) {
       for(; n < shared_ring_buffer::_size && n < str.size(); ++n){
         this->_nempty.wait();
         this->_mutex.wait();

         this->_buffer[(++front % shared_ring_buffer::_size)] = str.at(n);
         std::cerr << front % shared_ring_buffer::_size << ":" << this->_buffer[front % shared_ring_buffer::_size] << std::endl;

         this->_mutex.post();
         this->_nstored.post();
       }

       if(n == str.size()) {
         this->_nempty.wait();
         this->_mutex.wait();

         this->_buffer[++front % shared_ring_buffer::_size] = 0;

         this->_mutex.post();
         this->_nstored.post();

       }
     }
  }

  OutputIterator getOutputIterator() {
    std::function<void (const string&)> f = boost::bind( &shared_ring_buffer::writeln, this, _1 );

    return boost::make_function_output_iterator(f);
  }

  string readln() {
    //Extract the data
    string str;
    for(;;) {
      for(size_t i = 0; i < shared_ring_buffer::_size; ++i){
        this->_nstored.wait();
        this->_mutex.wait();

        const char c = this->_buffer[++rear % shared_ring_buffer::_size];
         std::cerr << rear % shared_ring_buffer::_size << ":" << this->_buffer[rear % shared_ring_buffer::_size] << std::endl;

         if(c == '\0') {
           this->_mutex.post();
           this->_nempty.post();
           return str;
         }
         else
           str += c;

         this->_mutex.post();
         this->_nempty.post();
      }
    }

    return str;
  }

  InputIterator getInputIterator()
  {
    std::function<string()> f = boost::bind(&shared_ring_buffer::readln, this);
    return boost::make_generator_iterator(f);
  }
};

#endif /* IPC_HPP_ */
