
#ifndef TDAVIXFILEINTERNAL_H
#define	TDAVIXFILEINTERNAL_H

#include "TUrl.h"
#include "TMutex.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sstream>
#include <string>
#include <cstring>


//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TDavixFileInternal                                                   //
//                                                                      //
//                                                                      //
// Support class, common to TDavixFile and TDavixSystem                 //
//                                                                      //
// Authors:     Adrien Devresse (CERN IT/SDC)                           //
//              Fabrizio Furano (CERN IT/SDC)                           //
//                                                                      //
// September 2013                                                       //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

namespace Davix {
class Context;
class RequestParams;
class DavPosix;
}

class TDavixFileInternal {
  friend class TDavixFile;
  friend class TDavixSystem;

  TDavixFileInternal(TUrl & mUrl, Option_t* mopt) :
  positionLock(NULL),
  openLock(NULL),
  davixContext(NULL),
  davixParam(NULL),
  davixPosix(NULL),
  davixFd(NULL),
  fUrl(mUrl),
  opt(mopt) {

  }

  ~TDavixFileInternal();

  inline Davix_fd * getDavixFileInstance() {
    // singleton init
    if (davixFd == NULL) {
      TLockGuard l(&(openLock));
      if (davixFd == NULL) {
        davixFd = this->Open();
      }
    }
    return davixFd;
  }

  Davix_fd * Open();

  void Close();

  void enableGridMode();

  void setS3Auth(const std::string & key, const std::string & token);

  void parseParams(Option_t* option);

  void init();


  TMutex *positionLock;
  TMutex openLock;

  // DAVIX
  Davix::Context *davixContext;
  Davix::RequestParams *davixParam;
  Davix::DavPosix *davixPosix;
  Davix_fd *davixFd;
  TUrl &fUrl;
  Option_t* opt;

public:
  
  Int_t DavixStat(const char *url, struct stat *st);
  
};


#endif	/* TDAVIXFILEINTERNAL_H */

