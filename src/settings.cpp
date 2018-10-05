/*
 * settings.cpp
 *
 *  Created on: Feb 9, 2014
 *      Author: elchaschab
 */

#include "settings.hpp"
#include <fstream>
#include <exception>


namespace janosh {
using std::ifstream;
using std::exception;
Settings::Settings() {
   const char* home = getenv ("HOME");
   if (home==NULL) {
     error("Can't find environment variable.", "HOME");
   }
   string janoshDir = string(home) + "/.janosh/";
   fs::path dir(janoshDir);

   if (!fs::exists(dir)) {
     if (!fs::create_directory(dir)) {
       error("can't create directory", dir.string());
     }
   }

   this->janoshFile = fs::path(dir.string() + "janosh.json");

   if(!fs::exists(janoshFile)) {
     error("janosh configuration not found: ", janoshFile);
   } else {
     js::Value janoshConf;

     try{
       ifstream is(janoshFile.c_str());
       js::read(is,janoshConf);
       js::Object jObj = janoshConf.get_obj();
       js::Value v;

       if(find(jObj, "maxThreads", v)) {
            this->maxThreads = std::stoi(v.get_str());
       } else {
         error(this->janoshFile.string(), "maxThreads definition not found");
       }

       if(find(jObj, "dbstring", v)) {
            this->dbString = v.get_str();
       }

       if(find(jObj, "ktopts", v)) {
            this->ktopts = v.get_str();
       }

       if(find(jObj, "bindUrl", v)) {
            this->bindUrl = v.get_str();
       }

       if(find(jObj, "connectUrl", v)) {
            this->connectUrl = v.get_str();
       }
     } catch (exception& e) {
       error("Unable to load janosh configuration", e.what());
     }
   }
 }

 bool Settings::find(const js::Object& obj, const string& name, js::Value& value) {
   auto it = find_if(obj.begin(), obj.end(),
       [&](const js::Pair& p){ return p.name_ == name;});
   if (it != obj.end()) {
     value = (*it).value_;
     return true;
   }
   return false;
 }
}
/* namespace janosh */
