//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Main entry point for the SkookumScript runtime plugin
// 
// Author: Conan Reis
//=======================================================================================

#include "SkookumScriptRuntimePrivatePCH.h"

#include "ISkookumScriptRuntime.h"
#include "Bindings/SkUEBindings.hpp"
#include "Bindings/SkUEClassBinding.hpp"
#include "Bindings/SkUERuntime.hpp"
#include "Bindings/SkUERemote.hpp"
#include "Bindings/SkUEBlueprintInterface.hpp"
#include "Bindings/SkUESymbol.hpp"
#include "Bindings/SkUEUtils.hpp"

#include "SkookumScriptRuntimeGenerator.h"
#include "SkookumScriptBehaviorComponent.h"
#include "SkookumScriptClassDataComponent.h"
#include "SkookumScriptMindComponent.h"

#include "Runtime/Launch/Resources/Version.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Stats.h"

#include <AgogCore/AMethodArg.hpp>
#include <SkookumScript/SkSymbolDefs.hpp>

#if defined(A_PLAT_PC)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// For profiling SkookumScript performance
DECLARE_CYCLE_STAT(TEXT("SkookumScript Time"), STAT_SkookumScriptTime, STATGROUP_Game);

//---------------------------------------------------------------------------------------
// UE4 implementation of AAppInfoCore
class FAppInfo : public AAppInfoCore, public SkAppInfo
  {
  public:

    FAppInfo();
    ~FAppInfo();

  protected:

    // AAppInfoCore implementation

    virtual void *             malloc(size_t size, const char * debug_name_p) override;
    virtual void               free(void * mem_p) override;
    virtual uint32_t           request_byte_size(uint32_t size_requested) override;
    virtual bool               is_using_fixed_size_pools() override;
    virtual void               debug_print(const char * cstr_p) override;
    virtual AErrorOutputBase * on_error_pre(bool nested) override;
    virtual void               on_error_post(eAErrAction action) override;
    virtual void               on_error_quit() override;

    // SkAppInfo implementation

    virtual bool               use_builtin_actor() const override;
    virtual ASymbol            get_custom_actor_class_name() const override;

  };

//---------------------------------------------------------------------------------------
class FSkookumScriptRuntime : public ISkookumScriptRuntime
#if WITH_EDITORONLY_DATA
  , public ISkookumScriptRuntimeInterface
#endif
  {
  public:

    FSkookumScriptRuntime();
    ~FSkookumScriptRuntime();

  protected:

  // Methods

    // Overridden from IModuleInterface

    virtual void    StartupModule() override;
    //virtual void    PostLoadCallback() override;
    //virtual void    PreUnloadCallback() override;
    virtual void    ShutdownModule() override;

    // Overridden from ISkookumScriptRuntime

    virtual void  set_project_generated_bindings(SkUEBindingsInterface * project_generated_bindings_p) override;
    virtual bool  is_skookum_disabled() const override;    
    virtual bool  is_freshen_binaries_pending() const override;

    #if WITH_EDITOR

      virtual void  set_editor_interface(ISkookumScriptRuntimeEditorInterface * editor_interface_p) override;

      virtual void  on_editor_map_opened() override;
      virtual void  show_ide(const FString & focus_class_name, const FString & focus_member_name, bool is_data_member, bool is_class_member) override;
      virtual void  freshen_compiled_binaries_if_have_errors() override;

      virtual bool  has_skookum_default_constructor(UClass * class_p) const override;
      virtual bool  has_skookum_destructor(UClass * class_p) const override;
      virtual bool  is_skookum_class_data_component_class(UClass * class_p) const override;
      virtual bool  is_skookum_behavior_component_class(UClass * class_p) const override;
      virtual bool  is_skookum_blueprint_function(UFunction * function_p) const override;
      virtual bool  is_skookum_blueprint_event(UFunction * function_p) const override;

      virtual void  on_class_added_or_modified(UClass * ue_class_p, bool check_if_reparented) override;
      virtual void  on_class_renamed(UClass * ue_class_p, const FString & old_class_name) override;
      virtual void  on_class_renamed(UClass * ue_class_p, const FString & old_class_name, const FString & new_class_name) override;
      virtual void  on_class_deleted(UClass * ue_class_p) override;

    #endif

    // Overridden from ISkookumScriptRuntimeGeneratorInterface

    #if WITH_EDITORONLY_DATA

      virtual bool  is_static_class_known_to_skookum(UClass * class_p) const override;
      virtual bool  is_static_struct_known_to_skookum(UStruct * struct_p) const override;
      virtual bool  is_static_enum_known_to_skookum(UEnum * enum_p) const override;

      virtual void  on_class_scripts_changed_by_generator(const FString & class_name, eChangeType change_type) override;
      virtual bool  check_out_file(const FString & file_path) const override;

    #endif

    // Local methods

    bool          is_skookum_initialized() const;
    void          ensure_runtime_initialized();
    void          compile_and_load_binaries();

    void          tick_game(float deltaTime);
    void          tick_editor(float deltaTime);

    #ifdef SKOOKUM_REMOTE_UNREAL
      void        tick_remote();
    #endif
      
    void          on_post_engine_init();
    void          on_world_init_pre(UWorld * world_p, const UWorld::InitializationValues init_vals);
    void          on_world_init_post(UWorld * world_p, const UWorld::InitializationValues init_vals);
    void          on_world_cleanup(UWorld * world_p, bool session_ended_b, bool cleanup_resources_b);

  // Data Members

    bool                    m_is_skookum_disabled;

    FAppInfo                m_app_info;

    mutable SkUERuntime     m_runtime;

    #if WITH_EDITORONLY_DATA
      FSkookumScriptRuntimeGenerator  m_generator;
    #endif

    #ifdef SKOOKUM_REMOTE_UNREAL
      SkUERemote            m_remote_client;
      bool                  m_freshen_binaries_requested;
    #endif

    UWorld *                m_game_world_p;
    UWorld *                m_editor_world_p;

    uint32                  m_num_game_worlds;

    FDelegateHandle         m_on_post_engine_init_handle;
    FDelegateHandle         m_on_world_init_pre_handle;
    FDelegateHandle         m_on_world_init_post_handle;
    FDelegateHandle         m_on_world_cleanup_handle;

    FDelegateHandle         m_game_tick_handle;
    FDelegateHandle         m_editor_tick_handle;
  };


