//=======================================================================================
// SkookumScript Unreal Engine Editor Plugin
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Author: Markus Breyer
//=======================================================================================

#include "SkookumScriptEditorPrivatePCH.h"
#include "SkookumScriptEditorCommands.h"

PRAGMA_DISABLE_OPTIMIZATION
void FSkookumScriptEditorCommands::RegisterCommands()
  {
  #define LOCTEXT_NAMESPACE "SkookumScript"

    UI_COMMAND(m_skookum_button, "Skookum IDE", "Show in Skookum IDE", EUserInterfaceActionType::Button, FInputGesture());

  #undef LOCTEXT_NAMESPACE
  }
PRAGMA_ENABLE_OPTIMIZATION
