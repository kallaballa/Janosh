/*
s * Ctrl-Cut - A laser cutter CUPS driver
 * Copyright (C) 2009-2010 Amir Hassan <amir@viel-zu.org> and Marius Kintel <marius@kintel.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "logger.hpp"
#include "record.hpp"
#include <iostream>
#include <sstream>


/* Amirs' custom ThreadNameLookup .. !J! */
#include <thread>
using std::string;

class ThreadNameLookup {
  std::mutex mutex_;
  std::map<std::thread::id, string> idLookup_;
  std::map<string, std::thread::id> nameLookup_;
  std::map<string, size_t> nameCount_;

  static ThreadNameLookup* instance_;

  ThreadNameLookup() {}
public:
  static ThreadNameLookup* getInstance() {
    if(instance_ == NULL) {
      instance_ = new ThreadNameLookup();
    }

    return instance_;
  }

  static void set(const std::thread::id& threadID, const string& threadName) {
    getInstance()->mutex_.lock();
    auto itname = getInstance()->nameCount_.find(threadName);
    string name = threadName;
    size_t cnt = 0;
    if(itname != getInstance()->nameCount_.end()) {
      cnt = getInstance()->nameCount_[threadName];
    }

    name += ("-" + std::to_string(cnt));
    getInstance()->nameCount_[threadName] = cnt + 1;
    getInstance()->nameLookup_[name] = threadID;
    getInstance()->idLookup_[threadID] = name;
    getInstance()->mutex_.unlock();
  }

  static string get(const std::thread::id& threadID) {
    getInstance()->mutex_.lock();
    auto it = getInstance()->idLookup_.find(threadID);
    string name;
    if(it != getInstance()->idLookup_.end())
      name = (*it).second;
    else
      name = "Unknown";
    getInstance()->mutex_.unlock();
    return name;
  }

  static void remove(const std::thread::id& threadID) {
    getInstance()->mutex_.lock();
    string name = getInstance()->idLookup_[threadID];
    getInstance()->nameLookup_.erase(name);
    getInstance()->idLookup_.erase(threadID);
    getInstance()->mutex_.unlock();
  }

  static size_t size() {
    return getInstance()->idLookup_.size();
  }
};

 ThreadNameLookup* ThreadNameLookup::instance_ = NULL;

namespace janosh {
  Logger* Logger::instance_ = NULL;

  Logger::Logger(const LogLevel l) :
      tracing_(false), dblog_(false), level_(l) {
    plog::ConsoleAppender<plog::TxtFormatter>* consoleAppender = new plog::ConsoleAppender<plog::TxtFormatter>();
    switch (l) {
    case L_DEBUG:
      plog::init(plog::debug, consoleAppender);
      break;
    case L_INFO:
      plog::init(plog::info, consoleAppender);
      break;
    case L_WARNING:
      plog::init(plog::warning, consoleAppender);
      break;
    case L_ERROR:
      plog::init(plog::error, consoleAppender);
      break;
    case L_FATAL:
      plog::init(plog::fatal, consoleAppender);
      break;
    case L_GLOBAL:
      plog::init(plog::verbose, consoleAppender);
      break;
    }
  }

  DBLogger::~DBLogger() {
    _assert_(true);
  }

  void DBLogger::log(const char* file, int32_t line, const char* func, Kind kind, const char* message) {
    std::stringstream ss;
    ss << "DB: " << "(" << file << ":" << line << " in " << func << "):" << message;

    switch (kind) {
    case DEBUG:
        LOG(plog::debug) << ss.str();
      break;
      case INFO:
        LOG(plog::info) << ss.str();
      break;
      case WARN:
        LOG(plog::warning) << ss.str();
      break;
      case ERROR:
        LOG(plog::error) << ss.str();
      break;
    }
  }

  void Logger::init(const LogLevel l) {
    Logger::instance_ = new Logger(l);
  }

  Logger& Logger::getInstance() {
    assert(Logger::instance_ != NULL);
    return *Logger::instance_;
  }

  LogLevel Logger::getLevel() {
    return Logger::getInstance().level_;
  }

  bool Logger::isTracing() {
    return Logger::getInstance().tracing_;
  }

  void Logger::setTracing(bool t) {
    Logger::getInstance().tracing_ = t;
  }

  bool Logger::isDBLogging() {
    return Logger::getInstance().dblog_;
  }

  void Logger::setDBLogging(bool enabled) {
    Logger::getInstance().dblog_ = enabled;
    Record::db.tune_logger(&Logger::getInstance().dblogger_, kc::BasicDB::Logger::DEBUG
        | kc::BasicDB::Logger::INFO
        | kc::BasicDB::Logger::WARN
        | kc::BasicDB::Logger::ERROR);
  }

  void Logger::registerThread(const string& name) {
    ThreadNameLookup::set(std::this_thread::get_id(),name);
    LOG_DEBUG_MSG("Register thread", ThreadNameLookup::size());
  }

  void Logger::removeThread() {
    LOG_DEBUG("Remove thread");
    ThreadNameLookup::remove(std::this_thread::get_id());
  }

  void Logger::trace(const string& caller, std::initializer_list<janosh::Record> records) {
    LOG_DEBUG(makeCallString(caller, records));
  }

  const string Logger::makeCallString(const string& caller, std::initializer_list<janosh::Record> records) {
    std::stringstream ss;

    for(auto& rec : records) {
      ss << "\"" << rec.path().pretty() << "\",";
    }

    string arguments = ss.str();
    return caller + "(" + arguments.substr(0, arguments.size() - 1) + ")";
  }
}