//---------------------------------------------------------------------------------------
// Simple error dialog until more sophisticated one in place.
// Could communicate remotely with SkookumIDE and have it bring up message window.
class ASimpleErrorOutput : public AErrorOutputBase
  {
    public:

    virtual bool determine_choice(const AErrMsg & info, eAErrAction * action_p) override;

  };

//---------------------------------------------------------------------------------------
// Determines which error choice to take by prompting the user.  It also writes out
// information to the default output window(s).
// 
// # Returns:  true if a user break should be made in the debugger, false if not
// 
// # Params:
//   msg:      See the definition of `AErrMsg` in ADebug.hpp for more information.
//   action_p: address to store chosen course of action to take to resolve error
//   
// # Author(s): Conan Reis
bool ASimpleErrorOutput::determine_choice(
  const AErrMsg & msg,
  eAErrAction *   action_p
  )
  {
  const char * title_p;
  const char * choice_p    = NULL;
  bool         dbg_present = FPlatformMisc::IsDebuggerPresent();

  // Set pop-up attributes and default values
  switch (msg.m_err_level)
    {
    case AErrLevel_internal:
      title_p = (msg.m_title_p ? msg.m_title_p : "Internal recoverable exception");
      break;

    default:
      title_p  = (msg.m_title_p ? msg.m_title_p : "Error");
      choice_p =
        "\nChoose:\n"
        "  'Abort'  - break into C++ & get callstack [then ignore on continue]\n"
        "  'Retry'  - attempt recovery/ignore [still tests this assert in future]\n"
        "  'Ignore' - recover/ignore always [auto-ignores this assert in future]";
    }

  // Format description
  char         desc_p[2048];
  AString      desc(desc_p, 2048, 0u, false);
  const char * high_desc_p = msg.m_desc_high_p ? msg.m_desc_high_p : "An error has occurred.";
  const char * low_desc_p  = msg.m_desc_low_p  ? msg.m_desc_low_p  : "";
  const char * func_desc_p = msg.m_func_name_p ? msg.m_func_name_p : "";

  desc.insert(high_desc_p);
  desc.append(ADebug::context_string());

  // Ensure there is some space
  desc.ensure_size_extra(512u);

  if (msg.m_source_path_p)
    {
    desc.append_format("\n\n  C++ Internal Info:\n    %s\n    %s(%u) :\n    %s\n", func_desc_p, msg.m_source_path_p, msg.m_source_line, low_desc_p, msg.m_err_id);
    }
  else
    {
    desc.append_format("\n\n  C++ Internal Info:\n    %s\n    %s\n", func_desc_p, low_desc_p, msg.m_err_id);
    }

  // Print out to debug system first
  ADebug::print_format("\n###%s : ", title_p);
  ADebug::print(desc);

  desc.append(choice_p);

  // Prompt user (if necessary)
  eAErrAction action     = AErrAction_ignore;
  bool        user_break = false;

  if (choice_p)
    {
    #if defined(A_PLAT_PC)

      int result = ::MessageBoxA(
        NULL, desc, title_p, MB_ICONEXCLAMATION | MB_ABORTRETRYIGNORE | MB_DEFBUTTON1 | MB_SETFOREGROUND | MB_APPLMODAL);

      switch (result)
        {
        case IDABORT:    // Abort button was selected.
          user_break = true;
          action     = AErrAction_ignore;
          break;

        case IDRETRY:    // Retry button was selected.
          action = AErrAction_ignore;
          break;

        case IDIGNORE:   // Ignore button was selected.
          action = AErrAction_ignore_all;
          break;
        }
    #else
      user_break = dbg_present;
    #endif
    }

  *action_p = action;

  return user_break;
  }


