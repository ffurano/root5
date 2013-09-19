void P045_TDavixSystem()
{
   gPluginMgr->AddHandler("TSystem", "^http:", "TDavixSystem",
      "DAVIX", "TDavixSystem()");

   gPluginMgr->AddHandler("TSystem", "^dav:", "TDavixSystem",
      "DAVIX", "TDavixSystem()");

   gPluginMgr->AddHandler("TSystem", "^davs:", "TDavixSystem",
      "DAVIX", "TDavixSystem()");

   gPluginMgr->AddHandler("TSystem", "^https:", "TDavixSystem",
      "DAVIX", "TDavixSystem()");

   gPluginMgr->AddHandler("TSystem", "^s3:", "TDavixSystem",
      "DAVIX", "TDavixSystem()");

   gPluginMgr->AddHandler("TSystem", "^s3s:", "TDavixSystem",
      "DAVIX", "TDavixSystem()");
}
