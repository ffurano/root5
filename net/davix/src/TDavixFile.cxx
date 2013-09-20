/*************************************************************************
 * Copyright (C) 2013, Tigran Mkrtchyan <tigran.mkrtchyan@desy.de>.   *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/


//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TDavixFile                                                           //
//                                                                      //
// A TDavixFile is like a normal TFile except that it uses              //
// libdavix to read/write remote files.                                 //
// It supports HTTP and HTTPS in a number of dialects and options       //
//  e.g. S3 is one of them                                              //
// Other caracteristics come from the full support of Davix,            //
//  e.g. full redirection support in any circumstance                   //
//                                                                      //
// Authors:     Adrien Devresse (CERN IT/SDC)                           //
//              Tigran Mkrtchyan (DESY)                                 //
//                                                                      //
// Checks and ROOT5 porting:                                            //
//              Fabrizio Furano (CERN IT/SDC)                           //
//                                                                      //
// September 2013                                                       //
//                                                                      //
//////////////////////////////////////////////////////////////////////////


#include "TDavixFile.h"
#include "TROOT.h"
#include "TSocket.h"
#include "Bytes.h"
#include "TError.h"
#include "TSystem.h"
#include "TEnv.h"
#include "TBase64.h"
#include "TVirtualPerfStats.h"
#include "TDavixFileInternal.h"
#include "TSocket.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <davix.hpp>
#include <sstream>
#include <string>
#include <cstring>


static const std::string VERSION = "0.0.1";

static const std::string gUserAgent = "ROOT/" + std::string(gROOT->GetVersion()) +
" TDavixFile/" + VERSION + " davix/" + Davix::version();

// The prefix that is used to find the variables in the gEnv
#define ENVPFX	"Davix."

ClassImp(TDavixFile)

using namespace Davix;

const char* grid_mode_opt = "grid_mode=yes";
const char* ca_check_opt = "ca_check=no";
const char* s3_seckey_opt = "s3seckey=";
const char* s3_acckey_opt = "s3acckey=";


bool isno(const char *str) {
  if (!str) return false;
  
  if (!strcmp(str, "n") || !strcmp(str, "no") || !strcmp(str, "0") || !strcmp(str, "false")) return true;
    
  return false;
  
}



//____________________________________________________________________________

static void ConfigureDavixLogLevel() {
  Int_t log_level = (gEnv) ? gEnv->GetValue("Davix.Debug", 0) : 0;

  switch (log_level) {
    case 0:
      davix_set_log_level(0);
      break;
    case 1:
      davix_set_log_level(DAVIX_LOG_WARNING);
      break;
    case 2:
      davix_set_log_level(DAVIX_LOG_VERBOSE);
      break;
    case 3:
      davix_set_log_level(DAVIX_LOG_DEBUG);
      break;
    default:
      davix_set_log_level(DAVIX_LOG_ALL);
      break;
  }
}

///////////////////////////////////////////////////////////////////
// Authn implementation, Locate and get VOMS cred if exist

inline void TDavixFile_http_get_ucert(std::string& ucert, std::string& ukey) {
  char default_proxy[64];
  const char *genvvar = 0, *genvvar1 = 0;
  // The gEnv has higher priority, let's look for a proxy cert
  genvvar = gEnv->GetValue("Davix.GSI.UserProxy", (const char*) NULL);
  if (genvvar) {
    ucert = ukey = genvvar;
    if (gDebug > 0)
      Info("TDavixFile_http_get_ucert", "Found proxy in gEnv");
    return;
  }

  // Try explicit environment for proxy
  if (getenv("X509_USER_PROXY")) {
    if (gDebug > 0)
      Info("TDavixFile_http_get_ucert", "Found proxy in X509_USER_PROXY");
    ucert = ukey = getenv("X509_USER_PROXY");
    return;
  }

  // Try with default location
  snprintf(default_proxy, sizeof (default_proxy), "/tmp/x509up_u%d",
          geteuid());

  if (access(default_proxy, R_OK) == 0) {
    if (gDebug > 0)
      Info("TDavixFile_http_get_ucert", "Found proxy in /tmp");

    ucert = ukey = default_proxy;
    return;
  }

  // It seems we got no proxy, let's try to gather the keys
  genvvar = gEnv->GetValue("Davix.GSI.UserCert", (const char*) NULL);
  genvvar1 = gEnv->GetValue("Davix.GSI.UserKey", (const char*) NULL);
  if (genvvar || genvvar1) {
    if (gDebug > 0)
      Info("TDavixFile_http_get_ucert", "Found cert and key in gEnv");

    ucert = genvvar;
    ukey = genvvar1;
    return;
  }

  // try with X509_* environment
  if (getenv("X509_USER_CERT"))
    ucert = getenv("X509_USER_CERT");
  if (getenv("X509_USER_KEY"))
    ukey = getenv("X509_USER_KEY");

  if ((ucert.size() > 0) || (ukey.size() > 0)) {
    if (gDebug > 0)
      Info("TDavixFile_http_get_ucert", "Found cert and key in gEnv");

    
  
  }
  return;
  
}

static int TDavixFile_http_authn_cert_X509(void* userdata, const Davix::SessionInfo & info,
        Davix::X509Credential * cert, Davix::DavixError** err) {
  (void) userdata; // keep quiete compilation warnings
  (void) info;
  std::string ucert, ukey;
  TDavixFile_http_get_ucert(ucert, ukey);

  if (ucert.empty() || ukey.empty()) {
    Davix::DavixError::setupError(err, "TDavixFile",
            Davix::StatusCode::AuthentificationError,
            "Could not set the user's proxy or certificate");
    return -1;
  }
  return cert->loadFromFilePEM(ukey, ucert, "", err);
}
/////////////////////////////////////////////////////////////////////////////////////////////


TDavixFileInternal::~TDavixFileInternal() {
  delete davixPosix;
  delete davixContext;
  delete davixParam;
  delete positionLock;
}

Davix_fd * TDavixFileInternal::Open() {
  DavixError *davixErr = NULL;
  Davix_fd *fd = davixPosix->open(davixParam, fUrl.GetUrl(), O_RDONLY, &davixErr);
  if (fd == NULL) {
    Error("DavixOpen", "failed to open file with davix: %s (%d)",
            davixErr->getErrMsg().c_str(), davixErr->getStatus());
    DavixError::clearError(&davixErr);
  }
  return fd;
}

void TDavixFileInternal::Close() {
  DavixError *davixErr = NULL;
  if (davixFd != NULL && davixPosix->close(davixFd, &davixErr)) {
    Error("DavixClose", "failed to close file with davix: %s (%d)",
            davixErr->getErrMsg().c_str(), davixErr->getStatus());
    DavixError::clearError(&davixErr);
  }
}

void TDavixFileInternal::enableGridMode() {
  const char * env_var = NULL;

  if (gDebug > 1)
    Info("enableGridMode", " grid mode enabled !");

  env_var = gEnv->GetValue("Davix.GSI.CAdir", (const char*) NULL);
  if (!env_var)
    env_var = (((char*) getenv("X509_CERT_DIR")) ? env_var : "/etc/grid-security/certificates/");

  if (env_var) {
    davixParam->addCertificateAuthorityPath(env_var);
    if (gDebug > 0)
      Info("enableGridMode", "Setting CAdir to %s", env_var);
  }

  davixParam->setTransparentRedirectionSupport(true);
  davixParam->setClientCertCallbackX509(&TDavixFile_http_authn_cert_X509, NULL);
  
  
  env_var = gEnv->GetValue("Davix.GSI.CACheck", (const char*)"y");
  if (!isno(env_var)) {
   if (gDebug > 0)
     Info("enableGridMode", "Setting CAcheck to yes");
   davixParam->setSSLCAcheck(true);
  }
  else {
   if (gDebug > 0)
     Info("enableGridMode", "Setting CAcheck to no");
   davixParam->setSSLCAcheck(false);
  }
  
  
}

void TDavixFileInternal::setS3Auth(const std::string & key, const std::string & token) {
  if (gDebug > 1)
    Info("setS3Auth", " Aws S3 tokens configured");
  davixParam->setAwsAuthorizationKeys(key, token);
  davixParam->setProtocol(RequestProtocol::AwsS3);
}

void TDavixFileInternal::parseParams(Option_t* option) {
  // intput params
  std::stringstream ss(option);
  std::string item;
  std::vector<std::string> parsed_options;
  // parameters
  std::string s3seckey, s3acckey;

  const char *env_var = gEnv->GetValue("Davix.GSI.GridMode", (const char*) NULL);
  if (env_var && (!strcasecmp(env_var, "y") || !strcmp(env_var, "1")))
    enableGridMode();



  while (std::getline(ss, item, ' ')) {
    parsed_options.push_back(item);
  }

  for (std::vector<std::string>::iterator it = parsed_options.begin(); it < parsed_options.end(); ++it) {
    if ((strcasecmp(it->c_str(), grid_mode_opt)) == 0) {
      enableGridMode();
    }
    if ((strcasecmp(it->c_str(), ca_check_opt)) == 0) {
      davixParam->setSSLCAcheck(false);
    }
    if (strncasecmp(it->c_str(), s3_seckey_opt, strlen(s3_seckey_opt)) == 0) {
      s3seckey = std::string(it->c_str() + strlen(s3_seckey_opt));
    }

    if (strncasecmp(it->c_str(), s3_acckey_opt, strlen(s3_acckey_opt)) == 0) {
      s3acckey = std::string(it->c_str() + strlen(s3_acckey_opt));
    }
  }

  if (s3seckey.size() > 0) {
    setS3Auth(s3seckey, s3acckey);
  }
}

void TDavixFileInternal::init() {
  davixContext = new Context();
  davixPosix = new DavPosix(davixContext);
  davixParam = new RequestParams();
  davixParam->setUserAgent(gUserAgent);
  positionLock = new TMutex();
  ConfigureDavixLogLevel();
  parseParams(opt);
}





Int_t TDavixFileInternal::DavixStat(const char *url, struct stat *st) {
  DavixError *davixErr = NULL;
  
  if (davixPosix->stat(davixParam, url, st, &davixErr) < 0) {
    
    Error("DavixStat", "failed to stat the file with davix: %s (%d)",
            davixErr->getErrMsg().c_str(), davixErr->getStatus());
    DavixError::clearError(&davixErr);
    
    return 0;
  }
  return 1;
}



/////////////////////////////////////////////////////////////////////////////////////////////

//______________________________________________________________________________

TDavixFile::TDavixFile(const char* url, Option_t *opt) : TFile(url, "WEB"),
d_ptr(new TDavixFileInternal(fUrl, opt)) {
  Init(kFALSE);
}

//______________________________________________________________________________

TDavixFile::TDavixFile(TUrl url, Option_t *opt) : TFile(url.GetUrl(), "WEB"),
d_ptr(new TDavixFileInternal(fUrl, opt)) {
  Init(kFALSE);
}

//______________________________________________________________________________

TDavixFile::~TDavixFile() {
  d_ptr->Close();
  delete d_ptr;
}


//______________________________________________________________________________

void TDavixFile::Init(Bool_t init) {
  d_ptr->init();
  TFile::Init(kFALSE);
  fOffset = 0;
  fD = -2; // so TFile::IsOpen() will return true when in TFile::~TFi */
}


