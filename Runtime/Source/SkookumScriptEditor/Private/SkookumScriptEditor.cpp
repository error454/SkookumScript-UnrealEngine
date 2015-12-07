//=======================================================================================
// SkookumScript Unreal Engine Editor Plugin
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Author: Markus Breyer
//=======================================================================================

#include "SkookumScriptEditorPrivatePCH.h"
#include <ISkookumScriptRuntime.h>
#include <AssetRegistryModule.h>
#include <BlueprintActionDatabase.h>
#include <BlueprintEditorUtils.h>
#include <K2Node_CallFunction.h>
#include <K2Node_Event.h>

#include "SkookumScriptEditorCommands.h"
#include "SkookumStyles.h"
#include "LevelEditor.h"
#include "GraphEditor.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorModule.h"
#include "MultiBoxExtender.h"

#include "../../../../Generator/Source/SkookumScriptGenerator/Private/SkookumScriptGeneratorBase.inl"

DEFINE_LOG_CATEGORY(LogSkookumScriptEditor);

//---------------------------------------------------------------------------------------

class FSkookumScriptEditor : public ISkookumScriptEditor, public ISkookumScriptRuntimeEditorInterface, public FSkookumScriptGeneratorBase
{
public:

protected:

  //---------------------------------------------------------------------------------------
  // IModuleInterface implementation

  virtual void StartupModule() override;
  virtual void ShutdownModule() override;

  //---------------------------------------------------------------------------------------
  // ISkookumScriptRuntimeEditorInterface implementation

  virtual FString       ensure_temp_project(const TCHAR * generated_overlay_name_p) override;
  virtual FString       make_project_editable() override;
  virtual void          set_overlay_path(const FString & scripts_path, const FString & overlay_name) override; // E.g. ProjectName/Scripts/Project-Generated
  virtual int32         get_overlay_path_depth() const override;
  virtual void          generate_all_class_script_files() override;
  virtual void          recompile_blueprints_with_errors() override;
  virtual UBlueprint *  load_blueprint_asset(const FString & class_path, bool * sk_class_deleted_p) override;
  virtual void          on_class_updated(UClass * ue_class_p) override;

  //---------------------------------------------------------------------------------------
  // Local implementation

  ISkookumScriptRuntime * get_runtime() const { return static_cast<ISkookumScriptRuntime *>(m_runtime_p.Get()); }

  void                    on_asset_loaded(UObject * new_object_p);
  void                    on_object_modified(UObject * obj_p);
  void                    on_new_asset_created(UFactory * factory_p);
  void                    on_assets_deleted(const TArray<UClass*> & deleted_asset_classes);
  void                    on_asset_post_import(UFactory * factory_p, UObject * obj_p);
  void                    on_asset_added(const FAssetData & asset_data);
  void                    on_asset_renamed(const FAssetData & asset_data, const FString & old_object_path);
  void                    on_in_memory_asset_created(UObject * obj_p);
  void                    on_in_memory_asset_deleted(UObject * obj_p);
  void                    on_map_opened(const FString & file_name, bool as_template);
  void                    on_blueprint_compiled(UBlueprint * blueprint_p);

  bool                    is_property_type_supported_and_known(UProperty * property_p) const;
  void                    generate_class_script_files(UClass * ue_class_p, bool generate_data);
  void                    rename_class_script_files(UClass * ue_class_p, const FString & old_class_name);
  void                    delete_class_script_files(UClass * ue_class_p);
  void                    generate_used_class_script_files();

  void                    add_skookum_button_to_level_tool_bar(FToolBarBuilder &);
  void                    add_skookum_button_to_blueprint_tool_bar(FToolBarBuilder &);
  void                    on_skookum_button_clicked();

  // Data members

  TSharedPtr<IModuleInterface>  m_runtime_p;  // TSharedPtr holds on to the module so it can't go away while we need it

  FDelegateHandle               m_on_asset_loaded_handle;
  FDelegateHandle               m_on_object_modified_handle;
  FDelegateHandle               m_on_map_opened_handle;
  FDelegateHandle               m_on_new_asset_created_handle;
  FDelegateHandle               m_on_assets_deleted_handle;
  FDelegateHandle               m_on_asset_post_import_handle;
  FDelegateHandle               m_on_asset_added_handle;
  FDelegateHandle               m_on_asset_renamed_handle;
  FDelegateHandle               m_on_in_memory_asset_created_handle;
  FDelegateHandle               m_on_in_memory_asset_deleted_handle;

  FString                       m_package_name_key;
  FString                       m_package_path_key;

  const TCHAR *                 m_editable_ini_settings_p; // ini file settings to describe that a project is not editable

  TSharedPtr<FUICommandList>        m_button_commands;

  TSharedPtr<FExtensibilityManager> m_level_extension_manager;
  TSharedPtr<const FExtensionBase>  m_level_tool_bar_extension;
  TSharedPtr<FExtender>             m_level_tool_bar_extender;

  TSharedPtr<FExtensibilityManager> m_blueprint_extension_manager;
  TSharedPtr<const FExtensionBase>  m_blueprint_tool_bar_extension;
  TSharedPtr<FExtender>             m_blueprint_tool_bar_extender;
  };