//=======================================================================================
// Global Function Definitions
//=======================================================================================

IMPLEMENT_MODULE(FSkookumScriptRuntime, SkookumScriptRuntime)
DEFINE_LOG_CATEGORY(LogSkookum);


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// FAppInterface implementation
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//---------------------------------------------------------------------------------------

FAppInfo::FAppInfo()
  {
  AgogCore::initialize(this);
  SkookumScript::set_app_info(this);
  SkUESymbol::initialize();
  }

//---------------------------------------------------------------------------------------

FAppInfo::~FAppInfo()
  {
  SkUESymbol::deinitialize();
  SkookumScript::set_app_info(nullptr);
  AgogCore::deinitialize();
  }

//---------------------------------------------------------------------------------------

void * FAppInfo::malloc(size_t size, const char * debug_name_p)
  {
  return size ? FMemory::Malloc(size, 16) : nullptr; // $Revisit - MBreyer Make alignment controllable by caller
  }

//---------------------------------------------------------------------------------------

void FAppInfo::free(void * mem_p)
  {
  if (mem_p) FMemory::Free(mem_p); // $Revisit - MBreyer Make alignment controllable by caller
  }

//---------------------------------------------------------------------------------------

uint32_t FAppInfo::request_byte_size(uint32_t size_requested)
  {
  // Since we call the 16-byte aligned allocator
  return a_align_up(size_requested, 16);
  }

//---------------------------------------------------------------------------------------

bool FAppInfo::is_using_fixed_size_pools()
  {
  return false;
  }

//---------------------------------------------------------------------------------------

void FAppInfo::debug_print(const char * cstr_p)
  {
  /*
  // Strip LF from start and end to prevent unnecessary gaps in log
  FString msg(cstr_p);
  msg.RemoveFromEnd(TEXT("\n"));
  msg.RemoveFromEnd(TEXT("\n"));
  msg.RemoveFromStart(TEXT("\n"));
  msg.RemoveFromStart(TEXT("\n"));
  UE_LOG(LogSkookum, Log, TEXT("%s"), *msg);
  */

#if WITH_EDITOR
  if (GLogConsole && IsRunningCommandlet())
    {
    FString message(cstr_p);
    GLogConsole->Serialize(*message, ELogVerbosity::Display, FName(TEXT("SkookumScript")));
    }
  else
#endif
    {
    ADebug::print_std(cstr_p);
    }
  }

//---------------------------------------------------------------------------------------

AErrorOutputBase * FAppInfo::on_error_pre(bool nested)
  {
  static ASimpleErrorOutput s_simple_err_out;
  return &s_simple_err_out;
  }

//---------------------------------------------------------------------------------------

void FAppInfo::on_error_post(eAErrAction action)
  {
  // Depending on action could switch back to fullscreen
  }

//---------------------------------------------------------------------------------------

void FAppInfo::on_error_quit()
  {
  exit(EXIT_FAILURE);
  }

//---------------------------------------------------------------------------------------

bool FAppInfo::use_builtin_actor() const
  {
  return false;
  }

//---------------------------------------------------------------------------------------

ASymbol FAppInfo::get_custom_actor_class_name() const
  {
  return ASymbol_Actor;
  }


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// FSkookumScriptRuntime
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//---------------------------------------------------------------------------------------
FSkookumScriptRuntime::FSkookumScriptRuntime()
  : m_is_skookum_disabled(false)
#if WITH_EDITORONLY_DATA
  , m_generator(this)
#endif
#ifdef SKOOKUM_REMOTE_UNREAL
#if WITH_EDITORONLY_DATA 
  , m_remote_client(&m_generator)
  , m_freshen_binaries_requested(true)
#else
  , m_remote_client(nullptr)
  , m_freshen_binaries_requested(false) // With cooked data, load binaries immediately and do not freshen
#endif
#endif
  , m_game_world_p(nullptr)
  , m_editor_world_p(nullptr)
  , m_num_game_worlds(0)
  {
  //m_runtime.set_compiled_path("Scripts" SK_BITS_ID "\\");

  #if WITH_EDITORONLY_DATA 
    SkUEClassBindingHelper::set_runtime_generator(&m_generator);
  #endif
  }

//---------------------------------------------------------------------------------------

FSkookumScriptRuntime::~FSkookumScriptRuntime()
  {
  #if WITH_EDITORONLY_DATA 
    SkUEClassBindingHelper::set_runtime_generator(nullptr);
  #endif
  }

