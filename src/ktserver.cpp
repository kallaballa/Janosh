/*************************************************************************************************
 * A handy cache/storage server
 *                                                               Copyright (C) 2009-2012 FAL Labs
 * This file is part of Kyoto Tycoon.
 * This program is free software: you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation, either version
 * 3 of the License, or any later version.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *************************************************************************************************/

#include "ktserver.hpp"
#include "cmdcommon.hpp"


enum {                                   // enumeration for operation counting
  CNTSET,                                // setting operations
  CNTSETMISS,                            // misses of setting operations
  CNTREMOVE,                             // removing operations
  CNTREMOVEMISS,                         // misses of removing operations
  CNTGET,                                // getting operations
  CNTGETMISS,                            // misses of getting operations
  CNTSCRIPT,                             // scripting operations
  CNTMISC                                // miscellaneous operations
};
typedef uint64_t OpCount[CNTMISC+1];     // counters per thread


// global variables
const char* g_progname;                  // program name
int32_t g_procid;                        // process ID number
double g_starttime;                      // start time
bool g_daemon;                           // daemon flag
kc::Compressor* bgscomp = NULL;
const char* pidpath = NULL;
OpCount* opcounts;
std::vector<std::string> dbpaths;
int32_t dbnum;
std::vector<kt::TimedDB*> dbs;
kt::UpdateLogger* ulog = NULL;
DBUpdateLogger* ulogdbs = NULL;

// function prototypes
static void usage();
static void killserver(int signum);
static std::vector<kt::TimedDB*> proc(const std::vector<std::string>& dbpaths,
                    const char* host, int32_t port, double tout, int32_t thnum,
                    const char* logpath, uint32_t logkinds,
                    const char* ulogpath, int64_t ulim, double uasi,
                    int32_t sid, int32_t omode, double asi, bool ash,
                    const char* bgspath, double bgsi, kc::Compressor* bgscomp, bool dmn,
                    const char* pidpath, const char* cmdpath, const char* scrpath,
                    const char* mhost, int32_t mport, const char* rtspath, double riv,
                    const char* plsvpath, const char* plsvex, const char* pldbpath);
//static bool dosnapshot(const char* bgspath, kc::Compressor* bgscomp,
//                       kt::TimedDB* dbs, int32_t dbnum, kt::RPCServer* serv);


// logger implementation
class Logger : public kt::RPCServer::Logger {
 public:
  // constructor
  explicit Logger() : strm_(NULL), lock_() {}
  // destructor
  ~Logger() {
    if (strm_) close();
  }
  // open the stream
  bool open(const char* path) {
    if (strm_) return false;
    if (path && *path != '\0' && std::strcmp(path, "-")) {
      std::ofstream* strm = new std::ofstream;
      strm->open(path, std::ios_base::out | std::ios_base::binary | std::ios_base::app);
      if (!*strm) {
        delete strm;
        return false;
      }
      strm_ = strm;
    } else {
      strm_ = &std::cout;
    }
    return true;
  }
  // close the stream
  void close() {
    if (!strm_) return;
    if (strm_ != &std::cout) delete strm_;
    strm_ = NULL;
  }
  // process a log message.
  void log(Kind kind, const char* message) {
    if (!strm_) return;
    char date[48];
    kt::datestrwww(kc::nan(), kc::INT32MAX, 6, date);
    const char* kstr = "MISC";
    switch (kind) {
      case kt::RPCServer::Logger::DEBUG: kstr = "DEBUG"; break;
      case kt::RPCServer::Logger::INFO: kstr = "INFO"; break;
      case kt::RPCServer::Logger::SYSTEM: kstr = "SYSTEM"; break;
      case kt::RPCServer::Logger::ERROR: kstr = "ERROR"; break;
    }
    lock_.lock();
    *strm_ << date << ": [" << kstr << "]: " << message << "\n";
    strm_->flush();
    lock_.unlock();
  }
 private:
  std::ostream* strm_;
  kc::Mutex lock_;
};

