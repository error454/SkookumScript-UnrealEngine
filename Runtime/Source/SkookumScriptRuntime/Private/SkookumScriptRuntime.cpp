//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Main entry point for the SkookumScript runtime plugin
// 
// Author: Conan Reis
//=======================================================================================

#include "SkookumScriptRuntimePrivatePCH.h"
#include "Bindings/SkUEBindings.hpp"
#include "Bindings/SkUERuntime.hpp"
#include "Bindings/SkUERemote.hpp"
#include "Bindings/SkUEBlueprintInterface.hpp"

#include "Runtime/Launch/Resources/Version.h"
#include "Engine/World.h"
#include "Stats.h"

#include "../Classes/SkookumScriptComponent.h"

#include <SkUEWorld.generated.hpp>

#ifdef A_PLAT_PC
  #include <windows.h>  // Uses: IsDebuggerPresent(), OutputDebugStringA()
#endif

// For profiling SkookumScript performance
DECLARE_CYCLE_STAT(TEXT("SkookumScript Time"), STAT_SkookumScriptTime, STATGROUP_Game);

//---------------------------------------------------------------------------------------
class FSkookumScriptRuntime : public ISkookumScriptRuntime
  {
  public:

    FSkookumScriptRuntime();

  protected:

  // Methods

    // Overridden from IModuleInterface

    virtual void    StartupModule() override;
    //virtual void    PostLoadCallback() override;
    //virtual void    PreUnloadCallback() override;
    virtual void    ShutdownModule() override;

    // Overridden from ISkookumScriptRuntime

    virtual void  startup_skookum() override;
    virtual bool  is_skookum_initialized() const override;
    virtual bool  is_freshen_binaries_pending() const override;

    #if WITH_EDITOR

      virtual void  set_editor_interface(ISkookumScriptRuntimeEditorInterface * editor_interface_p) override;

      virtual void  on_editor_map_opened() override;
      virtual void  on_class_scripts_changed_by_editor(const FString & class_name, eChangeType change_type) override;
      virtual void  show_ide(const FString & focus_class_name, const FString & focus_member_name, bool is_data_member, bool is_class_member) override;
      virtual void  freshen_compiled_binaries_if_have_errors() override;

      virtual bool  is_static_class_known_to_skookum(UClass * class_p) const override;
      virtual bool  is_static_struct_known_to_skookum(UStruct * struct_p) const override;
      virtual bool  is_static_enum_known_to_skookum(UEnum * enum_p) const override;
      virtual bool  has_skookum_default_constructor(UClass * class_p) const override;
      virtual bool  has_skookum_destructor(UClass * class_p) const override;
      virtual bool  is_skookum_component_class(UClass * class_p) const override;
      virtual bool  is_skookum_blueprint_function(UFunction * function_p) const override;
      virtual bool  is_skookum_blueprint_event(UFunction * function_p) const override;

    #endif

    // Local methods

    void          tick_game(float deltaTime);
    void          tick_editor(float deltaTime);

    #ifdef SKOOKUM_REMOTE_UNREAL
      void        tick_remote();
    #endif

    void          on_world_init_pre(UWorld * world_p, const UWorld::InitializationValues init_vals);
    void          on_world_init_post(UWorld * world_p, const UWorld::InitializationValues init_vals);
    void          on_world_cleanup(UWorld * world_p, bool session_ended_b, bool cleanup_resources_b);

  // Data Members

    SkUERuntime       m_runtime;

    #ifdef SKOOKUM_REMOTE_UNREAL
      SkUERemote      m_remote_client;
      bool            m_freshen_binaries_requested;
    #endif

    UWorld *          m_game_world_p;
    UWorld *          m_editor_world_p;

    FDelegateHandle   m_on_world_init_pre_handle;
    FDelegateHandle   m_on_world_init_post_handle;
    FDelegateHandle   m_on_world_cleanup_handle;

    FDelegateHandle   m_game_tick_handle;
    FDelegateHandle   m_editor_tick_handle;

  };


//---------------------------------------------------------------------------------------
// Simple error dialog until more sophisticated one in place.
// Could communicate remotely with Skookum IDE and have it bring up message window.
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
// Memory Allocation with Descriptions
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// These must be defined because A_MEMORY_FUNCS_PRESENT is set in AgogCore/AgogExtHook.hpp

// Note that the new/delete allocation functions without description arguments are defined
// in the IMPLEMENT_MODULE macro above:
//   ```
//   void * operator new(size_t Size);
//   void * operator new[](size_t Size);
//   void   operator delete(void * Ptr);
//   void   operator delete[](void * Ptr);
//   ```