//---------------------------------------------------------------------------------------
// This code will execute after your module is loaded into memory (but after global
// variables are initialized, of course.)
void FSkookumScriptRuntime::StartupModule()
  {
  #if WITH_EDITORONLY_DATA
    // In editor builds, don't activate SkookumScript if there's no project (project wizard mode)
    if (!FApp::GetGameName() || !FApp::GetGameName()[0] || FPlatformString::Strcmp(FApp::GetGameName(), TEXT("None")) == 0)
      {
      m_is_skookum_disabled = true;
      return;
      }
  #else
    // In cooked builds, stay inert when there's no compiled binaries
    if (!m_runtime.is_binary_hierarchy_existing())
      {
      m_is_skookum_disabled = true;
      return;
      }
  #endif

  A_DPRINT("Starting up SkookumScript plug-in modules\n");

  // Note that FWorldDelegates::OnPostWorldCreation has world_p->WorldType set to None
  // Note that FWorldDelegates::OnPreWorldFinishDestroy has world_p->GetName() set to "None"
  m_on_post_engine_init_handle  = UEngine::OnPostEngineInit.AddRaw(this, &FSkookumScriptRuntime::on_post_engine_init);
  m_on_world_init_pre_handle    = FWorldDelegates::OnPreWorldInitialization.AddRaw(this, &FSkookumScriptRuntime::on_world_init_pre);
  m_on_world_init_post_handle   = FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FSkookumScriptRuntime::on_world_init_post);
  m_on_world_cleanup_handle     = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FSkookumScriptRuntime::on_world_cleanup);

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Start up SkookumScript
  // Originally, the compiled binaries were loaded with a delay when in UE4Editor to provide the user with a smoother startup sequence
  // However this caused issues with the proper initialization of Skookum Blueprint nodes
  // So to avoid glitches, SkookumScript is always initialized right away right here
  ensure_runtime_initialized();

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Send off connect request to IDE
  // Come back later to check on it
  #ifdef SKOOKUM_REMOTE_UNREAL
    if (!IsRunningCommandlet())
      {
      m_remote_client.set_mode(SkLocale_runtime);
      }
  #endif
  }

//---------------------------------------------------------------------------------------
// Called after the module has been reloaded
/*
void FSkookumScriptRuntime::PostLoadCallback()
  {
  A_DPRINT(A_SOURCE_STR " SkookumScript - loaded.\n");
  }
*/

//---------------------------------------------------------------------------------------
void FSkookumScriptRuntime::on_post_engine_init()
  {
  #if !WITH_EDITOR
    // If WITH_EDITOR is enabled, reexpose_all_to_blueprints() happens in on_editor_map_opened()
    if (m_runtime.is_compiled_scripts_loaded())
      {
      // Expose to blueprints once more
      // So exposed Sk routines can be attached to more derived static classes if applicable
      // We do this here as the engine needs to be present for the graph nodes to be properly rebuilt
      m_runtime.reexpose_all_to_blueprints(true);
      }
  #endif
  }

//---------------------------------------------------------------------------------------
void FSkookumScriptRuntime::on_world_init_pre(UWorld * world_p, const UWorld::InitializationValues init_vals)
  {
  //A_DPRINT("on_world_init_pre: %S %p\n", *world_p->GetName(), world_p);

  // Make sure atomics are bound by now
  if (m_runtime.is_compiled_scripts_loaded() && !m_runtime.is_compiled_scripts_bound())
    {
    m_runtime.bind_compiled_scripts();
    }

  // Use this callback as an opportunity to take care of connecting to the IDE
  #ifdef SKOOKUM_REMOTE_UNREAL
    if (!IsRunningCommandlet() && !m_remote_client.is_authenticated())
      {
      m_remote_client.attempt_connect(0.0, true, true);
      }
  #endif  

  if (world_p->IsGameWorld())
    {
    // Keep track of how many game worlds we got
    ++m_num_game_worlds;

    if (!m_game_world_p)
      {
      m_game_world_p = world_p;
      if (is_skookum_initialized())
        {
        SkUEClassBindingHelper::set_world(world_p);
        SkookumScript::initialize_gameplay();
        }
      m_game_tick_handle = world_p->OnTickDispatch().AddRaw(this, &FSkookumScriptRuntime::tick_game);
      }
    }
  else if (world_p->WorldType == EWorldType::Editor)
    {
    if (!m_editor_world_p)
      {
      m_editor_world_p = world_p;
      m_editor_tick_handle = world_p->OnTickDispatch().AddRaw(this, &FSkookumScriptRuntime::tick_editor);
      }
    }
  }

//---------------------------------------------------------------------------------------
void FSkookumScriptRuntime::on_world_init_post(UWorld * world_p, const UWorld::InitializationValues init_vals)
  {
  //A_DPRINT("on_world_init_post: %S %p\n", *world_p->GetName(), world_p);

  #if !WITH_EDITORONLY_DATA
    // Resolve raw data for all classes if a callback function is given
    // $Revisit MBreyer this gets called several times (so in cooked builds everything gets resolved) - fix so it's called only once
    SkBrain::ms_object_class_p->resolve_raw_data_recurse();
  #endif

  #if defined(SKOOKUM_REMOTE_UNREAL)
    if (world_p->IsGameWorld())
      {
      SkUERemote::ms_client_p->ensure_connected(5.0);
      }
  #endif
  }