Logger* logger_;

void log_v(Logger::Kind kind, const char* format, va_list ap) {
  _assert_(format);
  std::string msg;
  kc::vstrprintf(&msg, format, ap);
  logger_->log(kind, msg.c_str());
}

void log(Logger::Kind kind, const char* format, ...) {
  _assert_(format);
  va_list ap;
  va_start(ap, format);
  log_v(kind, format, ap);
  va_end(ap);
}

// database logger implementation
class DBLogger : public kc::BasicDB::Logger {
 public:
  // constructor
  explicit DBLogger(::Logger* logger, uint32_t kinds) : logger_(logger), kinds_(kinds) {}
  // process a log message.
  void log(const char* file, int32_t line, const char* func,
           kc::BasicDB::Logger::Kind kind, const char* message) {
    kt::RPCServer::Logger::Kind rkind;
    switch (kind) {
      default: rkind = kt::RPCServer::Logger::DEBUG; break;
      case kc::BasicDB::Logger::INFO: rkind = kt::RPCServer::Logger::INFO; break;
      case kc::BasicDB::Logger::WARN: rkind = kt::RPCServer::Logger::SYSTEM; break;
      case kc::BasicDB::Logger::ERROR: rkind = kt::RPCServer::Logger::ERROR; break;
    }
    if (!(rkind & kinds_)) return;
    std::string lmsg;
    kc::strprintf(&lmsg, "[DB]: %s", message);
    logger_->log(rkind, lmsg.c_str());
  }
 private:
  ::Logger* logger_;
  uint32_t kinds_;
};


