//=======================================================================================
// SkookumScript Unreal Engine Editor Plugin
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Author: Markus Breyer
//=======================================================================================

#pragma once

//---------------------------------------------------------------------------------------

class FSkookumStyles
  {
  public:
    // Initializes the value of MenuStyleInstance and registers it with the Slate Style Registry.
    static void Initialize();

    // Unregisters the Slate Style Set and then resets the MenuStyleInstance pointer.
    static void Shutdown();

    // Retrieves a reference to the Slate Style pointed to by MenuStyleInstance.
    static const class ISlateStyle& Get();

    // Retrieves the name of the Style Set.
    static FName GetStyleSetName();

  private:

    // Singleton instance used for our Style Set.
    static TSharedPtr<class FSlateStyleSet> ms_singleton_p;

  };