//______________________________________________________________________________

void TDavixFile::Seek(Long64_t offset, ERelativeTo pos) {
  // Set position from where to start reading.

  TLockGuard guard(d_ptr->positionLock);
  switch (pos) {
    case kBeg:
      fOffset = offset + fArchiveOffset;
      break;
    case kCur:
      fOffset += offset;
      break;
    case kEnd:
      // this option is not used currently in the ROOT code
      if (fArchiveOffset)
        Error("Seek", "seeking from end in archive is not (yet) supported");
      fOffset = fEND - offset; // is fEND really EOF or logical EOF?
      break;
  }

  if (gDebug > 1)
    Info("Seek", " move cursor to %lld"
          , fOffset);
}

//______________________________________________________________________________

Bool_t TDavixFile::ReadBuffer(char *buf, Int_t len) {
  // Read specified byte range from remote file via HTTP.
  // Returns kTRUE in case of error.
  TLockGuard guard(d_ptr->positionLock);
  Davix_fd *fd;
  if ((fd = d_ptr->getDavixFileInstance()) == NULL)
    return kTRUE;
  Long64_t ret = DavixReadBuffer(fd, buf, len);
  if (ret < 0)
    return kTRUE;

  if (gDebug > 1)
    Info("ReadBuffer", "%lld bytes of data read sequentially"
          " (%d requested)", ret, len);

  return kFALSE;
}

