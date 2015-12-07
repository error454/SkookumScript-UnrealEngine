//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// SkookumScript Runtime Hooks for Unreal - Input/Output Init/Update/Deinit Manager
// 
// Author: Conan Reis
//=======================================================================================


//=======================================================================================
// Includes
//=======================================================================================

#include "../SkookumScriptRuntimePrivatePCH.h"
#include "SkUERuntime.hpp"
#include "SkUERemote.hpp"
#include "SkUEBindings.hpp"
#include "SkUEClassBinding.hpp"

#include "../../Classes/SkookumScriptComponent.h"

#include "GenericPlatformProcess.h"
#include <chrono>


//=======================================================================================
// Local Global Structures
//=======================================================================================

namespace
{

  //---------------------------------------------------------------------------------------
  // Custom Unreal Binary Handle Structure
  struct SkBinaryHandleUE : public SkBinaryHandle
    {
    // Public Methods

      //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      SkBinaryHandleUE(void * binary_p, uint32_t size)
        {
        m_binary_p = binary_p;
        m_size = size;
        }

      //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      static SkBinaryHandleUE * create(const TCHAR * path_p)
        {
        FArchive * reader_p = IFileManager::Get().CreateFileReader(path_p);

        if (!reader_p)
          {
          return nullptr;
          }
    
        int32   size     = reader_p->TotalSize();
        uint8 * binary_p = (uint8*)FMemory::Malloc(size);

        if (!binary_p)
          {
          reader_p->Close();
          delete reader_p;

          return nullptr;
          }

        reader_p->Serialize(binary_p, size);
        reader_p->Close();
        delete reader_p;

        return new SkBinaryHandleUE(binary_p, size);
        }
    };


} // End unnamed namespace


//=======================================================================================
// SkUERuntime Methods
//=======================================================================================

//---------------------------------------------------------------------------------------
// One-time initialization of SkookumScript
// See Also    shutdown()
// Author(s)   Conan Reis
void SkUERuntime::startup()
  {
  SK_ASSERTX(!m_is_initialized, "Tried to initialize SkUERuntime twice in a row.");

  A_DPRINT("\nSkookumScript starting up.\n");

  // Let scripting system know that the game engine is present and is being hooked-in
  SkDebug::enable_engine_present();

  #ifdef SKOOKUM_REMOTE_UNREAL
    SkDebug::register_print_with_agog();
  #endif

  SkBrain::register_bind_atomics_func(SkookumRuntimeBase::bind_routines);
  SkClass::register_raw_resolve_func(SkUEClassBindingHelper::resolve_raw_data);

  m_is_initialized = true;
  }

//---------------------------------------------------------------------------------------
// One-time shutdown of SkookumScript
void SkUERuntime::shutdown()
  {
  SK_ASSERTX(!SkookumScript::is_flag_set(SkookumScript::Flag_updating), "Attempting to shut down SkookumScript while it is in the middle of an update.");
  SK_ASSERTX(m_is_initialized, "Tried to shut down SkUERuntime without prior initialization.");

  A_DPRINT("\nSkookumScript shutting down.\n");

  #ifdef SKOOKUM_REMOTE_UNREAL
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Disconnect from remote client
    SkookumRemoteBase::ms_default_p->set_mode(SkLocale_embedded);
  #endif

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Clears out Blueprint interface mappings
  SkUEBlueprintInterface::get()->clear();

  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Unloads SkookumScript and cleans-up
  SkookumScript::deinitialize_session();
  SkookumScript::deinitialize();

  m_is_initialized = false;
  }

//---------------------------------------------------------------------------------------
// Override to add bindings to any custom C++ routines (methods & coroutines).
//
// #See Also   SkBrain::register_bind_atomics_func()
// #Modifiers  virtual
// #Author(s)  Conan Reis
void SkUERuntime::on_bind_routines()
  {
  A_DPRINT(A_SOURCE_STR "\nBind routines for SkUERuntime.\n");

  #if WITH_EDITORONLY_DATA
    SkUEClassBindingHelper::reset_dynamic_class_mappings(); // Start over fresh
  #endif

  SkUEBindings::register_all_bindings();
  m_blueprint_interface.reexpose_all(); // Hook up Blueprint functions and events for static classes
  }

