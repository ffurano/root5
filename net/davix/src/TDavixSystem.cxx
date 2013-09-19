

//////////////////////////////////////////////////////////////////////////
//                                                                      //
// TDavixSystem                                                         //
//                                                                      //
// A TSystem specialization for HTTP and WebDAV                         //
// It supports HTTP and HTTPS in a number of dialects and options       //
//  e.g. S3 is one of them                                              //
// Other caracteristics come from the full support of Davix,            //
//  e.g. full redirection support in any circumstance                   //
//                                                                      //
// Authors:     Adrien Devresse (CERN IT/SDC)                           //
//              Fabrizio Furano (CERN IT/SDC)                          //
//                                                                      //
// September 2013                                                       //
//                                                                      //
//////////////////////////////////////////////////////////////////////////



#include "TDavixSystem.h"
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


extern const std::string VERSION;
extern const std::string gUserAgent;

// The prefix that is used to find the variables in the gEnv
#define ENVPFX	"Davix."

ClassImp(TDavixSystem)

using namespace Davix;

extern const char* grid_mode_opt;
extern const char* ca_check_opt;
extern const char* s3_seckey_opt;
extern const char* s3_acckey_opt;

TDavixSystem::TDavixSystem(const char *url) : TSystem(url) {

  TUrl u(url);
  d_ptr = new TDavixFileInternal(u, "WEB");
  d_ptr->init();
  SetTitle("WebDAV system administration");
}

TDavixSystem::~TDavixSystem() {
  SafeDelete(d_ptr);
}

void TDavixSystem::FreeDirectory(void *dirp) {

}

const char *TDavixSystem::GetDirEntry(void *dirp) {

  return NULL;
}

void *TDavixSystem::OpenDirectory(const char* dir) {

  return NULL;
}

Int_t TDavixSystem::GetPathInfo(const char* path, FileStat_t &buf) {
  DavixError *davixErr = NULL;
  struct stat st;

  if (!d_ptr->DavixStat(path, &st)) return 1;


  buf.fDev = 0;
  buf.fIno = 0;
  buf.fMode = st.st_mode; // protection (combination of EFileModeMask bits)
    
  
  buf.fUid = st.st_uid; // user id of owner
  buf.fGid = st.st_gid; // group id of owner
  buf.fSize = st.st_size; // total size in bytes
  buf.fMtime = st.st_mtime; // modification date
  buf.fIsLink = kFALSE; // symbolic link
  buf.fUrl = path; // end point url of file
  
  return 0;
}

Bool_t TDavixSystem::IsPathLocal(const char *path) {
  return kFALSE;
}

Int_t TDavixSystem::Locate(const char* path, TString &endurl) {
 return 0;
}

Int_t TDavixSystem::MakeDirectory(const char* dir) {
 return 0;
}

int TDavixSystem::Unlink(const char *path) {
 return 0;
}