//---------------------------------------------------------------------------------------
void FSkookumScriptRuntime::on_world_cleanup(UWorld * world_p, bool session_ended_b, bool cleanup_resources_b)
  {
  //A_DPRINT("on_world_cleanup: %S %p\n", *world_p->GetName(), world_p);

  if (world_p->IsGameWorld())
    {
    // Keep track of how many game worlds we got
    --m_num_game_worlds;

    // Set world pointer to null if it was pointing to us
    if (m_game_world_p == world_p)
      {
      m_game_world_p->OnTickDispatch().Remove(m_game_tick_handle);
      m_game_world_p = nullptr;
      SkUEClassBindingHelper::set_world(nullptr);
      }

    // Restart SkookumScript if initialized
    if (m_num_game_worlds == 0 && is_skookum_initialized())
      {
      // Simple shutdown
      //SkookumScript::get_world()->clear_coroutines();
      A_DPRINT(
        "SkookumScript resetting session...\n"
        "  cleaning up...\n");
      SkookumScript::deinitialize_gameplay();
      SkookumScript::deinitialize_sim();
      SkookumScript::initialize_sim();
      A_DPRINT("  ...done!\n\n");
      }
    }
  else if (world_p->WorldType == EWorldType::Editor)
    {
    // Set world pointer to null if it was pointing to us
    if (m_editor_world_p == world_p)
      {
      m_editor_world_p->OnTickDispatch().Remove(m_editor_tick_handle);
      m_editor_world_p = nullptr;
      }
    }
  }

//---------------------------------------------------------------------------------------
// Called before the module has been unloaded
/*
void FSkookumScriptRuntime::PreUnloadCallback()
  {
  A_DPRINT(A_SOURCE_STR " SkookumScript - about to unload.\n");
  }
*/

