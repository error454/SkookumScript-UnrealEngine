// Copyright 2000 Agog Labs Inc., All Rights Reserved.
using System.IO;
using System.Net;
using UnrealBuildTool;


public class AgogCore : ModuleRules
{
  public AgogCore(TargetInfo Target)
  {
    switch (Target.Platform)
    {
    case UnrealTargetPlatform.Win32:
      Definitions.Add("WIN32_LEAN_AND_MEAN");
      break;
    case UnrealTargetPlatform.Win64:
      Definitions.Add("WIN32_LEAN_AND_MEAN");
      break;
    case UnrealTargetPlatform.Mac:
      Definitions.Add("A_PLAT_OSX");
      break;
    case UnrealTargetPlatform.IOS:
      Definitions.Add("A_PLAT_iOS");
      break;
    case UnrealTargetPlatform.Android:
      Definitions.Add("A_PLAT_ANDROID");
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
      break;

    case UnrealTargetConfiguration.Development:
    case UnrealTargetConfiguration.Test:
      Definitions.Add("A_EXTRA_CHECK=1");
      Definitions.Add("SKOOKUM=31");
      break;

    case UnrealTargetConfiguration.Shipping:
      Definitions.Add("A_SYMBOL_STR_DB=1");
      Definitions.Add("A_NO_SYMBOL_REF_LINK=1");
      Definitions.Add("SKOOKUM=8");
      break;
    }
    
    PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
    PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

    // Build system wants us to be dependent on some module with precompiled headers, so be it
    PrivateDependencyModuleNames.Add("Core");
  }
}
