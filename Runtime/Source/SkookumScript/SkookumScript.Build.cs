// Copyright 2000 Agog Labs Inc., All Rights Reserved.
using System.IO;
using System.Net;
using System.Diagnostics;
using UnrealBuildTool;


public class SkookumScript : ModuleRules
{
  public SkookumScript(TargetInfo Target)
  {  
    Type = ModuleType.External;
    
    var bPlatformAllowed = false;
    
    string platPathSuffix = Target.Platform.ToString();
    string libPathExt = ".a";
    string libNamePrefix = "lib";
    bool useDebugCRT = BuildConfiguration.bDebugBuildsActuallyUseDebugCRT;
    
    switch (Target.Platform)
    {
    case UnrealTargetPlatform.Win32:
      bPlatformAllowed = true;
      platPathSuffix = Path.Combine("Win32", WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 ? "VS2015" : "VS2013");
      libPathExt = ".lib";
      libNamePrefix = "";
      Definitions.Add("WIN32_LEAN_AND_MEAN");
      break;
    case UnrealTargetPlatform.Win64:
      bPlatformAllowed = true;
      platPathSuffix = Path.Combine("Win64", WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 ? "VS2015" : "VS2013");
      libPathExt = ".lib";
      libNamePrefix = "";
      Definitions.Add("WIN32_LEAN_AND_MEAN");
      break;
    case UnrealTargetPlatform.Mac:
      bPlatformAllowed = true;
      Definitions.Add("A_PLAT_OSX");
      useDebugCRT = true;
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
      var buildNumber = "1956";
      var moduleName = "SkookumScript";

      // Get local file path where the library is located
      var libFileName = libNamePrefix + moduleName + libNameSuffix + libPathExt;
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
      if (File.Exists(builtLibFilePath))
      {
        // There might be leftovers from ancient versions of the plugin - make sure those are not used
        if (File.GetLastWriteTime(builtLibFilePath) > new System.DateTime(2015,11,1))
        {
          Log.TraceInformation("Using locally built SkookumScript.");
          libDirPath = builtLibDirPath;
          libFilePath = builtLibFilePath;
        }
        else
        {
          Log.TraceInformation("Using downloaded SkookumScript.");
        }
      }
      PublicLibraryPaths.Add(libDirPath);
      PublicAdditionalLibraries.Add(libFilePath);
      Log.TraceVerbose("{0} library added to path: {1}", moduleName, libDirPath);

      // Also make sure we got the latest IDE
      var ideDirPath = Path.Combine(ModuleDirectory, "..", "..", "SkookumIDE");
      var versionFilePath = Path.Combine(ideDirPath, buildNumber + ".version");
      // Got correct IDE?
      if (!File.Exists(versionFilePath))
      {
        // No, download from the internets
        var ideUrl = "http://download.skookumscript.com/beta/" + buildNumber + "/bin/SkookumIDE.exe";
        var tmpFilePath = Path.Combine(ideDirPath, "SkookumIDE.tmp");
        WebClient client = new WebClient();
        try
        {
          Log.TraceInformation("Downloading build {0} of Skookum IDE...", buildNumber);
          client.DownloadFile(ideUrl, @tmpFilePath);
        }
        catch (System.Exception)
        {
          if (File.Exists(tmpFilePath)) File.Delete(tmpFilePath);
          throw new BuildException("Could not download {0}!", ideUrl);
        }
        try
        {
          // Check if IDE is running and if so, terminate it
          Process[] processes = Process.GetProcessesByName("SkookumIDE");
          if (processes.Length > 0)
          {
            Log.TraceInformation("Skookum IDE currently running - attempting to terminate so executable can be updated..."); 
          }
          foreach (Process proc in processes)
          {
            if (!proc.CloseMainWindow()) throw(new System.Exception());
            if (!proc.WaitForExit(5000)) throw(new System.Exception());
          }
          Log.TraceInformation("Replacing Skookum IDE executable with updated version...");
          // Delete old IDE executable and replace with new one
          var ideFilePath = Path.Combine(ideDirPath, "SkookumIDE.exe");
          File.Delete(ideFilePath);
          File.Move(tmpFilePath, ideFilePath);
          // Delete previous version file if any
          var oldVersionFilePaths = Directory.EnumerateFiles(ideDirPath, "*.version");
          foreach (string oldVersionFilePath in oldVersionFilePaths)
          {
            File.Delete(oldVersionFilePath);
          }
          // Create new (empty) version file to indicate we have that version now
          FileStream stream = File.Create(versionFilePath);
          stream.Close();
          // Eureka!
          Log.TraceInformation("Success - enjoy the latest greatest Skookum IDE!");
        }
        catch (System.Exception)
        {
          if (File.Exists(tmpFilePath)) File.Delete(tmpFilePath);
          if (File.Exists(versionFilePath)) File.Delete(versionFilePath);
          throw new BuildException("Could not update Skookum IDE executable! Make sure no Skookum IDE is currently running then try building again.");
        }
      }
    }
  }    
}