// replication slave implemantation
class Slave : public kc::Thread {
  friend class Worker;
 public:
  // constructor
  explicit Slave(uint16_t sid, const char* rtspath, const char* host, int32_t port, double riv,std::vector<kt::TimedDB*> dbs, int32_t dbnum,
                 kt::UpdateLogger* ulog, DBUpdateLogger* ulogdbs) :
      lock_(), sid_(sid), rtspath_(rtspath), host_(""), port_(port), riv_(riv),
      dbs_(dbs), dbnum_(dbnum), ulog_(ulog), ulogdbs_(ulogdbs),
      wrts_(kc::UINT64MAX), rts_(0), alive_(true), hup_(false) {
    if (host) host_ = host;
  }
  // stop the slave
  void stop() {
    alive_ = false;
  }
  // restart the slave
  void restart() {
    hup_ = true;
  }
  // set the configuration of the master
  void set_master(const std::string& host, int32_t port, uint64_t ts, double iv) {
    kc::ScopedSpinLock lock(&lock_);
    host_ = host;
    port_ = port;
    wrts_ = ts;
    if (iv >= 0) riv_ = iv;
  }
  // get the host name of the master
  std::string host() {
    kc::ScopedSpinLock lock(&lock_);
    return host_;
  }
  // get the port number name of the master
  int32_t port() {
    kc::ScopedSpinLock lock(&lock_);
    return port_;
  }
  // get the replication time stamp
  uint64_t rts() {
    return rts_;
  }
  // get the replication interval
  double riv() {
    return riv_;
  }
 private:
  static const int32_t DUMMYFREQ = 256;
  static const size_t RTSFILESIZ = 21;
  // perform replication
  void run(void) {
    if (!rtspath_) return;
    kc::File rtsfile;
    if (!rtsfile.open(rtspath_, kc::File::OWRITER | kc::File::OCREATE, kc::NUMBUFSIZ) ||
        !rtsfile.truncate(RTSFILESIZ)) {
      log(Logger::ERROR, "opening the RTS file failed: path=%s", rtspath_);
      return;
    }
    rts_ = read_rts(&rtsfile);
    write_rts(&rtsfile, rts_);
    kc::Thread::sleep(0.2);
    bool deferred = false;
    while (true) {
      lock_.lock();
      std::string host = host_;
      int32_t port = port_;
      uint64_t wrts = wrts_;
      lock_.unlock();
      if (!host.empty()) {
        if (wrts != kc::UINT64MAX) {
          lock_.lock();
          wrts_ = kc::UINT64MAX;
          rts_ = wrts;
          write_rts(&rtsfile, rts_);
          lock_.unlock();
        }
        kt::ReplicationClient rc;
        if (rc.open(host, port, 60, rts_, sid_)) {
          log(Logger::SYSTEM, "replication started: host=%s port=%d rts=%llu",
                     host.c_str(), port, (unsigned long long)rts_);
          hup_ = false;
          double rivsum = 0;
          while (alive_ && !hup_ && rc.alive()) {
            size_t msiz;
            uint64_t mts;
            char* mbuf = rc.read(&msiz, &mts);
            if (mbuf) {
              if (msiz > 0) {
                size_t rsiz;
                uint16_t rsid, rdbid;
                const char* rbuf = DBUpdateLogger::parse(mbuf, msiz, &rsiz, &rsid, &rdbid);
                if (rbuf && rsid != sid_ && rdbid < dbnum_) {
                  kt::TimedDB* db = dbs_[rdbid];
                  DBUpdateLogger* ulogdb = ulogdbs_ ? ulogdbs_ + rdbid : NULL;
                  if (ulogdb) ulogdb->set_rsid(rsid);
                  if (!db->recover(rbuf, rsiz)) {
                    const kc::BasicDB::Error& e = db->error();
                    log(Logger::ERROR, "recovering a database failed: %s: %s",
                               e.name(), e.message());
                  }
                  if (ulogdb) ulogdb->clear_rsid();
                }
                rivsum += riv_;
              } else {
                rivsum += riv_ * DUMMYFREQ / 4;
              }
              delete[] mbuf;
              while (rivsum > 100 && alive_ && !hup_ && rc.alive()) {
                kc::Thread::sleep(0.1);
                rivsum -= 100;
              }
            }
            if (mts > rts_) rts_ = mts;
          }
          rc.close();
          log(Logger::SYSTEM, "replication finished: host=%s port=%d",
                     host.c_str(), port);
          write_rts(&rtsfile, rts_);
          deferred = false;
        } else {
          if (!deferred) log(Logger::SYSTEM, "replication was deferred: host=%s port=%d",
                                    host.c_str(), port);
          deferred = true;
        }
      }
      if (alive_) {
        kc::Thread::sleep(1);
      } else {
        break;
      }
    }
    if (!rtsfile.close()) log(Logger::ERROR, "closing the RTS file failed");
  }
  // read the replication time stamp
  uint64_t read_rts(kc::File* file) {
    char buf[RTSFILESIZ];
    file->read_fast(0, buf, RTSFILESIZ);
    buf[sizeof(buf)-1] = '\0';
    return kc::atoi(buf);
  }
  // write the replication time stamp
  void write_rts(kc::File* file, uint64_t rts) {
    char buf[kc::NUMBUFSIZ];
    std::sprintf(buf, "%020llu\n", (unsigned long long)rts);
    if (!file->write_fast(0, buf, RTSFILESIZ)) {}
      log(Logger::SYSTEM, "writing the time stamp failed");
  }
  kc::SpinLock lock_;
  const uint16_t sid_;
  const char* const rtspath_;
  std::string host_;
  int32_t port_;
  double riv_;
  std::vector<kt::TimedDB*> const dbs_;
  const int32_t dbnum_;
  kt::UpdateLogger* const ulog_;
  DBUpdateLogger* const ulogdbs_;
  uint64_t wrts_;
  uint64_t rts_;
  bool alive_;
  bool hup_;
};

Slave* slave;
//
//// main routine
//int mains(int argc, char** argv) {
//  g_progname = argv[0];
//  g_procid = kc::getpid();
//  g_starttime = kc::time();
//  kc::setstdiobin();
//  kt::setkillsignalhandler(killserver);
//  if (argc > 1 && !std::strcmp(argv[1], "--version")) {
//    printversion();
//    return 0;
//  }
//  int32_t rv = run(argc, argv);
//  return rv;
//}