//______________________________________________________________________________

Bool_t TDavixFile::ReadBuffer(char *buf, Long64_t pos, Int_t len) {

  Davix_fd *fd;
  if ((fd = d_ptr->getDavixFileInstance()) == NULL)
    return kTRUE;

  Long64_t ret = DavixPReadBuffer(fd, buf, pos, len);
  if (ret < 0)
    return kTRUE;


  if (gDebug > 1)
    Info("ReadBuffer", "%lld bytes of data read from offset"
          " %lld (%d requested)", ret, pos, len);
  return kFALSE;
}

//______________________________________________________________________________

Bool_t TDavixFile::ReadBuffers(char *buf, Long64_t *pos, Int_t *len, Int_t nbuf) {
  Davix_fd *fd;
  if ((fd = d_ptr->getDavixFileInstance()) == NULL)
    return kTRUE;

  Long64_t ret = DavixReadBuffers(fd, buf, pos, len, nbuf);
  if (ret < 0)
    return kTRUE;

  if (gDebug > 1)
    Info("ReadBuffers", "%lld bytes of data read from a list of %d buffers",
          ret, nbuf);

  return kFALSE;
}

void TDavixFile::setCACheck(Bool_t check) {
  d_ptr->davixParam->setSSLCAcheck((bool)check);
}