//---------------------------------------------------------------------------------------
// Override to run cleanup code before SkookumScript deinitializes its session
void SkUERuntime::on_pre_deinitialize_session()
  {
  }

//---------------------------------------------------------------------------------------
// Determine the compiled file path
//   - usually Content\SkookumScript\Compiled[bits]\Classes.sk-bin
// 
// #Author(s):  Conan Reis
const FString & SkUERuntime::get_compiled_path() const
  {
  if (!m_compiled_file_b)
    {
    m_compiled_file_b = content_file_exists(TEXT("classes.sk-bin"), &m_compiled_path);
    }

  return m_compiled_path;
  }

//---------------------------------------------------------------------------------------
// Determine if a given file name exists in the content/SkookumScript folder of either game or engine
// #Author(s): Markus Breyer
bool SkUERuntime::content_file_exists(const TCHAR * file_name_p, FString * folder_path_p) const
  {
  FString folder_path;

  // Check if we got a game
  FString game_name(FApp::GetGameName());
  if (game_name.IsEmpty())
    {
    // No game, look for default project binaries
    folder_path = FPaths::EngineContentDir() / TEXT("skookumscript") /*TEXT(SK_BITS_ID)*/;
    if (!FPaths::FileExists(folder_path / file_name_p))
      {
      return false;
      }
    }
  else
    {
    // We have a game, so first check if it exists in the game content folder
    folder_path = FPaths::GameContentDir() / TEXT("skookumscript") /*TEXT(SK_BITS_ID)*/;
    if (!FPaths::FileExists(folder_path / file_name_p))
      {
      #if WITH_EDITORONLY_DATA
        // If not found in game, check in temp location
        folder_path = FPaths::GameIntermediateDir() / TEXT("SkookumScript/Content/skookumscript");
        if (!FPaths::FileExists(folder_path / file_name_p))
          {
          return false;
          }
      #else
        return false;
      #endif
      }
    }

  *folder_path_p = folder_path;
  return true;
  }

//---------------------------------------------------------------------------------------
// Load the Skookum class hierarchy scripts in compiled binary form.
// 
// #Params
//   ensure_atomics:
//     If set makes sure all atomic (C++) scripts that were expecting a C++ function to be
//     bound/hooked up to them do actually have a binding.
//   ignore_classes_pp:
//     array of classes to ignore when ensure_atomics is set.
//     This allows some classes with optional or delayed bindings to be skipped such as
//     bindings to a in-game world editor.
//   ignore_count:  number of class pointers in ignore_classes_pp
//
// #Returns
//   true if compiled scrips successfully loaded, false if not
// 
// #See:        load_compiled_class_group(), SkCompiler::compiled_load()
// #Modifiers:  static
// #Author(s):  Conan Reis
bool SkUERuntime::load_compiled_scripts(
  bool       ensure_atomics,     // = true
  SkClass ** ignore_classes_pp,  // = nullptr
  uint32_t   ignore_count        // = 0u
  )
  {
  A_DPRINT("\nSkookumScript loading previously parsed compiled binary...\n");

  if (load_compiled_hierarchy() != SkLoadStatus_ok)
    {
    return false;
    }

  A_DPRINT("  ...done!\n\n");


  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Bind atomics
  A_DPRINT("SkookumScript binding with C++ routines...\n");

  // Registers/connects Generic SkookumScript atomic classes, stimuli, coroutines, etc.
  // with the compiled binary that was just loaded.
  SkookumScript::initialize_post_load();

  #if (SKOOKUM & SK_DEBUG)
    // Ensure atomic (C++) methods/coroutines are properly bound to their C++ equivalents
    if (ensure_atomics)
      {
      SkBrain::ensure_atomics_registered(ignore_classes_pp, ignore_count);
      }
  #endif

  A_DPRINT("  ...done!\n\n");


  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Enable SkookumScript evaluation
  SkookumScript::enable_flag(SkookumScript::Flag_evaluate);

  A_DPRINT("SkookumScript initializing session...\n");
  SkookumScript::initialize_session();
  A_DPRINT("  ...done!\n\n");

  return true;
  }