IMPLEMENT_MODULE(FSkookumScriptEditor, SkookumScriptEditor)

//=======================================================================================
// IModuleInterface implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::StartupModule()
  {
  // Get pointer to runtime module
  m_runtime_p = FModuleManager::Get().GetModule("SkookumScriptRuntime");

  // Tell runtime that editor is present (needed even in commandlet mode as we might have to demand-load blueprints)
  get_runtime()->set_editor_interface(this);

  // Clear contents of scripts folder for a fresh start
  // Won't work here if project has several maps using different blueprints
  //FString directory_to_delete(m_scripts_path / TEXT("Object"));
  //IFileManager::Get().DeleteDirectory(*directory_to_delete, false, true);

  // Reset super classes
  m_used_classes.Empty();

  // Label used to extract package path from Sk class meta file
  m_package_name_key = TEXT("// UE4 Package Name: \"");
  m_package_path_key = TEXT("// UE4 Package Path: \"");

  // String to insert into/remove from Sk project ini file
  m_editable_ini_settings_p = TEXT("Editable=false\r\nCanMakeEditable=true\r\n");

  if (IsRunningCommandlet())
    {
    // Tell runtime to start skookum now
    get_runtime()->startup_skookum();
    }
  else
    {
    // Hook up delegates
    m_on_asset_loaded_handle = FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &FSkookumScriptEditor::on_asset_loaded);
    m_on_object_modified_handle = FCoreUObjectDelegates::OnObjectModified.AddRaw(this, &FSkookumScriptEditor::on_object_modified);
    m_on_map_opened_handle = FEditorDelegates::OnMapOpened.AddRaw(this, &FSkookumScriptEditor::on_map_opened);
    m_on_new_asset_created_handle = FEditorDelegates::OnNewAssetCreated.AddRaw(this, &FSkookumScriptEditor::on_new_asset_created);
    m_on_assets_deleted_handle = FEditorDelegates::OnAssetsDeleted.AddRaw(this, &FSkookumScriptEditor::on_assets_deleted);
    m_on_asset_post_import_handle = FEditorDelegates::OnAssetPostImport.AddRaw(this, &FSkookumScriptEditor::on_asset_post_import);

    FAssetRegistryModule & asset_registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
    m_on_asset_added_handle = asset_registry.Get().OnAssetAdded().AddRaw(this, &FSkookumScriptEditor::on_asset_added);
    m_on_asset_renamed_handle = asset_registry.Get().OnAssetRenamed().AddRaw(this, &FSkookumScriptEditor::on_asset_renamed);
    m_on_in_memory_asset_created_handle = asset_registry.Get().OnInMemoryAssetCreated().AddRaw(this, &FSkookumScriptEditor::on_in_memory_asset_created);
    m_on_in_memory_asset_deleted_handle = asset_registry.Get().OnInMemoryAssetDeleted().AddRaw(this, &FSkookumScriptEditor::on_in_memory_asset_deleted);

    //---------------------------------------------------------------------------------------
    // UI extension

    // Register commands and styles
    FSkookumScriptEditorCommands::Register();
    FSlateStyleRegistry::UnRegisterSlateStyle(FSkookumStyles::GetStyleSetName()); // Hot reload hack
    FSkookumStyles::Initialize();

    // Button commands 
    m_button_commands = MakeShareable(new FUICommandList);
    m_button_commands->MapAction(
      FSkookumScriptEditorCommands::Get().m_skookum_button,
      FExecuteAction::CreateRaw(this, &FSkookumScriptEditor::on_skookum_button_clicked),
      FCanExecuteAction());

    // Add to level tool bar
    m_level_tool_bar_extender = MakeShareable(new FExtender);
    m_level_tool_bar_extension = m_level_tool_bar_extender->AddToolBarExtension("Compile", EExtensionHook::After, m_button_commands, FToolBarExtensionDelegate::CreateRaw(this, &FSkookumScriptEditor::add_skookum_button_to_level_tool_bar));
    FLevelEditorModule & level_editor_module = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    m_level_extension_manager = level_editor_module.GetToolBarExtensibilityManager();
    m_level_extension_manager->AddExtender(m_level_tool_bar_extender);

    // Add to blueprint tool bar
    m_blueprint_tool_bar_extender = MakeShareable(new FExtender);
    m_blueprint_tool_bar_extension = m_blueprint_tool_bar_extender->AddToolBarExtension("Asset", EExtensionHook::After, m_button_commands, FToolBarExtensionDelegate::CreateRaw(this, &FSkookumScriptEditor::add_skookum_button_to_blueprint_tool_bar));
    FBlueprintEditorModule & blueprint_editor_module = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
    m_blueprint_extension_manager = blueprint_editor_module.GetMenuExtensibilityManager();
    m_blueprint_extension_manager->AddExtender(m_blueprint_tool_bar_extender);
    }

  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::ShutdownModule()
  {
  get_runtime()->set_editor_interface(nullptr);
  m_runtime_p.Reset();

  if (!IsRunningCommandlet())
    {

    // Remove delegates
    FCoreUObjectDelegates::OnAssetLoaded.Remove(m_on_asset_loaded_handle);
    FCoreUObjectDelegates::OnObjectModified.Remove(m_on_object_modified_handle);
    FEditorDelegates::OnMapOpened.Remove(m_on_map_opened_handle);
    FEditorDelegates::OnNewAssetCreated.Remove(m_on_new_asset_created_handle);
    FEditorDelegates::OnAssetsDeleted.Remove(m_on_assets_deleted_handle);
    FEditorDelegates::OnAssetPostImport.Remove(m_on_asset_post_import_handle);

    FAssetRegistryModule * asset_registry_p = FModuleManager::GetModulePtr<FAssetRegistryModule>(AssetRegistryConstants::ModuleName);
    if (asset_registry_p)
      {
      asset_registry_p->Get().OnAssetAdded().Remove(m_on_asset_added_handle);
      asset_registry_p->Get().OnAssetRenamed().Remove(m_on_asset_renamed_handle);
      asset_registry_p->Get().OnInMemoryAssetCreated().Remove(m_on_in_memory_asset_created_handle);
      asset_registry_p->Get().OnInMemoryAssetDeleted().Remove(m_on_in_memory_asset_deleted_handle);
      }

    //---------------------------------------------------------------------------------------
    // UI extension

    if (m_level_extension_manager.IsValid())
      {
      FSkookumScriptEditorCommands::Unregister();
      m_level_tool_bar_extender->RemoveExtension(m_level_tool_bar_extension.ToSharedRef());
      m_level_extension_manager->RemoveExtender(m_level_tool_bar_extender);
      }
    else
      {
      m_level_extension_manager.Reset();
      }

    if (m_blueprint_extension_manager.IsValid())
      {
      FSkookumScriptEditorCommands::Unregister();
      m_blueprint_tool_bar_extender->RemoveExtension(m_blueprint_tool_bar_extension.ToSharedRef());
      m_blueprint_extension_manager->RemoveExtender(m_blueprint_tool_bar_extender);
      }
    else
      {
      m_blueprint_extension_manager.Reset();
      }

    FSkookumStyles::Shutdown();
    }
  }