void TDavixFile::enableGridMode() {
  d_ptr->enableGridMode();
}

//______________________________________________________________________________

Long64_t TDavixFile::GetSize() const {
  struct stat st;
  Int_t ret = d_ptr->DavixStat(fUrl.GetUrl(), &st);
  if (ret) {
    if (gDebug > 1)
      Info("GetSize", "file size requested:  %ld",
            st.st_size);
    return st.st_size;
  }
  return -1;
}

//______________________________________________________________________________

Double_t TDavixFile::eventStart() {
  if (gPerfStats)
    return TTimeStamp();
  return 0;
}

//______________________________________________________________________________

void TDavixFile::eventStop(Double_t t_start, Long64_t len) {
  // set TFile state info
  fBytesRead += len;
  fReadCalls += 1;

  if (gPerfStats)
    gPerfStats->FileReadEvent(this, (Int_t) len, t_start);
}

Long64_t TDavixFile::DavixReadBuffer(Davix_fd *fd, char *buf, Int_t len) {
  DavixError *davixErr = NULL;
  Double_t start_time = eventStart();

  Long64_t ret = d_ptr->davixPosix->pread(fd, buf, len, fOffset, &davixErr);
  if (ret < 0) {
    Error("DavixReadBuffer", "failed to read data with davix: %s (%d)",
            davixErr->getErrMsg().c_str(), davixErr->getStatus());
    DavixError::clearError(&davixErr);
  } else {
    fOffset += ret;
    eventStop(start_time, ret);
  }

  return ret;
}

Long64_t TDavixFile::DavixPReadBuffer(Davix_fd *fd, char *buf, Long64_t pos, Int_t len) {
  DavixError *davixErr = NULL;
  Double_t start_time = eventStart();

  Long64_t ret = d_ptr->davixPosix->pread(fd, buf, len, pos, &davixErr);
  if (ret < 0) {
    Error("DavixPReadBuffer", "failed to read data with davix: %s (%d)",
            davixErr->getErrMsg().c_str(), davixErr->getStatus());
    DavixError::clearError(&davixErr);
  } else {
    eventStop(start_time, ret);
  }


  return ret;
}

Long64_t TDavixFile::DavixReadBuffers(Davix_fd *fd, char *buf, Long64_t *pos, Int_t *len, Int_t nbuf) {
  DavixError *davixErr = NULL;
  Double_t start_time = eventStart();
  DavIOVecInput in[nbuf];
  DavIOVecOuput out[nbuf];

  int lastPos = 0;
  for (Int_t i = 0; i < nbuf; ++i) {
    in[i].diov_buffer = &buf[lastPos];
    in[i].diov_offset = pos[i];
    in[i].diov_size = len[i];
    lastPos += len[i];
  }

  Long64_t ret = d_ptr->davixPosix->preadVec(fd, in, out, nbuf, &davixErr);
  if (ret < 0) {
    Error("DavixReadBuffers", "failed to read data with davix: %s (%d)",
            davixErr->getErrMsg().c_str(), davixErr->getStatus());
    DavixError::clearError(&davixErr);
  } else {
    eventStop(start_time, ret);
  }

  return ret;
}



