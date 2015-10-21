//=======================================================================================
// SkookumScript Unreal Engine Editor Plugin
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Author: Markus Breyer
//=======================================================================================

#include "SkookumScriptEditorPrivatePCH.h"
#include "SkookumStyles.h"

#define IMAGE_BRUSH( RelativePath, ... )  FSlateImageBrush(ms_singleton_p->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH( RelativePath, ... )    FSlateBoxBrush(ms_singleton_p->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH( RelativePath, ... ) FSlateBorderBrush(ms_singleton_p->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT( RelativePath, ... )     FSlateFontInfo(ms_singleton_p->RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT( RelativePath, ... )     FSlateFontInfo(ms_singleton_p->RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)

TSharedPtr<FSlateStyleSet> FSkookumStyles::ms_singleton_p = NULL;

void FSkookumStyles::Initialize()
  {
  if (!ms_singleton_p.IsValid())
    {
    const FVector2D Icon20x20(20.0f, 20.0f);
    const FVector2D Icon40x40(40.0f, 40.0f);

    FString content_path = FPaths::EnginePluginsDir() / TEXT("SkookumScript/Runtime/Content/UI");
    ms_singleton_p = FSlateGameResources::New(FSkookumStyles::GetStyleSetName(), content_path, content_path);

    // toolbar icons
    ms_singleton_p->Set("SkookumScriptEditor.ShowIDE", new IMAGE_BRUSH("icon_ide_40x", Icon40x40));
    ms_singleton_p->Set("SkookumScriptEditor.ShowIDE.Small", new IMAGE_BRUSH("icon_ide_40x", Icon20x20));

    FSlateStyleRegistry::RegisterSlateStyle(*ms_singleton_p);
    }
  }

void FSkookumStyles::Shutdown()
  {
  FSlateStyleRegistry::UnRegisterSlateStyle(*ms_singleton_p);
  ensure(ms_singleton_p.IsUnique());
  ms_singleton_p.Reset();
  }

FName FSkookumStyles::GetStyleSetName()
  {
  static FName StyleSetName(TEXT("SkookumStyles"));
  return StyleSetName;
  }

const ISlateStyle& FSkookumStyles::Get()
  {
  return *ms_singleton_p;
  }