//=======================================================================================		//=======================================================================================
// ISkookumScriptRuntimeEditorInterface implementation
//=======================================================================================

//---------------------------------------------------------------------------------------
// Make sure a temporary Sk project file/folder exists in the project's `Intermediate` folder

FString FSkookumScriptEditor::ensure_temp_project(const TCHAR * generated_overlay_name_p)
  {
  // Check temporary location (in `Intermediate` folder)
  FString temp_root_path(FPaths::GameIntermediateDir() / TEXT("SkookumScript"));
  FString temp_scripts_path(temp_root_path / TEXT("Scripts"));
  FString project_path = temp_scripts_path / TEXT("Skookum-project.ini");
  if (FPaths::FileExists(project_path))
    {
    set_overlay_path(temp_scripts_path, generated_overlay_name_p);
    }
  else
    {
    // If in neither folder, create new project in temporary location
    // $Revisit MBreyer - read ini file from default_project_path and patch it up to carry over customizations
    FString proj_ini = FString::Printf(TEXT("[Project]\r\nProjectName=%s\r\nStrictParse=true\r\nUseBuiltinActor=false\r\nCustomActorClass=Actor\r\nStartupMind=Master\r\n%s"), FApp::GetGameName(), m_editable_ini_settings_p);
    proj_ini += TEXT("[Output]\r\nCompileManifest=false\r\nCompileTo=../Content/skookumscript/classes.sk-bin\r\n");
    proj_ini += TEXT("[Script Overlays]\r\nOverlay1=*Core|Core\r\nOverlay2=-*Core-Sandbox|Core-Sandbox\r\nOverlay3=*VectorMath|VectorMath\r\nOverlay4=*Engine-Generated|Engine-Generated|3\r\nOverlay5=*Engine|Engine\r\nOverlay6=*Project-Generated|Project-Generated|3\r\n");
    if (FFileHelper::SaveStringToFile(proj_ini, *project_path, FFileHelper::EEncodingOptions::ForceAnsi))
      {
      IFileManager::Get().MakeDirectory(*(temp_root_path / TEXT("Content/skookumscript")), true);
      IFileManager::Get().MakeDirectory(*(temp_scripts_path / generated_overlay_name_p / TEXT("Object")), true);
      set_overlay_path(temp_scripts_path, generated_overlay_name_p);
      generate_all_class_script_files();
      }
    else
      {
      project_path.Empty();
      }
    }

  return project_path;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptEditor::make_project_editable()
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
    FString editable_project_path(editable_scripts_path / TEXT("Skookum-project.ini"));
    if (!FPaths::FileExists(editable_project_path))
      {
      // Check temporary location (in `Intermediate` folder)
      FString temp_root_path(FPaths::GameIntermediateDir() / TEXT("SkookumScript"));
      FString temp_scripts_path(temp_root_path / TEXT("Scripts"));
      FString temp_project_path = temp_scripts_path / TEXT("Skookum-project.ini");
      if (!FPaths::FileExists(temp_project_path))
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
          FString temp_binary_folder_path = temp_root_path / TEXT("Content/skookumscript");
          FString editable_binary_folder_path = FPaths::GameDir() / TEXT("Content/skookumscript");
          IFileManager::Get().Move(*editable_binary_folder_path, *temp_binary_folder_path, true, true);

          // Change project packaging settings to include Sk binaries
          UProjectPackagingSettings * packaging_settings_p = Cast<UProjectPackagingSettings>(UProjectPackagingSettings::StaticClass()->GetDefaultObject());
          const TCHAR * binary_path_name_p = TEXT("skookumscript");
          for (TArray<FDirectoryPath>::TConstIterator dir_path(packaging_settings_p->DirectoriesToAlwaysStageAsNonUFS); dir_path; ++dir_path)
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
            packaging_settings_p->DirectoriesToAlwaysStageAsNonUFS.Add(binary_path);
            FString config_file_name = FPaths::GameConfigDir() / TEXT("DefaultGame.ini");
            if (ISourceControlModule::Get().IsEnabled())
              {
              SourceControlHelpers::CheckOutFile(config_file_name);
              }
            packaging_settings_p->SaveConfig(CPF_Config, *config_file_name);
            }

          // Create Project overlay folder
          IFileManager::Get().MakeDirectory(*(editable_scripts_path / TEXT("Project/Object")), true);

          // Change project to be editable
          FString proj_ini;
          verify(FFileHelper::LoadFileToString(proj_ini, *editable_project_path));
          proj_ini = proj_ini.Replace(m_editable_ini_settings_p, TEXT("")); // Remove editable settings
          proj_ini += TEXT("Overlay7=Project|Project\r\n"); // Create Project overlay definition
          verify(FFileHelper::SaveStringToFile(proj_ini, *editable_project_path, FFileHelper::EEncodingOptions::ForceAnsi));
          }
        }
      }
    }

  return error_msg;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::set_overlay_path(const FString & scripts_path, const FString & overlay_name)
  {
  // Set up scripts path and depth
  m_overlay_path = scripts_path / overlay_name;
  compute_scripts_path_depth(scripts_path / TEXT("Skookum-project.ini"), overlay_name);
  }