// print the usage and exit
static void usage() {
  eprintf("%s: Kyoto Tycoon: a handy cache/storage server\n", g_progname);
  eprintf("\n");
  eprintf("usage:\n");
  eprintf("  %s [-host str] [-port num] [-tout num] [-th num] [-log file] [-li|-ls|-le|-lz]"
          " [-ulog dir] [-ulim num] [-uasi num] [-sid num] [-ord] [-oat|-oas|-onl|-otl|-onr]"
          " [-asi num] [-ash] [-bgs dir] [-bgsi num] [-bgsc str]"
          " [-dmn] [-pid file] [-cmd dir] [-scr file]"
          " [-mhost str] [-mport num] [-rts file] [-riv num]"
          " [-plsv file] [-plex str] [-pldb file] [db...]\n", g_progname);
  eprintf("\n");
  std::exit(1);
}


// kill the running server
static void killserver(int signum) {
//  if (g_serv) {
//    g_serv->stop();
//    g_serv = NULL;
//    if (g_daemon && signum == SIGHUP) g_restart = true;
//    if (signum == SIGUSR1) g_restart = true;
//  }
}


// parse arguments of the command
std::vector<kt::TimedDB*> kt_run(int argc, char** argv) {
  bool argbrk = false;

  const char* host = NULL;
  int32_t port = kt::DEFPORT;
  double tout = DEFTOUT;
  int32_t thnum = DEFTHNUM;
  const char* logpath = NULL;
  uint32_t logkinds = kc::UINT32MAX;
  const char* ulogpath = NULL;
  int64_t ulim = DEFULIM;
  double uasi = 0;
  int32_t sid = -1;
  int32_t omode = kc::BasicDB::OWRITER | kc::BasicDB::OCREATE;
  double asi = 0;
  bool ash = false;
  const char* bgspath = NULL;
  double bgsi = DEFBGSI;
  bool dmn = false;
  const char* cmdpath = NULL;
  const char* scrpath = NULL;
  const char* mhost = NULL;
  int32_t mport = kt::DEFPORT;
  const char* rtspath = NULL;
  double riv = DEFRIV;
  const char* plsvpath = NULL;
  const char* plsvex = "";
  const char* pldbpath = NULL;
  for (int32_t i = 1; i < argc; i++) {
    if (!argbrk && argv[i][0] == '-') {
      if (!std::strcmp(argv[i], "--")) {
        argbrk = true;
      } else if (!std::strcmp(argv[i], "-host")) {
        if (++i >= argc) usage();
        host = argv[i];
      } else if (!std::strcmp(argv[i], "-port")) {
        if (++i >= argc) usage();
        port = kc::atoix(argv[i]);
      } else if (!std::strcmp(argv[i], "-tout")) {
        if (++i >= argc) usage();
        tout = kc::atof(argv[i]);
      } else if (!std::strcmp(argv[i], "-th")) {
        if (++i >= argc) usage();
        thnum = kc::atof(argv[i]);
      } else if (!std::strcmp(argv[i], "-log")) {
        if (++i >= argc) usage();
        logpath = argv[i];
      } else if (!std::strcmp(argv[i], "-li")) {
        logkinds = Logger::INFO | Logger::SYSTEM | Logger::ERROR;
      } else if (!std::strcmp(argv[i], "-ls")) {
        logkinds = Logger::SYSTEM | Logger::ERROR;
      } else if (!std::strcmp(argv[i], "-le")) {
        logkinds = Logger::ERROR;
      } else if (!std::strcmp(argv[i], "-lz")) {
        logkinds = 0;
      } else if (!std::strcmp(argv[i], "-ulog")) {
        if (++i >= argc) usage();
        ulogpath = argv[i];
      } else if (!std::strcmp(argv[i], "-ulim")) {
        if (++i >= argc) usage();
        ulim = kc::atoix(argv[i]);
      } else if (!std::strcmp(argv[i], "-uasi")) {
        if (++i >= argc) usage();
        uasi = kc::atof(argv[i]);
      } else if (!std::strcmp(argv[i], "-sid")) {
        if (++i >= argc) usage();
        sid = kc::atoix(argv[i]);
      } else if (!std::strcmp(argv[i], "-ord")) {
        omode &= ~kc::BasicDB::OWRITER;
        omode |= kc::BasicDB::OREADER;
      } else if (!std::strcmp(argv[i], "-oat")) {
        omode |= kc::BasicDB::OAUTOTRAN;
      } else if (!std::strcmp(argv[i], "-oas")) {
        omode |= kc::BasicDB::OAUTOSYNC;
      } else if (!std::strcmp(argv[i], "-onl")) {
        omode |= kc::BasicDB::ONOLOCK;
      } else if (!std::strcmp(argv[i], "-otl")) {
        omode |= kc::BasicDB::OTRYLOCK;
      } else if (!std::strcmp(argv[i], "-onr")) {
        omode |= kc::BasicDB::ONOREPAIR;
      } else if (!std::strcmp(argv[i], "-asi")) {
        if (++i >= argc) usage();
        asi = kc::atof(argv[i]);
      } else if (!std::strcmp(argv[i], "-ash")) {
        ash = true;
      } else if (!std::strcmp(argv[i], "-bgs")) {
        if (++i >= argc) usage();
        bgspath = argv[i];
      } else if (!std::strcmp(argv[i], "-bgsi")) {
        if (++i >= argc) usage();
        bgsi = kc::atof(argv[i]);
      } else if (!std::strcmp(argv[i], "-bgsc")) {
        if (++i >= argc) usage();
        const char* cn = argv[i];
        if (!kc::stricmp(cn, "zlib") || !kc::stricmp(cn, "gz")) {
          bgscomp = new kc::ZLIBCompressor<kc::ZLIB::RAW>;
        } else if (!kc::stricmp(cn, "lzo") || !kc::stricmp(cn, "oz")) {
          bgscomp = new kc::LZOCompressor<kc::LZO::RAW>;
        } else if (!kc::stricmp(cn, "lzma") || !kc::stricmp(cn, "xz")) {
          bgscomp = new kc::LZMACompressor<kc::LZMA::RAW>;
        }
      } else if (!std::strcmp(argv[i], "-dmn")) {
        dmn = true;
      } else if (!std::strcmp(argv[i], "-pid")) {
        if (++i >= argc) usage();
        pidpath = argv[i];
      } else if (!std::strcmp(argv[i], "-cmd")) {
        if (++i >= argc) usage();
        cmdpath = argv[i];
      } else if (!std::strcmp(argv[i], "-scr")) {
        if (++i >= argc) usage();
        scrpath = argv[i];
      } else if (!std::strcmp(argv[i], "-mhost")) {
        if (++i >= argc) usage();
        mhost = argv[i];
      } else if (!std::strcmp(argv[i], "-mport")) {
        if (++i >= argc) usage();
        mport = kc::atoix(argv[i]);
      } else if (!std::strcmp(argv[i], "-rts")) {
        if (++i >= argc) usage();
        rtspath = argv[i];
      } else if (!std::strcmp(argv[i], "-riv")) {
        if (++i >= argc) usage();
        riv = kc::atof(argv[i]);
      } else if (!std::strcmp(argv[i], "-plsv")) {
        if (++i >= argc) usage();
        plsvpath = argv[i];
      } else if (!std::strcmp(argv[i], "-plex")) {
        if (++i >= argc) usage();
        plsvex = argv[i];
      } else if (!std::strcmp(argv[i], "-pldb")) {
        if (++i >= argc) usage();
        pldbpath = argv[i];
      } else {
        usage();
      }
    } else {
      argbrk = true;
      dbpaths.push_back(argv[i]);
    }
  }
  if (port < 1 || thnum < 1 || mport < 1) usage();
  if (thnum > THREADMAX) thnum = THREADMAX;
  if (dbpaths.empty()) {
    if (pldbpath) usage();
    dbpaths.push_back(":");
  }
  dbs = proc(dbpaths, host, port, tout, thnum, logpath, logkinds,
                    ulogpath, ulim, uasi, sid, omode, asi, ash, bgspath, bgsi, bgscomp,
                    dmn, pidpath, cmdpath, scrpath, mhost, mport, rtspath, riv,
                    plsvpath, plsvex, pldbpath);
  return dbs;
}


