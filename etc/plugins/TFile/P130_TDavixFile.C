void P130_TDavixFile()
{
   gPluginMgr->AddHandler("TFile", "^http[s]?:", "TDavixFile",
      "DAVIX", "TDavixFile(const char*,Option_t*)");

   gPluginMgr->AddHandler("TFile", "^dav[s]?:", "TDavixFile",
      "DAVIX", "TDavixFile(const char*,Option_t*)");

   gPluginMgr->AddHandler("TFile", "^s3[s]?:", "TDavixFile",
      "DAVIX", "TDavixFile(const char*,Option_t*)");
}