//---------------------------------------------------------------------------------------

int32 FSkookumScriptEditor::get_overlay_path_depth() const
  {
  return m_overlay_path_depth;
  }

//---------------------------------------------------------------------------------------
// Generate SkookumScript class script files for all known blueprint assets
void FSkookumScriptEditor::generate_all_class_script_files()
  {
  if (!m_overlay_path.IsEmpty())
    {
    TArray<UObject*> blueprint_array;
    GetObjectsOfClass(UBlueprint::StaticClass(), blueprint_array, false, RF_ClassDefaultObject | RF_PendingKill);
    for (UObject * obj_p : blueprint_array)
      {
      generate_class_script_files(static_cast<UBlueprint *>(obj_p)->GeneratedClass, true);
      }

    generate_used_class_script_files();
    }
  }

//---------------------------------------------------------------------------------------
// Find blueprints that have compile errors and try recompiling them
// (as errors might be due to SkookumScript not having been initialized at previous compile time
void FSkookumScriptEditor::recompile_blueprints_with_errors()
  {
  TArray<UObject*> blueprint_array;
  GetObjectsOfClass(UBlueprint::StaticClass(), blueprint_array, false, RF_ClassDefaultObject | RF_PendingKill);
  for (UObject * obj_p : blueprint_array)
    {
    UBlueprint * blueprint_p = static_cast<UBlueprint *>(obj_p);
    if (blueprint_p->Status == BS_Error)
      {
      FKismetEditorUtilities::CompileBlueprint(blueprint_p);
      }
    }
  }

