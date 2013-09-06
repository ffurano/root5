void P130_TDavixFile()
{
   gPluginMgr->AddHandler("TFile", "^http[s]?:", "TDavixFile",
      "DAVIX", "TDavixFile(const char*,Option_t*)");
}
