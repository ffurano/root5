
#ifndef TDAVIXSYSTEM_H
#define	TDAVIXSYSTEM_H


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



#include "TUrl.h"
#include "TSystem.h"
#include "TMutex.h"


class TDavixFileInternal;
struct Davix_fd;


class TDavixSystem : public TSystem {
private:
  TDavixFileInternal* d_ptr;
  
public:

  TDavixSystem();
  TDavixSystem(const char *url);

  virtual ~TDavixSystem();
  
  virtual void FreeDirectory(void *dirp);
  virtual const char *GetDirEntry(void *dirp);

  virtual Int_t GetPathInfo(const char* path, FileStat_t &buf);
  virtual Bool_t IsPathLocal(const char *path);
  virtual Int_t Locate(const char* path, TString &endurl);
  virtual Int_t MakeDirectory(const char* dir);
  virtual void *OpenDirectory(const char* dir);
  virtual int Unlink(const char *path);

  ClassDef(TDavixSystem, 0);
};

#endif	/* TDAVIXSYSTEM_H */