//---------------------------------------------------------------------------------------
// Attempt to load blueprint with given qualified class path
UBlueprint * FSkookumScriptEditor::load_blueprint_asset(const FString & class_path, bool * sk_class_deleted_p)
  {
  // Try to extract asset path from meta file of Sk class
  FString full_class_path = m_overlay_path / class_path;
  FString meta_file_path = full_class_path / TEXT("!Class.sk-meta");
  FString meta_file_text;
  *sk_class_deleted_p = false;
  if (FFileHelper::LoadFileToString(meta_file_text, *meta_file_path))
    {
    // Found meta file - try to extract asset path contained in it
    int32 package_path_begin_pos = meta_file_text.Find(m_package_name_key);
    if (package_path_begin_pos >= 0)
      {
      package_path_begin_pos += m_package_name_key.Len();
      int32 package_path_end_pos = meta_file_text.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, package_path_begin_pos);
      if (package_path_end_pos > package_path_begin_pos)
        {
        // Successfully got the path of the package, so assemble with asset name and load the asset
        FString package_path = meta_file_text.Mid(package_path_begin_pos, package_path_end_pos - package_path_begin_pos);
        FString class_name = FPaths::GetCleanFilename(class_path);
        // If there's a dot in the name, use portion right of it
        int dot_pos = -1;
        if (class_name.FindChar(TCHAR('.'), dot_pos))
          {
          class_name = class_name.Mid(dot_pos + 1);
          }
        FString asset_path = package_path + TEXT(".") + class_name;
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
            get_runtime()->on_class_scripts_changed_by_editor(class_name, ISkookumScriptRuntime::ChangeType_deleted);
            }
          }
        return blueprint_p;
        }
      }
    }

  return nullptr;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::on_class_updated(UClass * ue_class_p)
  {
  // 1) Refresh actions (in Blueprint editor drop down menu)
  FBlueprintActionDatabase::Get().RefreshClassActions(ue_class_p);

  // Storage for gathered objects
  TArray<UObject*> obj_array;

  // 2) Refresh node display of all SkookumScript function call nodes
  obj_array.Reset();
  GetObjectsOfClass(UK2Node_CallFunction::StaticClass(), obj_array, true, RF_ClassDefaultObject | RF_PendingKill);
  for (auto obj_p : obj_array)
    {
    UK2Node_CallFunction * function_node_p = Cast<UK2Node_CallFunction>(obj_p);
    UFunction * target_function_p = function_node_p->GetTargetFunction();
    // Also refresh all nodes with no target function as it is probably a Sk function that was deleted
    //if (!target_function_p || get_runtime()->is_skookum_blueprint_function(target_function_p))
    if (target_function_p && get_runtime()->is_skookum_blueprint_function(target_function_p))
      {
      const UEdGraphSchema * schema_p = function_node_p->GetGraph()->GetSchema();
      schema_p->ReconstructNode(*function_node_p, true);
      }
    }

  // 3) Refresh node display of all SkookumScript event nodes
  obj_array.Reset();
  GetObjectsOfClass(UK2Node_Event::StaticClass(), obj_array, true, RF_ClassDefaultObject | RF_PendingKill);
  for (auto obj_p : obj_array)
    {
    UK2Node_Event * event_node_p = Cast<UK2Node_Event>(obj_p);
    UFunction * event_function_p = event_node_p->FindEventSignatureFunction();
    if (event_function_p && get_runtime()->is_skookum_blueprint_event(event_function_p))
      {
      const UEdGraphSchema * schema_p = event_node_p->GetGraph()->GetSchema();
      schema_p->ReconstructNode(*event_node_p, true);
      }
    }

  // 4) Try recompiling any Blueprints that previously had errors
  recompile_blueprints_with_errors();
  }