//---------------------------------------------------------------------------------------
// This function may be called during shutdown to clean up your module.  For modules that
// support dynamic reloading, we call this function before unloading the module.
void FSkookumScriptRuntime::ShutdownModule()
  {
  // Don't do anything if SkookumScript is not active
  if (m_is_skookum_disabled)
    {
    return;
    }

  // Printing during shutdown will re-launch IDE in case it has been closed prior to UE4
  // So quick fix is to just not print during shutdown
  //A_DPRINT(A_SOURCE_STR " Shutting down SkookumScript plug-in modules\n");

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Clean up SkookumScript
  m_runtime.shutdown();

  #ifdef SKOOKUM_REMOTE_UNREAL
    // Remote communication to and from SkookumScript IDE
    m_remote_client.disconnect();
  #endif

  // Clear out our registered delegates
  UEngine::OnPostEngineInit.Remove(m_on_post_engine_init_handle);
  FWorldDelegates::OnPreWorldInitialization.Remove(m_on_world_init_pre_handle);
  FWorldDelegates::OnPostWorldInitialization.Remove(m_on_world_init_post_handle);
  FWorldDelegates::OnWorldCleanup.Remove(m_on_world_cleanup_handle);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntime::set_project_generated_bindings(SkUEBindingsInterface * project_generated_bindings_p)
  {
  // If we got bindings, make sure things are initialized
  if (project_generated_bindings_p)
    {
    // Have we had any game bindings before?
    if (m_runtime.have_game_module())
      {
      // Yes, this is a hot reload of the game DLL which just has been recompiled, and scripts have been regenerated by UHT
      // so recompile and reload the binaries
      compile_and_load_binaries();
      }
    else
      {
      // No, this is the first time bindings are set
      ensure_runtime_initialized();
      }
    }

  // Make sure the SkookumScriptEditor module is loaded at this point as it might need to update blueprint classes and recompile blueprints with errors
  #if WITH_EDITORONLY_DATA
    FModuleManager::GetModulePtr<IModuleInterface>("SkookumScriptEditor");
  #endif

  // Now that binaries are loaded, point to the bindings to use
  m_runtime.set_project_generated_bindings(project_generated_bindings_p);
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptRuntime::is_skookum_initialized() const
  {
  return (SkookumScript::get_initialization_level() >= SkookumScript::InitializationLevel_program);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntime::ensure_runtime_initialized()
  {
  if (!m_runtime.is_initialized())
    {
    m_runtime.startup();
    compile_and_load_binaries();
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptRuntime::compile_and_load_binaries()
  {
  #ifdef SKOOKUM_REMOTE_UNREAL
    // At this point, have zero patience with the IDE and launch it if not connected
    if (!IsRunningCommandlet())
      {
      // At this point, wait if necessary to make sure we are connected
      m_remote_client.ensure_connected(0.0);

      // Alert user in case we are still not connected - and allow for corrective measures
      bool load_binaries = true;
      #if WITH_EDITOR
        while (!m_remote_client.is_authenticated())
          {
          FText title = FText::FromString(TEXT("SkookumScript UE4 Plugin cannot connect to the SkookumIDE!"));
          EAppReturnType::Type decision = FMessageDialog::Open(
            EAppMsgType::CancelRetryContinue,
            FText::Format(FText::FromString(TEXT(
              "The SkookumScript UE4 Plugin cannot connect to the SkookumIDE. A connection to the SkookumIDE is required to properly work with SkookumScript.\n\n"
              "The connection problem could be caused by any of the following situations:\n"
              "- The SkookumIDE application is not running. If this is the case, your security software (Virus checker, VPN, Firewall) may have blocked or even removed it. If so, allow SkookumIDE.exe to run, then click 'Retry'. "
              "You can also try to launch the IDE manually. It should be located at the following path: {0}. Once running, click 'Retry'.\n"
              "- The SkookumIDE application is running, but stuck on an error. If so, try to resolve the error, and when the SkookumIDE is back up, click 'Retry'.\n"
              "- The SkookumIDE application is running and seems to be working fine. "
              "If so, the IP and port that the SkookumScript UE4 Plugin is trying to connect to ({1}) might be different from the IP and port that the SkookumIDE is listening to (see SkookumIDE log window), or blocked by a firewall. "
              "These problems could be due to your networking environment, such as a custom firewall, virtualization software such as VirtualBox, or multiple network adapters.\n\n"
              "For additional information including how to specify the SkookumIDE address for the runtime, please see http://skookumscript.com/docs/v3.0/ide/ip-addresses/ and ensure 'Settings'->'Remote runtimes' on the SkookumIDE is set properly.\n\n"
              "If you are having difficulties resolving this issue, please don't hesitate to ask us for help at the SkookumScript Forum (http://forum.skookumscript.com). We are here to make your experience skookum!\n")), 
              FText::FromString(FPaths::ConvertRelativePathToFull(IPluginManager::Get().FindPlugin(TEXT("SkookumScript"))->GetBaseDir() / TEXT("SkookumIDE") / TEXT("SkookumIDE.exe"))),
              FText::FromString(m_remote_client.get_ip_address_ide()->ToString(true))),
            &title);
          if (decision != EAppReturnType::Retry)
            {
            load_binaries = (decision == EAppReturnType::Continue);
            break;
            }
          m_remote_client.ensure_connected(10.0);
          }
      #endif

      if (load_binaries && m_remote_client.is_authenticated())
        {
      #if WITH_EDITOR
        RetryCompilation:
      #endif
        // Block while binaries are being recompiled
        m_remote_client.cmd_compiled_state(true);
        m_freshen_binaries_requested = false; // Request satisfied
        while (!m_remote_client.is_load_compiled_binaries_requested()
            && !m_remote_client.is_compiled_binaries_have_errors())
          {
          m_remote_client.wait_for_update();
          }
        #if WITH_EDITOR
          if (m_remote_client.is_compiled_binaries_have_errors())
            {
            FText title = FText::FromString(TEXT("Compilation errors!"));
            EAppReturnType::Type decision = FMessageDialog::Open(
              EAppMsgType::CancelRetryContinue,
              FText::FromString(TEXT(
                "The SkookumScript compiled binaries could not be generated because errors were found in the script files.\n\n")),
              &title);
            if (decision == EAppReturnType::Retry)
              {
              m_remote_client.clear_load_compiled_binaries_requested();
              goto RetryCompilation;
              }
            load_binaries = (decision == EAppReturnType::Continue);
            }
        #endif
        m_remote_client.clear_load_compiled_binaries_requested();
        }

      if (load_binaries)
        {
        // Attempt to load binaries at this point
        bool success_b = m_runtime.load_compiled_scripts();
        if (success_b)
          {
          // Inform the IDE about the version we got
          m_remote_client.cmd_incremental_update_reply(true, SkBrain::ms_session_guid, SkBrain::ms_revision);
          }
        else
          {
          // Something went wrong - let the user know
          FText title = FText::FromString(TEXT("Unable to load SkookumScript compiled binaries!"));
          FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::FromString(TEXT(
              "Unable to load the compiled binaries. This is most likely caused by errors in the script files which prevented a successful compilation. The project will continue to load with SkookumScript temporarily disabled.")),
            &title);
          }
        }
      }
    else
  #endif
      {
      // If no remote connection, or commandlet mode, load binaries at this point
      bool success_b = m_runtime.load_compiled_scripts();
      SK_ASSERTX(success_b, AErrMsg("Unable to load SkookumScript compiled binaries!", AErrLevel_notify));
      }
  }

//---------------------------------------------------------------------------------------
// Update SkookumScript in game
//
// #Params:
//   deltaTime: Game time passed since the last call.
void FSkookumScriptRuntime::tick_game(float deltaTime)
  {
  #ifdef SKOOKUM_REMOTE_UNREAL
    tick_remote();
  #endif

  // When paused, set deltaTime to 0.0
  #if WITH_EDITOR
    if (!m_game_world_p->IsPaused())
  #endif
      {
      SCOPE_CYCLE_COUNTER(STAT_SkookumScriptTime);
      m_runtime.update(deltaTime);
      }
  }

//---------------------------------------------------------------------------------------
// Update SkookumScript in editor
//
// #Params:
//   deltaTime: Game time passed since the last call.
void FSkookumScriptRuntime::tick_editor(float deltaTime)
  {
  #ifdef SKOOKUM_REMOTE_UNREAL
    if (!m_game_world_p)
      {
      tick_remote();
      }
  #endif
  }

#ifdef SKOOKUM_REMOTE_UNREAL

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::tick_remote()
  {
  if (!IsRunningCommandlet())
    {
    // Request recompilation of binaries if script files changed
    if (m_freshen_binaries_requested)
      {
      m_remote_client.cmd_compiled_state(true);
      m_freshen_binaries_requested = false;
      }

    // Remote communication to and from SkookumScript IDE.
    // Needs to be called whether in editor or game and whether paused or not
    // $Revisit - CReis This is probably a hack. The remote client update should probably
    // live somewhere other than a tick method such as its own thread.
    m_remote_client.process_incoming();

    // Re-load compiled binaries?
    // If the game is currently running, delay until it's not
    if (m_remote_client.is_load_compiled_binaries_requested() 
     && SkookumScript::get_initialization_level() < SkookumScript::InitializationLevel_gameplay)
      {
      // Makes sure the SkookumScript runtime object is initialized at this point
      ensure_runtime_initialized();

      // Load the Skookum class hierarchy scripts in compiled binary form
      bool is_first_time = !is_skookum_initialized();

      bool success_b = m_runtime.load_and_bind_compiled_scripts();
      SK_ASSERTX(success_b, AErrMsg("Unable to load SkookumScript compiled binaries!", AErrLevel_notify));
      m_remote_client.clear_load_compiled_binaries_requested();

      // After reloading, re-resolve the raw data of all dynamic classes
      #if WITH_EDITORONLY_DATA
        TArray<UObject*> blueprint_array;
        GetObjectsOfClass(UBlueprint::StaticClass(), blueprint_array, true, RF_ClassDefaultObject);
        for (UObject * obj_p : blueprint_array)
          {
          UBlueprint * blueprint_p = static_cast<UBlueprint *>(obj_p);
          if (blueprint_p->GeneratedClass)
            {
            SkClass * sk_class_p = SkUEClassBindingHelper::get_sk_class_from_ue_class(blueprint_p->GeneratedClass);
            if (sk_class_p)
              {
              SkUEClassBindingHelper::resolve_raw_data(sk_class_p, blueprint_p->GeneratedClass);
              }
            }
          }
      #endif

      if (is_first_time && is_skookum_initialized())
        {
        #if WITH_EDITOR
          // Recompile Blueprints in error state as such error state might have been due to SkookumScript not being initialized at the time of compile
          if (m_runtime.get_editor_interface())
            {
            m_runtime.get_editor_interface()->recompile_blueprints_with_errors();
            }
        #endif
        }
      }
    }
  }

#endif

//---------------------------------------------------------------------------------------

bool FSkookumScriptRuntime::is_skookum_disabled() const
  {
  return m_is_skookum_disabled;
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_freshen_binaries_pending() const
  {
  #ifdef SKOOKUM_REMOTE_UNREAL
    return m_freshen_binaries_requested;
  #else
    return false;
  #endif
  }

#if WITH_EDITOR

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::set_editor_interface(ISkookumScriptRuntimeEditorInterface * editor_interface_p)
  {
  m_runtime.set_editor_interface(editor_interface_p);
  #ifdef SKOOKUM_REMOTE_UNREAL
    m_remote_client.set_editor_interface(editor_interface_p);
  #endif
  }

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::on_editor_map_opened()
  {
  // Regenerate them all just to be sure
  m_generator.generate_all_class_script_files();

  // When editor is present, initialize Sk here
  ensure_runtime_initialized();

  if (m_runtime.is_compiled_scripts_loaded())
    {
    // Expose to blueprints once more
    // So exposed Sk routines can be attached to more derived static classes if applicable
    // We do this here as the engine needs to be present for the graph nodes to be properly rebuilt
    m_runtime.reexpose_all_to_blueprints(true);
    }
  }

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::show_ide(const FString & focus_class_name, const FString & focus_member_name, bool is_data_member, bool is_class_member)
  {
  #ifdef SKOOKUM_REMOTE_UNREAL
    // Remove qualifier from member name if present
    FString focus_class_name_ide = focus_class_name;
    FString focus_member_name_ide = focus_member_name;
    int32 at_pos = 0;
    if (focus_member_name_ide.FindChar('@', at_pos))
      {
      focus_class_name_ide = focus_member_name_ide.Left(at_pos).TrimTrailing();
      focus_member_name_ide = focus_member_name_ide.Mid(at_pos + 1).Trim();
      }

    // Convert to symbols and send off
    ASymbol focus_class_name_sym(ASymbol::create_existing(FStringToAString(focus_class_name_ide)));
    ASymbol focus_member_name_sym(ASymbol::create_existing(FStringToAString(focus_member_name_ide)));
    m_remote_client.cmd_show(AFlag_on, focus_class_name_sym, focus_member_name_sym, is_data_member, is_class_member);
  #endif
  }

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::freshen_compiled_binaries_if_have_errors()
  {
  #ifdef SKOOKUM_REMOTE_UNREAL
    if (m_remote_client.is_compiled_binaries_have_errors())
      {
      m_freshen_binaries_requested = true;
      }
  #endif
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::has_skookum_default_constructor(UClass * class_p) const
  {
  SK_ASSERTX(m_runtime.is_initialized(), "Runtime must be initialized for this code to work.");

  SkClass * sk_class_p = SkUEClassBindingHelper::get_sk_class_from_ue_class(class_p);
  if (sk_class_p)
    {
    return (sk_class_p->find_instance_method_inherited(ASymbolX_ctor) != nullptr);
    }

  return false;
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::has_skookum_destructor(UClass * class_p) const
  {
  SK_ASSERTX(m_runtime.is_initialized(), "Runtime must be initialized for this code to work.");

  SkClass * sk_class_p = SkUEClassBindingHelper::get_sk_class_from_ue_class(class_p);
  if (sk_class_p)
    {
    return (sk_class_p->find_instance_method_inherited(ASymbolX_dtor) != nullptr);
    }

  return false;
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_skookum_class_data_component_class(UClass * class_p) const
  {
  return class_p->IsChildOf(USkookumScriptClassDataComponent::StaticClass());
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_skookum_behavior_component_class(UClass * class_p) const
  {
  return class_p->IsChildOf(USkookumScriptBehaviorComponent::StaticClass());
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_skookum_blueprint_function(UFunction * function_p) const
  {
  return m_runtime.get_blueprint_interface()->is_skookum_blueprint_function(function_p);
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_skookum_blueprint_event(UFunction * function_p) const
  {
  return m_runtime.get_blueprint_interface()->is_skookum_blueprint_event(function_p);
  }

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::on_class_added_or_modified(UClass * ue_class_p, bool check_if_reparented)
  {
  // Generate script files for the new/changed class
  m_generator.generate_class_script_files(ue_class_p, true, true, check_if_reparented);
  m_generator.generate_used_class_script_files();

  // Re-resolve the raw data if applicable
  SkClass * sk_class_p = SkUEClassBindingHelper::get_sk_class_from_ue_class(ue_class_p);
  if (sk_class_p)
    {
    SkUEClassBindingHelper::resolve_raw_data(sk_class_p, ue_class_p);
    }
  }

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::on_class_renamed(UClass * ue_class_p, const FString & old_class_name)
  {
  m_generator.rename_class_script_files(ue_class_p, old_class_name);
  }

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::on_class_renamed(UClass * ue_class_p, const FString & old_class_name, const FString & new_class_name)
  {
  m_generator.rename_class_script_files(ue_class_p, old_class_name, new_class_name);
  }

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::on_class_deleted(UClass * ue_class_p)
  {
  m_generator.delete_class_script_files(ue_class_p);
  }

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_static_class_known_to_skookum(UClass * class_p) const
  {
  m_runtime.ensure_static_ue_types_registered();
  return SkUEClassBindingHelper::is_static_class_registered(class_p);
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_static_struct_known_to_skookum(UStruct * struct_p) const
  {
  m_runtime.ensure_static_ue_types_registered();
  return SkUEClassBindingHelper::is_static_struct_registered(struct_p);
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_static_enum_known_to_skookum(UEnum * enum_p) const
  {
  m_runtime.ensure_static_ue_types_registered();
  return SkUEClassBindingHelper::is_static_enum_registered(enum_p);
  }

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::on_class_scripts_changed_by_generator(const FString & class_name, eChangeType change_type)
  {
  #ifdef SKOOKUM_REMOTE_UNREAL
    m_freshen_binaries_requested = true;
  #endif
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::check_out_file(const FString & file_path) const
  {
  if (!m_runtime.get_editor_interface())
    {
    return false;
    }

  return m_runtime.get_editor_interface()->check_out_file(file_path);
  }

#endif // WITH_EDITORONLY_DATA

//#pragma optimize("g", on)