//---------------------------------------------------------------------------------------
void * operator new(size_t size, const char * desc_cstr_p)
  {
  return FMemory::Malloc(size, 16); // $Revisit - MBreyer Make alignment controllable by caller
  }

//---------------------------------------------------------------------------------------
void * operator new[](size_t size, const char * desc_cstr_p)
  {
  return FMemory::Malloc(size, 16); // $Revisit - MBreyer Make alignment controllable by caller
  }

// $Note - *** delete operators with additional arguments cannot be called explicitly
// - they are only called on class destruction on a paired new with similar arguments.
// This means that a simple delete call will often call the single argument operator
// delete(void * mem_p) so take care to ensure that the proper delete is paired with a
// corresponding new.

//---------------------------------------------------------------------------------------
void operator delete(void * buffer_p, const char * desc_cstr_p)
  {
  FMemory::Free(buffer_p);
  }

//---------------------------------------------------------------------------------------
void operator delete[](void * buffer_p, const char * desc_cstr_p)
  {
  FMemory::Free(buffer_p);
  }


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Agog Namespace Functions
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

namespace Agog
  {

  //---------------------------------------------------------------------------------------
  // Description Returns constants / values used by the AgogCore library.
  // Returns     constants / values used by the AgogCore library.
  // Examples    Called internally
  // Author(s)   Conan Reis
  AgogCoreVals & get_agog_core_vals()
    {
    static AgogCoreVals s_values;

    //if (s_values.m_using_defaults)
    //  {
    //  // Set custom initial values
    //  s_values.m_using_defaults = false;
    //  }

    return s_values;
    }

  //---------------------------------------------------------------------------------------
  // Prints supplied C-string to debug console - which can be a debugger window, standard
  // out, something custom in the app, etc.
  // 
  // #Notes
  //   Called by ADebug print functions & A_DPRINT() which call the ADebug print functions.
  //   
  // #See Also
  //   ADebug print functions which all call this function.
  //   Also the functions ADebug::register_print_*() can register additional print
  //   functions.
  void dprint(const char * cstr_p)
    {
    #ifdef A_PLAT_PC
      if (FPlatformMisc::IsDebuggerPresent())
        {
        // Note that Unicode version OutputDebugStringW() actually calls OutputDebugStringA()
        // so calling it directly is faster.
        ::OutputDebugStringA(cstr_p);
        }
      //else
    #endif
      //  {
      //  ADebug::print_std(cstr_p);
      //  }
    }

  //---------------------------------------------------------------------------------------
  // Description Called whenever an error occurs but *before* a choice has been made as to
  //             how it should be resolved.  It optionally creates an error output object
  //             that will have its determine_choice() called if 'nested' is false.
  // Returns     an AErrorOutputBase object to call determine_choice() on or NULL if a
  //             default resolve error choice is to be made without prompting the user with
  //             output to the debug output window.
  // Arg         nested - Indicates whether the error is nested inside another error - i.e.
  //             an additional error happened before a prior error was fully resolved
  //             (while unwinding the stack on a 'continue' exception throw for example).
  //             determine_choice() will *not* be called if 'nested' is true.
  // Examples    Called by ADebug class - before on_error_post() and on_error_quit()
  // See Also    ADebug, AErrPopUp, AErrorDialog
  // Author(s)   Conan Reis
  AErrorOutputBase * on_error_pre(bool nested)
    {
    static ASimpleErrorOutput s_simple_err_out;

    return &s_simple_err_out;
    }

  //---------------------------------------------------------------------------------------
  // Description Called whenever an error occurs and *after* a choice has been made as to
  //             how it should be resolved.
  // Arg         action - the action that will be taken to attempt resolve the error.
  // Examples    Called by ADebug class - after on_error_pre() and before on_error_quit()
  // Author(s)   Conan Reis
  void on_error_post(eAErrAction action)
    {
    // Depending on action could switch back to fullscreen
    }

  //---------------------------------------------------------------------------------------
  // Description Called if 'Quit' is chosen during error.
  // Examples    Called by ADebug class - after on_error_pre() and on_error_post()
  // Author(s)   Conan Reis
  void on_error_quit()
    {
    //AApplication::shut_down();
    
    exit(EXIT_FAILURE);
    }

  //---------------------------------------------------------------------------------------
  void * malloc_func(size_t size, const char * name_p)
    {
    return size ? FMemory::Malloc(size, 16) : nullptr; // $Revisit - MBreyer Make alignment controllable by caller
    }

  //---------------------------------------------------------------------------------------
  void free_func(void * mem_p)
    {
    if (mem_p) FMemory::Free(mem_p); // $Revisit - MBreyer Make alignment controllable by caller
    }

  //---------------------------------------------------------------------------------------
  // Converts the size requested to allocate to the actual amount allocated.
  uint32_t req_byte_size_func(uint32_t bytes_requested)
    {
    // Assume 1:1 for default
    return bytes_requested;
    }

  }  // End Agog namespace


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Skookum Namespace Functions
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