//=======================================================================================
// FSkookumScriptEditor implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::on_asset_loaded(UObject * new_object_p)
  {
  UBlueprint * blueprint_p = Cast<UBlueprint>(new_object_p);
  if (blueprint_p)
    {
    // Register callback so we know when this Blueprint has been compiled
    blueprint_p->OnCompiled().AddRaw(this, &FSkookumScriptEditor::on_blueprint_compiled);

    if (!m_overlay_path.IsEmpty())
      {
      // And generate script files
      generate_class_script_files(blueprint_p->GeneratedClass, true);
      generate_used_class_script_files();
      }
    }
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_object_modified(UObject * obj_p)
  {
  if (!m_overlay_path.IsEmpty())
    {
    // Is this a blueprint?
    UBlueprint * blueprint_p = Cast<UBlueprint>(obj_p);
    if (blueprint_p)
      {
      generate_class_script_files(blueprint_p->GeneratedClass, true);
      }
    }
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_new_asset_created(UFactory * factory_p)
  {
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_assets_deleted(const TArray<UClass*> & deleted_asset_classes)
  {
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_asset_post_import(UFactory * factory_p, UObject * obj_p)
  {
  if (!m_overlay_path.IsEmpty())
    {
    UBlueprint * blueprint_p = Cast<UBlueprint>(obj_p);
    if (blueprint_p)
      {
      generate_class_script_files(blueprint_p->GeneratedClass, true);
      generate_used_class_script_files();
      }
    }
  }

//---------------------------------------------------------------------------------------
// An asset is being added to the asset registry - during load, or otherwise
void FSkookumScriptEditor::on_asset_added(const FAssetData & asset_data)
  {
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_asset_renamed(const FAssetData & asset_data, const FString & old_object_path)
  {
  static FName s_blueprint_class_name(TEXT("Blueprint"));

  if (!m_overlay_path.IsEmpty() && asset_data.AssetClass == s_blueprint_class_name)
    {
    UBlueprint * blueprint_p = FindObjectChecked<UBlueprint>(ANY_PACKAGE, *asset_data.AssetName.GetPlainNameString());
    if (blueprint_p)
      {
      rename_class_script_files(blueprint_p->GeneratedClass, FPaths::GetBaseFilename(old_object_path));
      }
    }
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_in_memory_asset_created(UObject * obj_p)
  {
  if (!m_overlay_path.IsEmpty())
    {
    UBlueprint * blueprint_p = Cast<UBlueprint>(obj_p);
    if (blueprint_p)
      {
      generate_class_script_files(blueprint_p->GeneratedClass, true);
      generate_used_class_script_files();
      }
    }
  }

//---------------------------------------------------------------------------------------
//
void FSkookumScriptEditor::on_in_memory_asset_deleted(UObject * obj_p)
  {
  if (!m_overlay_path.IsEmpty())
    {
    UBlueprint * blueprint_p = Cast<UBlueprint>(obj_p);
    if (blueprint_p)
      {
      delete_class_script_files(blueprint_p->GeneratedClass);
      }
    }
  }

//---------------------------------------------------------------------------------------
// Called when the map is done loading (load progress reaches 100%)
void FSkookumScriptEditor::on_map_opened(const FString & file_name, bool as_template)
  {
  // Generate all script files one more time to be sure
  generate_all_class_script_files();

  // Let runtime know we are done opening a new map
  get_runtime()->on_editor_map_opened();
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::on_blueprint_compiled(UBlueprint * blueprint_p)
  {
  // Re-generate script files for this class as things might have changed
  if (!m_overlay_path.IsEmpty())
    {
    generate_class_script_files(blueprint_p->GeneratedClass, true);
    }

  // Check that there's no dangling default constructor
  bool has_skookum_default_constructor = get_runtime()->has_skookum_default_constructor(blueprint_p->GeneratedClass);
  bool has_skookum_destructor = get_runtime()->has_skookum_destructor(blueprint_p->GeneratedClass);
  if (has_skookum_default_constructor || has_skookum_destructor)
    {
    // Determine if it has a SkookumScript component
    bool has_component = false;
    for (TFieldIterator<UProperty> property_it(blueprint_p->GeneratedClass, EFieldIteratorFlags::ExcludeSuper); property_it; ++property_it)
      {
      UObjectPropertyBase * obj_property_p = Cast<UObjectPropertyBase>(*property_it);
      if (obj_property_p)
        {
        if (get_runtime()->is_skookum_component_class(obj_property_p->PropertyClass))
          {
          has_component = true;
          break;
          }
        }
      }

    // Has no Sk component, see if it is called by the graph somewhere
    if (!has_component)
      {
      bool has_constructor_node = false;
      bool has_destructor_node = false;
      TArray<class UEdGraph*> graph_array;
      blueprint_p->GetAllGraphs(graph_array);
      for (auto graph_iter = graph_array.CreateConstIterator(); graph_iter && !has_constructor_node; ++graph_iter)
        {
        UEdGraph * graph_p = *graph_iter;
        for (auto node_iter = graph_p->Nodes.CreateConstIterator(); node_iter; ++node_iter)
          {
          UK2Node_CallFunction * function_node_p = Cast<UK2Node_CallFunction>(*node_iter);
          if (function_node_p)
            {
            UFunction * function_p = function_node_p->GetTargetFunction();
            if (get_runtime()->is_skookum_blueprint_function(function_p))
              {
              if (function_p->GetName().EndsWith(TEXT("@ !"), ESearchCase::CaseSensitive))
                {
                has_constructor_node = true;
                if (has_destructor_node) break; // If we found both we can stop looking any further
                }
              else if (function_p->GetName().EndsWith(TEXT("@ !!"), ESearchCase::CaseSensitive))
                {
                has_destructor_node = true;
                if (has_constructor_node) break; // If we found both we can stop looking any further
                }
              }
            }
          }
        }

      if (has_skookum_default_constructor && !has_constructor_node)
        {
        FText title = FText::Format(FText::FromString(TEXT("SkookumScript default constructor never called in '{0}'")), FText::FromString(blueprint_p->GetName()));
        FMessageDialog::Open(
          EAppMsgType::Ok,
          FText::Format(FText::FromString(
          TEXT("The SkookumScript class '{0}' has a default constructor but the blueprint has neither a SkookumScript component nor is the default constructor invoked somewhere in an event graph. ")
          TEXT("That means the default constructor will never be called. ")
          TEXT("To fix this issue, either add a SkookumScript component to the Blueprint, or invoke the default constructor explicitely in its event graph.")), FText::FromString(blueprint_p->GetName())),
          &title);
        }
      else if (has_skookum_destructor && !has_destructor_node) // `else if` here so we won't show both dialogs, one is enough
        {
        FText title = FText::Format(FText::FromString(TEXT("SkookumScript destructor never called in '{0}'")), FText::FromString(blueprint_p->GetName()));
        FMessageDialog::Open(
          EAppMsgType::Ok,
          FText::Format(FText::FromString(
          TEXT("The SkookumScript class '{0}' has a destructor but the blueprint has neither a SkookumScript component nor is the destructor invoked somewhere in an event graph. ")
          TEXT("That means the destructor will never be called. ")
          TEXT("To fix this issue, either add a SkookumScript component to the Blueprint, or invoke the destructor explicitely in its event graph.")), FText::FromString(blueprint_p->GetName())),
          &title);
        }
      }
    }
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptEditor::is_property_type_supported_and_known(UProperty * var_p) const
  {
  if (!is_property_type_supported(var_p))
    {
    return false;
    }

  UObjectPropertyBase * object_var_p = Cast<UObjectPropertyBase>(var_p);
  if (object_var_p && (!object_var_p->PropertyClass || !get_runtime()->is_static_class_known_to_skookum(object_var_p->PropertyClass)))
    {
    return false;
    }

  UStructProperty * struct_var_p = Cast<UStructProperty>(var_p);
  if (struct_var_p && (!struct_var_p->Struct || (get_skookum_struct_type(struct_var_p->Struct) == SkTypeID_UStruct && !get_runtime()->is_static_struct_known_to_skookum(struct_var_p->Struct))))
    {
    return false;
    }

  UArrayProperty * array_var_p = Cast<UArrayProperty>(var_p);
  if (array_var_p && (!array_var_p->Inner || !is_property_type_supported_and_known(array_var_p->Inner)))
    {
    return false;
    }

  UEnum * enum_p = get_enum(var_p);
  if (enum_p && !get_runtime()->is_static_enum_known_to_skookum(enum_p))
    {
    return false;
    }

  return true;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::generate_class_script_files(UClass * ue_class_p, bool generate_data)
  {
  check(!m_overlay_path.IsEmpty());

#if !PLATFORM_EXCEPTIONS_DISABLED
  try
#endif
    {
    FString class_name;
    const FString class_path = get_skookum_class_path(ue_class_p, &class_name);

    // Create class meta file
    const FString meta_file_path = class_path / TEXT("!Class.sk-meta");
    ISkookumScriptRuntime::eChangeType change_type = FPaths::FileExists(meta_file_path) ? ISkookumScriptRuntime::ChangeType_modified : ISkookumScriptRuntime::ChangeType_created;
    FString meta_body = get_comment_block(ue_class_p);
    UBlueprint * blueprint_p = UBlueprint::GetBlueprintFromClass(ue_class_p);
    if (blueprint_p)
      {
      meta_body += TEXT("// UE4 Asset Name: ") + blueprint_p->GetName() + TEXT("\r\n");
      UPackage * package_p = Cast<UPackage>(blueprint_p->GetOuter());
      if (package_p)
        {
        meta_body += m_package_name_key + package_p->GetName() + TEXT("\"\r\n");
        meta_body += m_package_path_key + package_p->FileName.ToString() + TEXT("\"\r\n");
        }
      }

    if (!FFileHelper::SaveStringToFile(meta_body, *meta_file_path, ms_script_file_encoding))
      {
      FError::Throwf(TEXT("Could not save file: %s"), *meta_file_path);
      }

    // Create raw data member file
    if (generate_data)
      {
      FString data_body = TEXT("\r\n");

      // Figure out column width of variable types & names
      int32 max_type_length = 0;
      int32 max_name_length = 0;
      for (TFieldIterator<UProperty> property_it(ue_class_p, EFieldIteratorFlags::ExcludeSuper); property_it; ++property_it)
        {
        UProperty * var_p = *property_it;
        if (is_property_type_supported_and_known(var_p))
          {
          FString type_name = get_skookum_property_type_name_existing(var_p);
          FString var_name = skookify_var_name(var_p->GetName(), var_p->IsA(UBoolProperty::StaticClass()), true);
          max_type_length = FMath::Max(max_type_length, type_name.Len());
          max_name_length = FMath::Max(max_name_length, var_name.Len());
          }
        }

      // Format nicely
      for (TFieldIterator<UProperty> property_it(ue_class_p, EFieldIteratorFlags::ExcludeSuper); property_it; ++property_it)
        {
        UProperty * var_p = *property_it;
        FString type_name = get_skookum_property_type_name_existing(var_p);
        FString var_name = skookify_var_name(var_p->GetName(), var_p->IsA(UBoolProperty::StaticClass()), true);
        if (is_property_type_supported_and_known(var_p))
          {
          FString comment = var_p->GetToolTipText().ToString().Replace(TEXT("\n"), TEXT(" "));
          data_body += FString::Printf(TEXT("&raw %s !%s // %s%s[%s]\r\n"), *(type_name.RightPad(max_type_length)), *(var_name.RightPad(max_name_length)), *comment, comment.IsEmpty() ? TEXT("") : TEXT(" "), *var_p->GetName());
          }
        else
          {
          data_body += FString::Printf(TEXT("// &raw %s !%s // Currently unsupported\r\n"), *(type_name.RightPad(max_type_length)), *(var_name.RightPad(max_name_length)));
          }
        }

      FString data_file_path = class_path / TEXT("!Data.sk");
      if (save_text_file_if_changed(*data_file_path, data_body))
        {
        flush_saved_text_files();
        get_runtime()->on_class_scripts_changed_by_editor(class_name, change_type);
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

void FSkookumScriptEditor::generate_used_class_script_files()
  {
  // Loop through all previously classes and create stubs for them
  for (auto struct_p : m_used_classes)
    {
    UClass * class_p = Cast<UClass>(struct_p);
    if (class_p)
      {
      generate_class_script_files(class_p, false);
      }
    }

  m_used_classes.Empty();
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::rename_class_script_files(UClass * ue_class_p, const FString & old_class_name)
  {
#if !PLATFORM_EXCEPTIONS_DISABLED
  try
#endif
    {
    FString new_class_name;
    const FString new_class_path = get_skookum_class_path(ue_class_p, &new_class_name);
    FString old_class_path = FPaths::GetPath(new_class_path) / old_class_name;
    // Does old class folder exist?
    if (FPaths::DirectoryExists(old_class_path))
      {
      // Old folder exists - decide how to change its name
      // Does new class folder already exist?
      if (FPaths::DirectoryExists(new_class_path))
        {
        // Yes, delete so we can rename the old folder
        IFileManager::Get().DeleteDirectory(*new_class_path, false, true);
        }
      // Now rename old to new
      if (!IFileManager::Get().Move(*new_class_path, *old_class_path, true, true))
        {
        FError::Throwf(TEXT("Couldn't rename class from '%s' to '%s'"), *old_class_path, *new_class_path);
        }
      get_runtime()->on_class_scripts_changed_by_editor(old_class_name, ISkookumScriptRuntime::ChangeType_deleted);
      get_runtime()->on_class_scripts_changed_by_editor(new_class_path, ISkookumScriptRuntime::ChangeType_created);
      }
    else
      {
      // Old folder does not exist - check that new folder exists, assuming the class has already been renamed
      checkf(FPaths::DirectoryExists(new_class_path), TEXT("Couldn't rename class from '%s' to '%s'. Neither old nor new class folder exist."), *old_class_path, *new_class_path);
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

void FSkookumScriptEditor::delete_class_script_files(UClass * ue_class_p)
  {
  FString class_name;
  const FString directory_to_delete = get_skookum_class_path(ue_class_p, &class_name);
  if (FPaths::DirectoryExists(directory_to_delete))
    {
    IFileManager::Get().DeleteDirectory(*directory_to_delete, false, true);
    get_runtime()->on_class_scripts_changed_by_editor(class_name, ISkookumScriptRuntime::ChangeType_deleted);
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::add_skookum_button_to_level_tool_bar(FToolBarBuilder & builder)
  {
  #define LOCTEXT_NAMESPACE "LevelEditorToolBar"

    FSlateIcon icon_brush = FSlateIcon(FSkookumStyles::GetStyleSetName(), "SkookumScriptEditor.ShowIDE", "SkookumScriptEditor.ShowIDE.Small");
    builder.AddToolBarButton(FSkookumScriptEditorCommands::Get().m_skookum_button, NAME_None, LOCTEXT("SkookumButton_Override", "Skookum IDE"), LOCTEXT("SkookumButton_ToolTipOverride", "Summons the Skookum IDE and navigates to the related class"), icon_brush, NAME_None);

  #undef LOCTEXT_NAMESPACE
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::add_skookum_button_to_blueprint_tool_bar(FToolBarBuilder & builder)
  {
  #define LOCTEXT_NAMESPACE "BlueprintEditorToolBar"

    FSlateIcon icon_brush = FSlateIcon(FSkookumStyles::GetStyleSetName(), "SkookumScriptEditor.ShowIDE", "SkookumScriptEditor.ShowIDE.Small");
    builder.AddToolBarButton(FSkookumScriptEditorCommands::Get().m_skookum_button, NAME_None, LOCTEXT("SkookumButton_Override", "Show in IDE"), LOCTEXT("SkookumButton_ToolTipOverride", "Summons the Skookum IDE and navigates to the related class or method/coroutine"), icon_brush, NAME_None);

  #undef LOCTEXT_NAMESPACE
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptEditor::on_skookum_button_clicked()
  {
  FString focus_class_name;
  FString focus_member_name;

  // There must be a better way of finding the active editor...
  TArray<UObject*> obj_array = FAssetEditorManager::Get().GetAllEditedAssets();
  double latest_activation_time = 0.0;
  IAssetEditorInstance * active_editor_p = nullptr;
  if (obj_array.Num() > 0)
    {
    for (auto obj_iter = obj_array.CreateConstIterator(); obj_iter; ++obj_iter)
      {
      // Don't allow user to perform certain actions on objects that aren't actually assets (e.g. Level Script blueprint objects)
      UBlueprint * blueprint_p = Cast<UBlueprint>(*obj_iter);
      if (blueprint_p)
        {
        IAssetEditorInstance * editor_p = FAssetEditorManager::Get().FindEditorForAsset(blueprint_p, false);
        if (editor_p->GetLastActivationTime() > latest_activation_time)
          {
          latest_activation_time = editor_p->GetLastActivationTime();
          active_editor_p = editor_p;
          }
        }
      }
    }

  if (active_editor_p)
    {
    // We found the most recently used Blueprint editor
    FBlueprintEditor * blueprint_editor_p = static_cast<FBlueprintEditor *>(active_editor_p);
    // Get name of associated Blueprint
    focus_class_name = blueprint_editor_p->GetBlueprintObj()->GetName();
    // See if any SkookumScript node is selected, if so, tell the IDE
    const FGraphPanelSelectionSet node_array = blueprint_editor_p->GetSelectedNodes();
    for (auto obj_iter = node_array.CreateConstIterator(); obj_iter; ++obj_iter)
      {
      UK2Node_CallFunction * call_node_p = Cast<UK2Node_CallFunction>(*obj_iter);
      if (call_node_p)
        {
        focus_member_name = call_node_p->FunctionReference.GetMemberName().ToString();
        break; // For now, just return the first one
        }
      UK2Node_Event * event_node_p = Cast<UK2Node_Event>(*obj_iter);
      if (event_node_p)
        {
        focus_member_name = event_node_p->EventReference.GetMemberName().ToString();
        break; // For now, just return the first one
        }
      }
    }

  // Bring up IDE and navigate to selected class/method/coroutine
  get_runtime()->show_ide(focus_class_name, focus_member_name, false, false);

  // Request recompilation if there have previously been errors
  get_runtime()->freshen_compiled_binaries_if_have_errors();
  }