// drive the server process
static std::vector<kt::TimedDB*> proc(const std::vector<std::string>& dbpaths,
                    const char* host, int32_t port, double tout, int32_t thnum,
                    const char* logpath, uint32_t logkinds,
                    const char* ulogpath, int64_t ulim, double uasi,
                    int32_t sid, int32_t omode, double asi, bool ash,
                    const char* bgspath, double bgsi, kc::Compressor* bgscomp, bool dmn,
                    const char* pidpath, const char* cmdpath, const char* scrpath,
                    const char* mhost, int32_t mport, const char* rtspath, double riv,
                    const char* plsvpath, const char* plsvex, const char* pldbpath) {
  g_daemon = false;
  if (dmn) {
    if (kc::File::PATHCHR == '/') {
      if (logpath && *logpath != kc::File::PATHCHR) {
        eprintf("%s: %s: a daemon can accept absolute path only\n", g_progname, logpath);
        return {};
      }
      if (ulogpath && *ulogpath != kc::File::PATHCHR) {
        eprintf("%s: %s: a daemon can accept absolute path only\n", g_progname, ulogpath);
        return {};
      }
      if (bgspath && *bgspath != kc::File::PATHCHR) {
        eprintf("%s: %s: a daemon can accept absolute path only\n", g_progname, bgspath);
        return {};
      }
      if (pidpath && *pidpath != kc::File::PATHCHR) {
        eprintf("%s: %s: a daemon can accept absolute path only\n", g_progname, pidpath);
        return {};
      }
      if (cmdpath && *cmdpath != kc::File::PATHCHR) {
        eprintf("%s: %s: a daemon can accept absolute path only\n", g_progname, cmdpath);
        return {};
      }
      if (scrpath && *scrpath != kc::File::PATHCHR) {
        eprintf("%s: %s: a daemon can accept absolute path only\n", g_progname, scrpath);
        return {};
      }
      if (rtspath && *rtspath != kc::File::PATHCHR) {
        eprintf("%s: %s: a daemon can accept absolute path only\n", g_progname, rtspath);
        return {};
      }
      if (plsvpath && *plsvpath != kc::File::PATHCHR) {
        eprintf("%s: %s: a daemon can accept absolute path only\n", g_progname, plsvpath);
        return {};
      }
      if (pldbpath && *pldbpath != kc::File::PATHCHR) {
        eprintf("%s: %s: a daemon can accept absolute path only\n", g_progname, pldbpath);
        return {};
      }
    }
    if (!kt::daemonize()) {
      eprintf("%s: switching to a daemon failed\n", g_progname);
      return {};
    }
    g_procid = kc::getpid();
    g_daemon = true;
  }
  if (ulogpath && sid < 0) {
    eprintf("%s: update log requires the server ID\n", g_progname);
    return {};
  }
  if (!cmdpath) cmdpath = kc::File::CDIRSTR;
  if (mhost) {
    if (sid < 0) {
      eprintf("%s: replication requires the server ID\n", g_progname);
      return {};
    }
    if (!rtspath) {
      eprintf("%s: replication requires the replication time stamp file\n", g_progname);
      return {};
    }
  }
  if (sid < 0) sid = 0;
  kc::File::Status sbuf;
  if (bgspath && !kc::File::status(bgspath, &sbuf) && !kc::File::make_directory(bgspath)) {
    eprintf("%s: %s: could not open the directory\n", g_progname, bgspath);
    return {};
  }
  if (!kc::File::status(cmdpath, &sbuf) || !sbuf.isdir) {
    eprintf("%s: %s: no such directory\n", g_progname, cmdpath);
    return {};
  }
  if (scrpath && !kc::File::status(scrpath)) {
    eprintf("%s: %s: no such file\n", g_progname, scrpath);
    return {};
  }
  if (dbpaths.size() > (size_t)OPENDBMAX) {
    eprintf("%s: too much databases\n", g_progname);
    return {};
  }
  logger_ = new Logger();
  if (!logger_->open(logpath)) {
    eprintf("%s: %s: could not open the log file\n", g_progname, logpath ? logpath : "-");
    return {};
  }
  log(Logger::SYSTEM, "================ [START]: pid=%d", g_procid);
  std::string addr = "";
  if (host) {
    addr = kt::Socket::get_host_address(host);
    if (addr.empty()) {
      log(Logger::ERROR, "unknown host: %s", host);
      return {};
    }
  }

  dbnum = dbpaths.size();

  if (ulogpath) {
    ulog = new kt::UpdateLogger;
    log(Logger::SYSTEM, "opening the update log: path=%s sid=%u", ulogpath, sid);
    if (!ulog->open(ulogpath, ulim, uasi)) {
      log(Logger::ERROR, "could not open the update log: %s", ulogpath);
      delete ulog;
      return {};
    }
    ulogdbs = new DBUpdateLogger[dbnum];
  }

  dbs.resize(dbnum);
  DBLogger dblogger(logger_, logkinds);

  std::map<std::string, int32_t> dbmap;
  for (int32_t i = 0; i < dbnum; i++) {
    dbs[i] = new kt::TimedDB();
    const std::string& dbpath = dbpaths[i];
    log(Logger::SYSTEM, "opening a database: path=%s", dbpath.c_str());
    if (logkinds != 0)
      dbs[i]->tune_logger(&dblogger, kc::BasicDB::Logger::WARN | kc::BasicDB::Logger::ERROR);
    if (ulog) {
      ulogdbs[i].initialize(ulog, sid, i);
      dbs[i]->tune_update_trigger(ulogdbs + i);
    }
    if (!dbs[i]->open(dbpath, omode)) {
      const kc::BasicDB::Error& e = dbs[i]->error();
      log(Logger::ERROR, "could not open a database file: %s: %s: %s",
               dbpath.c_str(), e.name(), e.message());
      for(auto db : dbs)
        delete db;
      delete[] ulogdbs;
      delete ulog;
      return {};
    }
    std::string path = dbs[i]->path();
    const char* rp = path.c_str();
    const char* pv = std::strrchr(rp, kc::File::PATHCHR);
    if (pv) rp = pv + 1;
    dbmap[rp] = i;
  }
  if (bgspath) {
    kc::DirStream dir;

    if (dir.open(bgspath)) {
      std::string name;
      while (dir.read(&name)) {
        const char* nstr = name.c_str();
        const char* pv = std::strrchr(nstr, kc::File::EXTCHR);
        int32_t idx = kc::atoi(nstr);
        if (*nstr >= '0' && *nstr <= '9' && pv && !kc::stricmp(pv + 1, BGSPATHEXT) &&
            idx >= 0 && idx < dbnum) {
          std::string path;
          kc::strprintf(&path, "%s%c%s", bgspath, kc::File::PATHCHR, nstr);
          uint64_t ssts;
          int64_t sscount, sssize;
          if (kt::TimedDB::status_snapshot_atomic(path, &ssts, &sscount, &sssize)) {
            log(Logger::SYSTEM,
                     "applying a snapshot file: db=%d ts=%llu count=%lld size=%lld",
                     idx, (unsigned long long)ssts, (long long)sscount, (long long)sssize);
            if (!dbs[idx]->load_snapshot_atomic(path, bgscomp)) {
              const kc::BasicDB::Error& e = dbs[idx]->error();
              log(Logger::ERROR, "could not apply a snapshot: %s: %s",
                       e.name(), e.message());
            }
          }
        }
      }
      dir.close();
    }
  }

  opcounts = new OpCount[thnum];
  for (int32_t i = 0; i < thnum; i++) {
    for (int32_t j = 0; j <= CNTMISC; j++) {
      opcounts[i][j] = 0;
    }
  }
  kc::CondMap condmap;

  if (pidpath) {
    char numbuf[kc::NUMBUFSIZ];
    size_t nsiz = std::sprintf(numbuf, "%d\n", g_procid);
    kc::File::write_file(pidpath, numbuf, nsiz);
  }

//    g_restart = false;
    slave = new Slave(sid, rtspath, mhost, mport, riv, dbs, dbnum, ulog, ulogdbs);
    slave->start();
    return dbs;
}

