//=======================================================================================
// SkookumScript Unreal Engine Editor Plugin
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Author: Markus Breyer
//=======================================================================================

#pragma once

#include "Commands.h"

class FSkookumScriptEditorCommands : public TCommands<FSkookumScriptEditorCommands>
  {
  public:

    FSkookumScriptEditorCommands()
      : TCommands<FSkookumScriptEditorCommands>(TEXT("SkookumScript"), FText::FromString(TEXT("SkookumScript")), NAME_None, FEditorStyle::GetStyleSetName())
      {
      }

    virtual void RegisterCommands() override;

    TSharedPtr<FUICommandInfo> m_skookum_button;

  };