namespace Skookum
  {

  //---------------------------------------------------------------------------------------
  // Description Returns constants / values used by the SkookumScript library.
  // Returns     constants / values used by the SkookumScript library.
  // Examples    Called internally
  // Author(s)   Conan Reis
  SkookumVals & get_lib_vals()
    {
    static SkookumVals s_values;

    if (s_values.m_using_defaults)
      {
      // Set custom initial values
      s_values.m_using_defaults = false;

      // Unreal uses its own actor class
      s_values.m_use_builtin_actor = false; 
      s_values.m_custom_actor_class_name = "Actor";
      }

    return s_values;
    }

  }


//---------------------------------------------------------------------------------------
FSkookumScriptRuntime::FSkookumScriptRuntime()
  : m_game_world_p(nullptr)
#ifdef SKOOKUM_REMOTE_UNREAL
  , m_freshen_binaries_requested(WITH_EDITORONLY_DATA) // With cooked data, load binaries immediately and do not freshen
#endif
  , m_editor_world_p(nullptr)
  {
  //m_runtime.set_compiled_path("Scripts" SK_BITS_ID "\\");
  }

//---------------------------------------------------------------------------------------
// This code will execute after your module is loaded into memory (but after global
// variables are initialized, of course.)
void FSkookumScriptRuntime::StartupModule()
  {
  // In cooked builds, stay inert when there's no compiled binaries
  #if !WITH_EDITORONLY_DATA
    if (!m_runtime.is_binary_hierarchy_existing())
      {
      return;
      }
  #endif

  A_DPRINT(A_SOURCE_STR " Starting up SkookumScript plug-in modules\n");

  // Note that FWorldDelegates::OnPostWorldCreation has world_p->WorldType set to None
  // Note that FWorldDelegates::OnPreWorldFinishDestroy has world_p->GetName() set to "None"

  m_on_world_init_pre_handle  = FWorldDelegates::OnPreWorldInitialization.AddRaw(this, &FSkookumScriptRuntime::on_world_init_pre);
  m_on_world_init_post_handle = FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FSkookumScriptRuntime::on_world_init_post);
  m_on_world_cleanup_handle   = FWorldDelegates::OnWorldCleanup.AddRaw(this, &FSkookumScriptRuntime::on_world_cleanup);

  // Hook up Unreal memory allocator
  AMemory::override_functions(&Agog::malloc_func, &Agog::free_func, &Agog::req_byte_size_func);

  // Gather and register all UE classes/structs/enums known to SkookumScript
  SkUEBindings::register_static_types();

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Start up SkookumScript
  #if !WITH_EDITOR
    // If no editor, initialize right away
    startup_skookum();
  #endif

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
void FSkookumScriptRuntime::on_world_init_pre(UWorld * world_p, const UWorld::InitializationValues init_vals)
  {
  //A_DPRINT("on_world_init_pre: %S %p\n", *world_p->GetName(), world_p);

  // Use this callback as an opportunity to take care of connecting to the IDE
  #ifdef SKOOKUM_REMOTE_UNREAL
    if (!IsRunningCommandlet() && !m_remote_client.is_authenticated())
      {
      m_remote_client.attempt_connect(0.0, true, true);
      }
  #endif  

  if (world_p->IsGameWorld())
    {
    if (!m_game_world_p)
      {
      m_game_world_p = world_p;
      if (is_skookum_initialized())
        {
        SkUEClassBindingHelper::set_world(world_p);
        SkookumScript::initialize_instances();
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

  if (cleanup_resources_b)
    {
    if (world_p->IsGameWorld())
      {
      // Set world pointer to null if it was pointing to us
      if (m_game_world_p == world_p)
        {
        m_game_world_p->OnTickDispatch().Remove(m_game_tick_handle);
        m_game_world_p = nullptr;
        SkUEClassBindingHelper::set_world(nullptr);
        }

      // Restart SkookumScript if initialized
      if (is_skookum_initialized())
        {
        // Simple shutdown
        //SkookumScript::get_world()->clear_coroutines();
        A_DPRINT(
          "SkookumScript resetting session...\n"
          "  cleaning up...\n");
        SkookumScript::deinitialize_instances();
        SkookumScript::deinitialize_session();
        SkookumScript::initialize_session();
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
  // In cooked builds, stay inert when there's no compiled binaries
  #if !WITH_EDITORONLY_DATA
    if (!m_runtime.is_binary_hierarchy_existing())
      {
      return;
      }
  #endif

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
  FWorldDelegates::OnPreWorldInitialization.Remove(m_on_world_init_pre_handle);
  FWorldDelegates::OnPostWorldInitialization.Remove(m_on_world_init_post_handle);
  FWorldDelegates::OnWorldCleanup.Remove(m_on_world_cleanup_handle);
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
    if (m_remote_client.is_load_compiled_binaries_requested() && !m_game_world_p)
      {
      // Load the Skookum class hierarchy scripts in compiled binary form
      bool is_first_time = !is_skookum_initialized();

      bool success_b = m_runtime.load_compiled_scripts();
      SK_ASSERTX(success_b, AErrMsg("Unable to load SkookumScript compiled binaries!", AErrLevel_notify));
      m_remote_client.clear_load_compiled_binaries_requested();

      if (is_first_time && is_skookum_initialized())
        {
        #if WITH_EDITOR
          // Recompile Blueprints in error state as such error state might have been due to SkookumScript not being initialized at the time of compile
          if (m_runtime.get_editor_interface())
            {
            m_runtime.get_editor_interface()->recompile_blueprints_with_errors();
            }
        #endif

        // Set world pointer variable now that SkookumScript is initialized
        SkUEClassBindingHelper::set_world(m_game_world_p);

        // Set up instances if game is already running
        if (m_game_world_p)
          {
          SkookumScript::initialize_instances();
          }
        }
      }
    }
  }

#endif

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::startup_skookum()
  {
  m_runtime.startup();

#if defined(SKOOKUM_REMOTE_UNREAL) && WITH_EDITORONLY_DATA // Always initialize here if there's no remote connection or cooked build
  if (IsRunningCommandlet())
#endif
    {
    bool success_b = m_runtime.load_compiled_scripts();
    SK_ASSERTX(success_b, AErrMsg("Unable to load SkookumScript compiled binaries!", AErrLevel_notify));
    }
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_skookum_initialized() const
  {
  return SkookumScript::is_flag_set(SkookumScript::Flag_evaluate);
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
  // When editor is present, initialize Sk here
  if (!m_runtime.is_initialized())
    {
    m_runtime.startup();

    #ifdef SKOOKUM_REMOTE_UNREAL
      // At this point, have zero patience with the IDE and launch it if not connected
      if (!IsRunningCommandlet())
        {
        // At this point, wait if necessary to make sure we are connected
        m_remote_client.ensure_connected(0.0);
        if (m_remote_client.is_authenticated())
          {
          // Kick off re-compilation of the binaries
          m_remote_client.cmd_compiled_state(true);
          m_freshen_binaries_requested = false; // Request satisfied
          }
        else
          {
          // If no remote connection, attempt to load binaries at this point
          bool success_b = m_runtime.load_compiled_scripts();
          SK_ASSERTX(success_b, AErrMsg("Unable to load SkookumScript compiled binaries!", AErrLevel_notify));
          }
        }
    #else
      // If no remote connection, load binaries at this point
      bool success_b = m_runtime.load_compiled_scripts();
      SK_ASSERTX(success_b, AErrMsg("Unable to load SkookumScript compiled binaries!", AErrLevel_notify));
    #endif
    }
  }

//---------------------------------------------------------------------------------------
// 
void FSkookumScriptRuntime::on_class_scripts_changed_by_editor(const FString & class_name, eChangeType change_type)
  {
  #ifdef SKOOKUM_REMOTE_UNREAL
    m_freshen_binaries_requested = true;
  #endif
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
bool FSkookumScriptRuntime::is_static_class_known_to_skookum(UClass * class_p) const
  {
  return SkUEClassBindingHelper::is_static_class_registered(class_p);
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_static_struct_known_to_skookum(UStruct * struct_p) const
  {
  return SkUEClassBindingHelper::is_static_struct_registered(struct_p);
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_static_enum_known_to_skookum(UEnum * enum_p) const
  {
  return SkUEClassBindingHelper::is_static_enum_registered(enum_p);
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::has_skookum_default_constructor(UClass * class_p) const
  {
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
  SkClass * sk_class_p = SkUEClassBindingHelper::get_sk_class_from_ue_class(class_p);
  if (sk_class_p)
    {
    return (sk_class_p->find_instance_method_inherited(ASymbolX_dtor) != nullptr);
    }

  return false;
  }

//---------------------------------------------------------------------------------------
// 
bool FSkookumScriptRuntime::is_skookum_component_class(UClass * class_p) const
  {
  return (class_p == USkookumScriptComponent::StaticClass());
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

#endif

//#pragma optimize("g", on)