bool kt_cleanup() {
  bool err = false;
//
  slave->stop();
  slave->join();
  logger_->close();
  delete bgscomp;

  if (pidpath)
    kc::File::remove(pidpath);
  delete[] opcounts;

  for (int32_t i = 0; i < dbnum; i++) {
    const std::string& dbpath = dbpaths[i];
    log(Logger::SYSTEM, "closing a database: path=%s", dbpath.c_str());
    if (!dbs[i]->close()) {
      const kc::BasicDB::Error& e = dbs[i]->error();
      log(Logger::ERROR, "could not close a database file: %s: %s: %s", dbpath.c_str(), e.name(), e.message());
      err = true;
    }
  }
  for(auto db : dbs)
    delete db;

  if (ulog) {
    delete[] ulogdbs;
    if (!ulog->close()) {
      eprintf("%s: closing the update log faild\n", g_progname);
      err = true;
    }
    delete ulog;
  }

//  log(Logger::SYSTEM, "================ [FINISH]: pid=%d", g_procid);
  return err ? 1 : 0;
}

//
//// snapshot all databases
//static bool dosnapshot(const char* bgspath, kc::Compressor* bgscomp,
//                       kt::TimedDB* dbs, int32_t dbnum, kt::RPCServer* serv) {
//  bool err = false;
//  for (int32_t i = 0; i < dbnum; i++) {
//    kt::TimedDB* db = dbs + i;
//    std::string destpath;
//    kc::strprintf(&destpath, "%s%c%08d%c%s",
//                  bgspath, kc::File::PATHCHR, i, kc::File::EXTCHR, BGSPATHEXT);
//    std::string tmppath;
//    kc::strprintf(&tmppath, "%s%ctmp", destpath.c_str(), kc::File::EXTCHR);
//    int32_t cnt = 0;
//    while (true) {
//      if (db->dump_snapshot_atomic(tmppath, bgscomp)) {
//        if (!kc::File::rename(tmppath, destpath)) {
//          serv->log(Logger::ERROR, "renaming a file failed: %s: %s",
//                    tmppath.c_str(), destpath.c_str());
//        }
//        kc::File::remove(tmppath);
//        break;
//      }
//      kc::File::remove(tmppath);
//      const kc::BasicDB::Error& e = db->error();
//      if (e != kc::BasicDB::Error::LOGIC) {
//        serv->log(Logger::ERROR, "database error: %d: %s: %s", e.code(), e.name(), e.message());
//        break;
//      }
//      if (++cnt >= 3) {
//        serv->log(Logger::SYSTEM, "snapshotting was abandoned");
//        err = true;
//        break;
//      }
//      serv->log(Logger::INFO, "retrying snapshot: %d", cnt);
//    }
//    kc::Thread::yield();
//  }
//  return !err;
//}



// END OF FILE
