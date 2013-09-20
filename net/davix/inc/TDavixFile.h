#ifndef ROOT_TDavixFile
#define ROOT_TDavixFile




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
// Checks, refactoring and ROOT5 porting:                                            //
//              Fabrizio Furano (CERN IT/SDC)                           //
//                                                                      //
// September 2013                                                       //
//                                                                      //
//////////////////////////////////////////////////////////////////////////

//
// Parameters that influence the behavior of TDavixFile/TDavixSystem. The names should be self-explanatory
//
//Davix.Debug
//Davix.GSI.UserProxy
//Davix.GSI.UserCert
//Davix.GSI.UserKey

//Davix.GSI.CAdir
//Davix.GSI.CACheck
//Davix.GSI.GridMode
//
// Environment variables:
// X509_USER_CERT, X509_USER_KEY, X509_USER_PROXY ... usual meaning for the X509 Grid things. gEnv vars have higher priority.

#include "TFile.h"
#include "TUrl.h"
#include "TSystem.h"
#include "TMutex.h"

class TDavixFileInternal;
struct Davix_fd;





class TDavixFile : public TFile {
private:
    TDavixFileInternal* d_ptr;

    void Init(Bool_t init);
    Long64_t DavixReadBuffer(Davix_fd *fd, char *buf, Int_t len);
    Long64_t DavixPReadBuffer(Davix_fd *fd, char *buf, Long64_t pos, Int_t len);
    Long64_t DavixReadBuffers(Davix_fd *fd, char *buf, Long64_t *pos, Int_t *len, Int_t nbuf);
    Int_t DavixStat(struct stat *st) const;

    // perfStats
    Double_t eventStart();
    void eventStop(Double_t t, Long64_t len);

public:
    ///
    ///  Open function for TDavixFile
    ///
    /// TDavixFile supports several options :
    ///
    ///  - GRID_MODE=yes    : enable the grid authentication and CA support
    ///  - CA_CHECK=no      : remove all the certificate authority check, this option can create a security vulnerability
    ///  - S3_SECKEY=string : Amazon S3 secret token
    ///  - S3_ACCKEY=string : Amazon S3 access token
    ///
    /// Several parameters can be used if separated with whitespace
    ////
    TDavixFile(TUrl url, Option_t *opt = "");
    TDavixFile(const char* url, Option_t *opt = "");
    
    ~TDavixFile();

    // TFile interface.
    virtual Long64_t GetSize() const;
    virtual void  Seek(Long64_t offset, ERelativeTo pos = kBeg);
    virtual Bool_t ReadBuffer(char *buf, Int_t len);
    virtual Bool_t ReadBuffer(char *buf, Long64_t pos, Int_t len);
    virtual Bool_t ReadBuffers(char *buf, Long64_t *pos, Int_t *len, Int_t nbuf);

    // TDavixFile options
    /// Enable or disable certificate authority check
    void setCACheck(Bool_t check);

    /// Enable the grid mode
    /// The grid Mode configure automatically all grid-CA path, VOMS authentication
    /// and grid related extension for a grid analysis usage
    void enableGridMode();

    ClassDef(TDavixFile, 0)
};



#endif
