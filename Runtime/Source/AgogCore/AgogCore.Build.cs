// Copyright 2000 Agog Labs Inc., All Rights Reserved.
using System.IO;
using System.Net;
using UnrealBuildTool;


public class AgogCore : ModuleRules
{
  public AgogCore(TargetInfo Target)
  {  
    Type = ModuleType.External;
    
    var bPlatformAllowed = false;
    
    string platPathSuffix = Target.Platform.ToString();
    string libPathExt = ".a";
    bool useDebugCRT = BuildConfiguration.bDebugBuildsActuallyUseDebugCRT;
    
    switch (Target.Platform)
    {
    case UnrealTargetPlatform.Win32:
      bPlatformAllowed = true;
      platPathSuffix = Path.Combine("Win32", "VS2013");
      libPathExt = ".lib";
      Definitions.Add("WIN32_LEAN_AND_MEAN");
      break;
    case UnrealTargetPlatform.Win64:
      bPlatformAllowed = true;
      platPathSuffix = Path.Combine("Win64", "VS2013");
      libPathExt = ".lib";
      Definitions.Add("WIN32_LEAN_AND_MEAN");
      break;
    case UnrealTargetPlatform.IOS:
      bPlatformAllowed = true;
      Definitions.Add("A_PLAT_iOS");
      useDebugCRT = true;
      break;
    }

    string libNameSuffix = "";   

    // NOTE: All modules inside the SkookumScript plugin folder must use the exact same definitions!
    switch (Target.Configuration)
    {
    case UnrealTargetConfiguration.Debug:
    case UnrealTargetConfiguration.DebugGame:
      Definitions.Add("A_EXTRA_CHECK=1");
      Definitions.Add("A_UNOPTIMIZED=1");
      Definitions.Add("SKOOKUM=31");
      libNameSuffix = useDebugCRT ? "-Debug" : "-DebugCRTOpt";
      break;

    case UnrealTargetConfiguration.Development:
    case UnrealTargetConfiguration.Test:
      Definitions.Add("A_EXTRA_CHECK=1");
      Definitions.Add("SKOOKUM=31");
      libNameSuffix = "-Development";
      break;

    case UnrealTargetConfiguration.Shipping:
      Definitions.Add("A_SYMBOL_STR_DB=1");
      Definitions.Add("A_NO_SYMBOL_REF_LINK=1");
      Definitions.Add("SKOOKUM=8");
      libNameSuffix = "-Shipping";
      break;
    }
    
    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

    if (bPlatformAllowed)
    {
      var buildNumber = "1784";
      var moduleName = "AgogCore";

      // Get local file path where the library is located
      var libFileName = moduleName + libNameSuffix + libPathExt;
      var libDirPath = Path.Combine(ModuleDirectory, "..", "..", "Intermediate", "Lib", buildNumber, platPathSuffix);
      var libFilePath = Path.Combine(libDirPath, libFileName);
      if (!File.Exists(libFilePath))
      {
        // Does not exist, try to download it
        if (!File.Exists(libDirPath))
        {
          Directory.CreateDirectory(libDirPath);
        }
        var libUrl = ("http://download.skookumscript.com/beta/" + buildNumber + "/lib/" + platPathSuffix + "/" + libFileName).Replace('\\', '/');
        WebClient client = new WebClient();
        try
        {
          Log.TraceInformation("Downloading build {0} of {1}...", buildNumber, libFileName);
          client.DownloadFile(libUrl, @libFilePath);
          Log.TraceInformation("Success!");
        }
        catch (System.Exception)
        {
          if (File.Exists(libFilePath)) File.Delete(libFilePath);
          Log.TraceInformation("Could not download {0}!", libUrl);
        }
      }
      // Check if a newer custom built library exists that we want to use instead
      var builtLibDirPath = Path.Combine(ModuleDirectory, "Lib", platPathSuffix);
      var builtLibFilePath = Path.Combine(builtLibDirPath, libFileName);
      if (File.Exists(builtLibFilePath) && (!File.Exists(libFilePath) || (File.GetLastWriteTime(builtLibFilePath) > File.GetLastWriteTime(libFilePath))))
      {
        Log.TraceInformation("Using locally built AgogCore.");
        libDirPath = builtLibDirPath;
        libFilePath = builtLibFilePath;
      }
      PublicLibraryPaths.Add(libDirPath);
      PublicAdditionalLibraries.Add(libFilePath);
      Log.TraceVerbose("{0} library added to path: {1}", moduleName, libDirPath);
    }
  }    
}
