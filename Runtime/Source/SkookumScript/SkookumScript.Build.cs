// Copyright 2000 Agog Labs Inc., All Rights Reserved.
using System.IO;
using System.Net;
using System.Diagnostics;
using System.Collections.Generic;
using UnrealBuildTool;


public class SkookumScript : ModuleRules
{
  public SkookumScript(TargetInfo Target)
  { 
    // Check if Sk source code is present (Pro-RT license) 
    var bFullSource = File.Exists(Path.Combine(ModuleDirectory, "Private", "SkookumScript", "SkookumScript.cpp"));

    // If full source is present, build module from source, otherwise link with binary library
    Type = bFullSource ? ModuleType.CPlusPlus : ModuleType.External;
    
    var bPlatformAllowed = false;

    List<string> platPathSuffixes = new List<string>();

    string libNameExt = ".a";
    string libNamePrefix = "lib";
    string libNameSuffix = "";
    string dllNameExt = ".dylib";
    bool useDebugCRT = BuildConfiguration.bDebugBuildsActuallyUseDebugCRT;

    switch (Target.Platform)
    {
      case UnrealTargetPlatform.Win32:
        bPlatformAllowed = true;
        platPathSuffixes.Add(Path.Combine("Win32", WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 ? "VS2015" : "VS2013"));
        libNameExt = ".lib";
        dllNameExt = ".dll";
        libNamePrefix = "";
        Definitions.Add("WIN32_LEAN_AND_MEAN");
        break;
      case UnrealTargetPlatform.Win64:
        bPlatformAllowed = true;
        platPathSuffixes.Add(Path.Combine("Win64", WindowsPlatform.Compiler == WindowsCompiler.VisualStudio2015 ? "VS2015" : "VS2013"));
        libNameExt = ".lib";
        dllNameExt = ".dll";
        libNamePrefix = "";
        Definitions.Add("WIN32_LEAN_AND_MEAN");
        break;
      case UnrealTargetPlatform.Mac:
        bPlatformAllowed = true;
        platPathSuffixes.Add("Mac");
        Definitions.Add("A_PLAT_OSX");
        useDebugCRT = true;
        break;
      case UnrealTargetPlatform.IOS:
        bPlatformAllowed = true;
        platPathSuffixes.Add("IOS");
        Definitions.Add("A_PLAT_iOS");
        useDebugCRT = true;
        break;
      case UnrealTargetPlatform.Android:
        bPlatformAllowed = true;
        platPathSuffixes.Add(Path.Combine("Android", "ARM"));
        platPathSuffixes.Add(Path.Combine("Android", "ARM64"));
        platPathSuffixes.Add(Path.Combine("Android", "x86"));
        platPathSuffixes.Add(Path.Combine("Android", "x64"));
        Definitions.Add("A_PLAT_ANDROID");
        useDebugCRT = true;
        break;
    }

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

    // Public include paths
    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

    // Public dependencies
    PublicDependencyModuleNames.Add("AgogCore");

    if (bFullSource)
    {
      // We're building SkookumScript from source - not much else needed
      PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));
      // Build system wants us to be dependent on some module with precompiled headers, so be it
      PrivateDependencyModuleNames.Add("Core");
    }
    else if (bPlatformAllowed)
    {
      var buildNumber = "2234";
      var moduleName = "SkookumScript";

      // Get local file path where the library is located
      var bIsMonolithic = (!Target.bIsMonolithic.HasValue || (bool)Target.bIsMonolithic); // Assume monolithic if not specified
      var libFileNameStem = bIsMonolithic 
        ? libNamePrefix + moduleName + libNameSuffix 
        : "UE4Editor-" + moduleName + (Target.Configuration == UnrealTargetConfiguration.Debug ? "-" + Target.Platform.ToString() + "-Debug" : "");
      var libFileName = libFileNameStem + libNameExt;
      var libDirPathBase = Path.Combine(ModuleDirectory, "..", "..", "Intermediate", "Lib", buildNumber);
      // Check for library in download location
      foreach (var platPathSuffix in platPathSuffixes)
      {
        var libDirPath = Path.Combine(libDirPathBase, platPathSuffix);
        var libFilePath = Path.Combine(libDirPath, libFileName);
        if (!File.Exists(libFilePath))
        {
          // Does not exist, try to download it
          if (!File.Exists(libDirPath))
          {
            Directory.CreateDirectory(libDirPath);
          }

          var libUrlBase = ("http://download.skookumscript.com/beta/" + buildNumber + "/lib/" + platPathSuffix + "/").Replace('\\', '/');
          WebClient client = new WebClient();

          // First get DLL if needed as it is most likely to fail (if UE4Editor is up and running)
          if (!bIsMonolithic)
          {
            var dllDirPath = Path.Combine(ModuleDirectory, "..", "..", "Binaries", Target.Platform.ToString());
            var dllFileName = libFileNameStem + dllNameExt;
            var dllFilePath = Path.Combine(dllDirPath, dllFileName);
            var dllUrl = libUrlBase + dllFileName;
            try
            {
              if(!Directory.Exists(dllDirPath))
              {
                Directory.CreateDirectory(dllDirPath);
              }
              
              Log.TraceInformation("Downloading build {0} of {1}...", buildNumber, dllFileName);
              var tmpFilePath = dllFilePath + ".tmp";
              client.DownloadFile(dllUrl, @tmpFilePath);
              if (File.Exists(dllFilePath)) File.Delete(dllFilePath);
              File.Move(tmpFilePath, dllFilePath);
              Log.TraceInformation("Success!");
            }
            catch (System.Exception)
            {
              if (File.Exists(dllFilePath)) File.Delete(dllFilePath);
              throw new BuildException("Could not download {0}!", dllUrl);
            }
          }

          // Then get static library
          var libUrl = libUrlBase + libFileName;
          try
          {
            Log.TraceInformation("Downloading build {0} of {1}...", buildNumber, platPathSuffix.Replace('\\', '/') + "/" + libFileName);
            client.DownloadFile(libUrl, @libFilePath);
            Log.TraceInformation("Success!");
          }
          catch (System.Exception)
          {
            if (File.Exists(libFilePath)) File.Delete(libFilePath);
            throw new BuildException("Could not download {0}!", libUrl);
          }
        }

        PublicLibraryPaths.Add(libDirPath);

        // For non-Android, add full path
        if (Target.Platform != UnrealTargetPlatform.Android)
        {
          PublicAdditionalLibraries.Add(libFilePath);
        }

        Log.TraceVerbose("{0} library found at: {1}", moduleName, libFilePath);
      }

      // For Android, just add core of library name, e.g. "SkookumScript-Development"
      if (Target.Platform == UnrealTargetPlatform.Android)
      {
        PublicAdditionalLibraries.Add(moduleName + libNameSuffix);
      }

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
            if (!proc.CloseMainWindow()) throw (new System.Exception());
            if (!proc.WaitForExit(5000)) throw (new System.Exception());
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
