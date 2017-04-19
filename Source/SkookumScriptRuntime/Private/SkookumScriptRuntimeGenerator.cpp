//=======================================================================================
// Copyright (c) 2001-2017 Agog Labs Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//=======================================================================================

//=======================================================================================
// SkookumScript Unreal Engine Runtime Script Generator
//=======================================================================================

#include "SkookumScriptRuntimeGenerator.h"
#include "../../SkookumScriptGenerator/Private/SkookumScriptGeneratorBase.inl" // Need this even in cooked builds

#if WITH_EDITORONLY_DATA

#include "Settings/ProjectPackagingSettings.h"
#include "Interfaces/IPluginManager.h"

//=======================================================================================

//---------------------------------------------------------------------------------------

FSkookumScriptRuntimeGenerator::FSkookumScriptRuntimeGenerator(ISkookumScriptRuntimeInterface * runtime_interface_p)
  : m_runtime_interface_p(runtime_interface_p)
  , m_project_mode(SkProjectMode_read_only)
  {
  // Reset super classes
  m_used_classes.Empty();

  // Set up project and overlay paths
  initialize_paths();
  }

//---------------------------------------------------------------------------------------

FSkookumScriptRuntimeGenerator::~FSkookumScriptRuntimeGenerator()
  {
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptRuntimeGenerator::get_project_file_path()
  {
  return m_project_file_path;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptRuntimeGenerator::get_default_project_file_path()
  {
  return m_default_project_file_path;
  }

//---------------------------------------------------------------------------------------

int32 FSkookumScriptRuntimeGenerator::get_overlay_path_depth() const
  {
  return m_overlay_path_depth;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntimeGenerator::delete_all_class_script_files()
  {
  // Clear contents of overlay scripts folder
  FString directory_to_delete(m_overlay_path / TEXT("Object"));
  IFileManager::Get().DeleteDirectory(*directory_to_delete, false, true);
  IFileManager::Get().MakeDirectory(*directory_to_delete, true);

  // Reset super classes
  m_used_classes.Empty();
  }

//---------------------------------------------------------------------------------------
// Generate SkookumScript class script files for all known blueprint assets
void FSkookumScriptRuntimeGenerator::generate_all_class_script_files()
  {
  if (!m_overlay_path.IsEmpty())
    {
    TArray<UObject*> blueprint_array;
    GetObjectsOfClass(UBlueprint::StaticClass(), blueprint_array, false, RF_ClassDefaultObject);
    for (UObject * obj_p : blueprint_array)
      {
      UClass * ue_class_p = static_cast<UBlueprint *>(obj_p)->GeneratedClass;
      if (ue_class_p)
        {
        generate_class_script_files(ue_class_p, true, true, true);
        }
      }

    generate_used_class_script_files();
    }
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptRuntimeGenerator::make_project_editable()
  {
  FString error_msg;

  FString game_name(FApp::GetGameName());
  if (game_name.IsEmpty())
    {
    error_msg = TEXT("Tried to make project editable but engine has no project loaded!");
    }
  else
    {
    // Check if maybe already editable - if so, silently do nothing
    FString editable_scripts_path = FPaths::GameDir() / TEXT("Scripts");
    FString editable_project_file_path(editable_scripts_path / TEXT("Skookum-project.ini"));
    if (FPaths::FileExists(editable_project_file_path))
      {
      // Found editable project - make sure mode is set properly
      m_project_mode = SkProjectMode_editable;
      }
    else
      {
      // No editable project found - check temporary location (in `Intermediate` folder)
      FString temp_root_path(FPaths::GameIntermediateDir() / TEXT("SkookumScript"));
      FString temp_scripts_path(temp_root_path / TEXT("Scripts"));
      FString temp_project_file_path = temp_scripts_path / TEXT("Skookum-project.ini");
      if (!FPaths::FileExists(temp_project_file_path))
        {
        error_msg = TEXT("Tried to make project editable but neither an editable nor a non-editable project was found!");
        }
      else
        {
        if (!IFileManager::Get().Move(*editable_scripts_path, *temp_scripts_path, true, true))
          {
          error_msg = TEXT("Failed moving project information from temporary to editable location!");
          }
        else
          {
          // Move compiled binaries for convenience
          // We don't care if this succeeds
          FString temp_binary_folder_path = temp_root_path / TEXT("Content/SkookumScript");
          FString editable_binary_folder_path = FPaths::GameDir() / TEXT("Content/SkookumScript");
          IFileManager::Get().Move(*editable_binary_folder_path, *temp_binary_folder_path, true, true);

          // Change project packaging settings to include Sk binaries
          UProjectPackagingSettings * packaging_settings_p = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());
          const TCHAR * binary_path_name_p = TEXT("SkookumScript");
          for (TArray<FDirectoryPath>::TConstIterator dir_path(packaging_settings_p->DirectoriesToAlwaysStageAsUFS); dir_path; ++dir_path)
            {
            if (dir_path->Path == binary_path_name_p)
              {
              binary_path_name_p = nullptr;
              break;
              }
            }
          if (binary_path_name_p)
            {
            FDirectoryPath binary_path;
            binary_path.Path = binary_path_name_p;
            packaging_settings_p->DirectoriesToAlwaysStageAsUFS.Add(binary_path);
            FString config_file_name = FPaths::GameConfigDir() / TEXT("DefaultGame.ini");
            m_runtime_interface_p->check_out_file(config_file_name);
            packaging_settings_p->SaveConfig(CPF_Config, *config_file_name);
            }

          // Create Project overlay folder
          IFileManager::Get().MakeDirectory(*(editable_scripts_path / TEXT("Project/Object")), true);

          // Change project to be editable
          FString proj_ini;
          verify(FFileHelper::LoadFileToString(proj_ini, *editable_project_file_path));
          proj_ini = proj_ini.Replace(ms_editable_ini_settings_p, TEXT("")); // Remove editable settings
          proj_ini += TEXT("Overlay8=Project|Project\r\n"); // Create Project overlay definition
          verify(FFileHelper::SaveStringToFile(proj_ini, *editable_project_file_path, FFileHelper::EEncodingOptions::ForceAnsi));

          // Now in new mode
          m_project_mode = SkProjectMode_editable;
          // Remember new project path
          m_project_file_path = FPaths::ConvertRelativePathToFull(editable_project_file_path);
          // Also update overlay path and depth
          set_overlay_path();
          }
        }
      }
    }

  return error_msg;
  }

//---------------------------------------------------------------------------------------
// Attempt to load blueprint with given qualified class path
UBlueprint * FSkookumScriptRuntimeGenerator::load_blueprint_asset(const FString & class_path, bool * sk_class_deleted_p)
  {
  // Try to extract asset path from meta file of Sk class
  FString full_class_path = m_overlay_path / class_path;
  FString meta_file_path = full_class_path / TEXT("!Class.sk-meta");
  FString meta_file_text;
  *sk_class_deleted_p = false;
  if (FFileHelper::LoadFileToString(meta_file_text, *meta_file_path))
    {
    // Found meta file - try to extract asset path contained in it
    int32 package_path_begin_pos = meta_file_text.Find(ms_package_name_key);

    // Temporary clean-up hack (2016-06-19): We only support Game assets now, so if not a game asset, it's an old script file lingering around
    if (package_path_begin_pos < 0 || meta_file_text.Mid(package_path_begin_pos + ms_package_name_key.Len(), 6) != TEXT("/Game/"))
      {
      // If it has a path and it's not "/Game/" then delete it and pretend it never existed
      if (package_path_begin_pos >= 0)
        {
        IFileManager::Get().DeleteDirectory(*full_class_path, false, true);
        }
      return nullptr;
      }

    if (package_path_begin_pos >= 0)
      {
      package_path_begin_pos += ms_package_name_key.Len();
      int32 package_path_end_pos = meta_file_text.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, package_path_begin_pos);
      if (package_path_end_pos > package_path_begin_pos)
        {
        // Successfully got the path of the package, so assemble with asset name and load the asset
        FString package_path = meta_file_text.Mid(package_path_begin_pos, package_path_end_pos - package_path_begin_pos);

        // Get Sk class name
        FString class_name = FPaths::GetCleanFilename(class_path);
        // If there's a dot in the class name, use portion right of it
        int dot_pos = -1;
        if (class_name.FindChar(TCHAR('.'), dot_pos))
          {
          class_name = class_name.Mid(dot_pos + 1);
          }
        // Default asset name is the Sk class name
        FString asset_name = class_name;
        // Now try to extract exact asset name from comment, as it may differ in punctuation
        int32 asset_name_begin_pos = meta_file_text.Find(ms_asset_name_key);
        if (asset_name_begin_pos >= 0)
          {
          asset_name_begin_pos += ms_asset_name_key.Len();
          int32 asset_name_end_pos = meta_file_text.Find(TEXT("\r\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, asset_name_begin_pos);
          if (asset_name_end_pos <= asset_name_begin_pos)
            {
            asset_name_end_pos = meta_file_text.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, asset_name_begin_pos);
            }
          if (asset_name_end_pos > asset_name_begin_pos)
            {
            asset_name = meta_file_text.Mid(asset_name_begin_pos, asset_name_end_pos - asset_name_begin_pos);
            }
          }

        // Now try to find the Blueprint
        FString asset_path = package_path + TEXT(".") + asset_name;
        UBlueprint * blueprint_p = LoadObject<UBlueprint>(nullptr, *asset_path);
        if (!blueprint_p)
          {
          // Asset not found, ask the user what to do
          FText title = FText::Format(FText::FromString(TEXT("Asset Not Found For {0}")), FText::FromString(class_name));
          if (FMessageDialog::Open(
            EAppMsgType::YesNo,
            FText::Format(FText::FromString(
              TEXT("Cannot find Blueprint asset belonging to SkookumScript class '{0}'. ")
              TEXT("It was originally generated from the asset '{1}' but this asset appears to no longer exist. ")
              TEXT("Maybe it was deleted or renamed. ")
              TEXT("If you no longer need the SkookumScript class '{0}', you can fix this issue by deleting the class. ")
              TEXT("Would you like to delete the SkookumScript class '{0}'?")), FText::FromString(class_name), FText::FromString(asset_path)),
            &title) == EAppReturnType::Yes)
            {
            // User requested deletion, so nuke it
            IFileManager::Get().DeleteDirectory(*full_class_path, false, true);
            *sk_class_deleted_p = true;
            m_runtime_interface_p->on_class_scripts_changed_by_generator(class_name, ISkookumScriptRuntimeInterface::ChangeType_deleted);
            }
          }
        return blueprint_p;
        }
      }
    }

  return nullptr;
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptRuntimeGenerator::can_export_property(UProperty * var_p, int32 include_priority, uint32 referenced_flags)
  {
  if (!is_property_type_supported(var_p))
    {
    return false;
    }

  // Accept all known static object types plus all generated by Blueprints
  UObjectPropertyBase * object_var_p = Cast<UObjectPropertyBase>(var_p);
  if (object_var_p && (!object_var_p->PropertyClass || (!m_runtime_interface_p->is_static_class_known_to_skookum(object_var_p->PropertyClass) && !UBlueprint::GetBlueprintFromClass(object_var_p->PropertyClass))))
    {
    return false;
    }

  // Accept all known static struct types
  UStructProperty * struct_var_p = Cast<UStructProperty>(var_p);
  if (struct_var_p && (!struct_var_p->Struct || (get_skookum_struct_type(struct_var_p->Struct) == SkTypeID_UStruct && !m_runtime_interface_p->is_static_struct_known_to_skookum(struct_var_p->Struct))))
    {
    return false;
    }

  // Accept all arrays of known types
  UArrayProperty * array_var_p = Cast<UArrayProperty>(var_p);
  if (array_var_p && (!array_var_p->Inner || !can_export_property(array_var_p->Inner, include_priority + 1, referenced_flags)))
    {
    return false;
    }

  // Accept all known static enum types
  UEnum * enum_p = get_enum(var_p);
  if (enum_p && !m_runtime_interface_p->is_static_enum_known_to_skookum(enum_p))
    {
    return false;
    }

  return true;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntimeGenerator::on_type_referenced(UField * type_p, int32 include_priority, uint32 referenced_flags)
  {
  // In this use case this callback should never be called for anything but structs/classes
  m_used_classes.Add(CastChecked<UStruct>(type_p));
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntimeGenerator::report_error(const FString & message)
  {
  FText title = FText::FromString(TEXT("Error during SkookumScript script file generation!"));
  FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(message), &title);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntimeGenerator::generate_class_script_files(UClass * ue_class_p, bool generate_data, bool skip_non_game_classes, bool check_if_reparented)
  {
  check(!m_overlay_path.IsEmpty());

  // Do not generate any script files in commandlet mode
  if (IsRunningCommandlet())
    {
    return;
    }

  // Only generate script files for game assets if requested
  UPackage * package_p = Cast<UPackage>(ue_class_p->GetOutermost());
  if (skip_non_game_classes && (!package_p || (!package_p->FileName.ToString().StartsWith(TEXT("/Game/")) && !package_p->GetName().StartsWith(TEXT("/Game/")))))
    {
    return;
    }

#if !PLATFORM_EXCEPTIONS_DISABLED
  try
#endif
    {
    FString class_name;
    const FString class_path = get_skookum_class_path(ue_class_p, 0, 0, &class_name);

    // Make sure there is no other folder with the same name around
    if (check_if_reparented)
      {
      TArray<FString> found_folders;
      // 1) Try to find any folder with the exact class_name
      IFileManager::Get().FindFilesRecursive(found_folders, *m_overlay_path, *class_name, false, true);
      if (found_folders.Num() == 0)
        {
        // 2) Try to find any folder with the pattern parent_name.class_name
        FString pattern = TEXT("*.") + class_name;
        IFileManager::Get().FindFilesRecursive(found_folders, *m_overlay_path, *pattern, false, true);
        }
      // Found anything?
      for (FString & folder_path : found_folders)
        {
        // Is it the same as the one we are in already?
        if (IFileManager::Get().ConvertToRelativePath(*folder_path) != IFileManager::Get().ConvertToRelativePath(*class_path))
          {
          // No, delete
          IFileManager::Get().DeleteDirectory(*folder_path, false, true);
          }
        }
      }

    // Create class meta file
    const FString meta_file_path = class_path / TEXT("!Class.sk-meta");
    ISkookumScriptRuntimeInterface::eChangeType change_type = FPaths::FileExists(meta_file_path) ? ISkookumScriptRuntimeInterface::ChangeType_modified : ISkookumScriptRuntimeInterface::ChangeType_created;
    FString meta_body = generate_class_meta_file_body(ue_class_p);
    bool anything_changed = save_text_file_if_changed(*meta_file_path, meta_body);

    // Create raw data member file
    if (generate_data)
      {
      FString data_body = generate_class_instance_data_file_body(ue_class_p, 0, Referenced_by_game_module);
      FString data_file_path = class_path / TEXT("!Data.sk");
      if (save_text_file_if_changed(*data_file_path, data_body))
        {
        anything_changed = true;
        }
      }

    if (anything_changed)
      {
      //tSourceControlCheckoutFunc checkout_f = ISourceControlModule::Get().IsEnabled() ? &SourceControlHelpers::CheckOutFile : nullptr;
      tSourceControlCheckoutFunc checkout_f = nullptr; // Leaving this disabled for now as it might be bothersome
      flush_saved_text_files(checkout_f);
      m_runtime_interface_p->on_class_scripts_changed_by_generator(class_name, change_type);
      }
    }
#if !PLATFORM_EXCEPTIONS_DISABLED
  catch (TCHAR * error_msg_p)
    {
    checkf(false, error_msg_p);
    }
#endif
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntimeGenerator::generate_used_class_script_files()
  {
  // Loop through all previously used classes and create stubs for them
  for (tUsedClasses::TConstIterator iter(m_used_classes); iter; ++iter)
    {
    UClass * class_p = Cast<UClass>(*iter);
    if (class_p)
      {
      generate_class_script_files(class_p, false, false, true);
      }
    }

  m_used_classes.Empty();
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntimeGenerator::rename_class_script_files(UClass * ue_class_p, const FString & old_class_name)
  {
  // Rename this class
  FString new_class_name = get_skookum_class_name(ue_class_p);
  rename_class_script_files(ue_class_p, old_class_name, new_class_name);

  // Also attempt to rename all child classes as they may have the name of the parent in its folder name
  for (TObjectIterator<UClass> it; it; ++it)
    {
    if (it->GetSuperClass() == ue_class_p)
      {
      rename_class_script_files(*it, old_class_name, new_class_name);
      }
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntimeGenerator::rename_class_script_files(UClass * ue_class_p, const FString & old_class_name, const FString & new_class_name)
  {
#if !PLATFORM_EXCEPTIONS_DISABLED
  try
#endif
    {
    FString this_class_name;
    const FString this_class_path = get_skookum_class_path(ue_class_p, 0, 0, &this_class_name);
    // Construct old path form new path
    FString replace_from = new_class_name;
    FString replace_to = skookify_class_name(old_class_name);
    // If we are replacing a parent name, append a dot to avoid accidentally modifying the class name itself
    if (this_class_name != new_class_name)
      {
      replace_from += TEXT(".");
      replace_to += TEXT(".");
      }
    FString old_class_path = this_class_path.Replace(*replace_from, *replace_to);
    if (this_class_path != old_class_path)
      {
      // Does old class folder exist?
      if (FPaths::DirectoryExists(old_class_path))
        {
        // Old folder exists - decide how to change its name
        // Does new class folder already exist?
        if (FPaths::DirectoryExists(this_class_path))
          {
          // Yes, delete so we can rename the old folder
          IFileManager::Get().DeleteDirectory(*this_class_path, false, true);
          }
        // Now rename old to new
        if (!IFileManager::Get().Move(*this_class_path, *old_class_path, true, true))
          {
          report_error(FString::Printf(TEXT("Couldn't rename class from '%s' to '%s'"), *old_class_path, *this_class_path));
          }
        // Regenerate the meta file to correctly reflect the Blueprint it originated from
        generate_class_script_files(ue_class_p, false, false, false);
        // Inform the runtime module of the change
        m_runtime_interface_p->on_class_scripts_changed_by_generator(old_class_name, ISkookumScriptRuntimeInterface::ChangeType_deleted);
        m_runtime_interface_p->on_class_scripts_changed_by_generator(new_class_name, ISkookumScriptRuntimeInterface::ChangeType_created);
        }
      else
        {
        // Old folder does not exist - check that new folder exists, assuming the class has already been renamed
        checkf(FPaths::DirectoryExists(this_class_path), TEXT("Couldn't rename class from '%s' to '%s'. Neither old nor new class folder exist."), *old_class_path, *this_class_path);
        }
      }
    }
#if !PLATFORM_EXCEPTIONS_DISABLED
  catch (TCHAR * error_msg_p)
    {
    checkf(false, error_msg_p);
    }
#endif
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntimeGenerator::delete_class_script_files(UClass * ue_class_p)
  {
  FString class_name;
  const FString directory_to_delete = get_skookum_class_path(ue_class_p, 0, 0, &class_name);
  if (FPaths::DirectoryExists(directory_to_delete))
    {
    IFileManager::Get().DeleteDirectory(*directory_to_delete, false, true);
    m_runtime_interface_p->on_class_scripts_changed_by_generator(class_name, ISkookumScriptRuntimeInterface::ChangeType_deleted);
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntimeGenerator::initialize_paths()
  {
  // Look for default SkookumScript project file in engine folder.
  FString plugin_root_path(IPluginManager::Get().FindPlugin(TEXT("SkookumScript"))->GetBaseDir());
  FString default_project_file_path(plugin_root_path / TEXT("Scripts/Skookum-project-default.ini"));
  checkf(FPaths::FileExists(default_project_file_path), TEXT("Cannot find default project settings file '%s'!"), *default_project_file_path);
  m_default_project_file_path = FPaths::ConvertRelativePathToFull(default_project_file_path);
  m_project_file_path.Empty();

  // Look for specific SkookumScript project in game/project folder.
  FString project_file_path;
  if (!FPaths::GameDir().IsEmpty())
    {
    project_file_path = get_or_create_project_file(FPaths::GameDir(), FApp::GetGameName(), &m_project_mode);
    }
  if (!project_file_path.IsEmpty())
    {
    // Qualify and store for later reference
    m_project_file_path = FPaths::ConvertRelativePathToFull(project_file_path);
    // Set overlay path and depth
    set_overlay_path();
    }
  }

//---------------------------------------------------------------------------------------
// Set overlay path and depth
void FSkookumScriptRuntimeGenerator::set_overlay_path()
  {
  check(!m_project_file_path.IsEmpty());
  FString scripts_path = FPaths::GetPath(m_project_file_path);
  const TCHAR * overlay_name_bp_p = ms_overlay_name_bp_p;
  m_overlay_path = FPaths::ConvertRelativePathToFull(scripts_path / overlay_name_bp_p);
  if (!FPaths::DirectoryExists(m_overlay_path))
    {
    overlay_name_bp_p = ms_overlay_name_bp_old_p;
    m_overlay_path = FPaths::ConvertRelativePathToFull(scripts_path / overlay_name_bp_p);
    }
  compute_scripts_path_depth(m_project_file_path, overlay_name_bp_p);
  }

#endif // WITH_EDITORONLY_DATA