//---------------------------------------------------------------------------------------
// Determines if binary for class hierarchy and associated info exists.
// 
// This check is done before the symbol file check since the symbol file is only needed
// when debugging and giving an error about missing the compiled binary is more intuitive
// to the end user than a missing symbol file.
// 
// #See Also:   get_binary_hierarchy()
// #Modifiers:  virtual - overridden from SkookumRuntimeBase
// #Author(s):  Conan Reis
bool SkUERuntime::is_binary_hierarchy_existing()
  {
  get_compiled_path();

  return m_compiled_file_b;
  }

//---------------------------------------------------------------------------------------
// Get notified that the compiled binaries moved to a new location
void SkUERuntime::on_binary_hierarchy_path_changed()
  {
  m_compiled_file_b = false;
  }

//---------------------------------------------------------------------------------------
// Gets memory representing binary for class hierarchy and associated info.
// 
// #See Also:   load_compiled_scripts(), SkCompiler::get_binary_class_group()
// #Modifiers:  virtual - overridden from SkookumRuntimeBase
// #Author(s):  Conan Reis
SkBinaryHandle * SkUERuntime::get_binary_hierarchy()
  {
  FString compiled_file = get_compiled_path() / TEXT("classes.sk-bin");

  A_DPRINT("  Loading compiled binary file '%ls'...\n", *compiled_file);

  return SkBinaryHandleUE::create(*compiled_file);
  }

//---------------------------------------------------------------------------------------
// Gets memory representing binary for group of classes with specified class as root.
// Used as a mechanism to "demand load" scripts.
// 
// #See Also:   load_compiled_scripts(), SkCompiler::get_binary_class_group()
// #Modifiers:  virtual - overridden from SkookumRuntimeBase
// #Author(s):  Conan Reis
SkBinaryHandle * SkUERuntime::get_binary_class_group(const SkClass & cls)
  {
  FString compiled_file = get_compiled_path();
  
  // $Revisit - CReis Should use fast custom uint32_t to hex string function.
  compiled_file += a_cstr_format("/Class[%x].sk-bin", cls.get_name_id());
  return SkBinaryHandleUE::create(*compiled_file);
  }


#if defined(A_SYMBOL_STR_DB_AGOG)  

//---------------------------------------------------------------------------------------
// Gets memory representing binary for class hierarchy and associated info.
// 
// #See Also:   load_compiled_scripts(), SkCompiler::get_binary_class_group()
// #Modifiers:  virtual - overridden from SkookumRuntimeBase
// #Author(s):  Conan Reis
SkBinaryHandle * SkUERuntime::get_binary_symbol_table()
  {
  FString sym_file = get_compiled_path() / TEXT("classes.sk-sym");

  A_DPRINT("  Loading compiled binary symbol file '%ls'...\n", *sym_file);

  SkBinaryHandleUE * handle_p = SkBinaryHandleUE::create(*sym_file);

  // Ensure symbol table binary exists
  if (!handle_p)
    {
    A_DPRINT("  ...it does not exist!\n\n", *sym_file);
    }

  return handle_p;
  }                                                                  

#endif


//---------------------------------------------------------------------------------------
// #Author(s):  Conan Reis
void SkUERuntime::release_binary(SkBinaryHandle * handle_p)
  {
  delete static_cast<SkBinaryHandleUE *>(handle_p);
  }



