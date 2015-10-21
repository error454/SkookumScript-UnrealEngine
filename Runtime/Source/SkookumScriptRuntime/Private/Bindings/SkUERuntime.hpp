//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// SkookumScript Runtime Hooks for Unreal - Input/Output Init/Update/Deinit Manager
// 
// Author: Conan Reis
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include <SkookumScript/SkookumRuntimeBase.hpp>
#include "../SkookumScriptListenerManager.hpp"
#include "SkUEBlueprintInterface.hpp"

#include "Platform.h"  // Set up base types, etc for the platform

//---------------------------------------------------------------------------------------

class ISkookumScriptRuntimeEditorInterface;

//=======================================================================================
// Global Structures
//=======================================================================================

//---------------------------------------------------------------------------------------
// SkookumScript Runtime Hooks for Unreal
// - Input/Output Init/Update/Deinit Manager
class SkUERuntime : public SkookumRuntimeBase
  {
  public:

    static SkUERuntime * get_singleton() { return static_cast<SkUERuntime *>(SkookumRuntimeBase::ms_singleton_p); }

  // Methods

    SkUERuntime() : m_is_initialized(false), m_compiled_file_b(false), m_listener_manager(256, 256), m_editor_interface_p(nullptr){ ms_singleton_p = this; }
    ~SkUERuntime() {}

    void startup();
    void shutdown();

    // Script Loading / Binding

      const FString & get_compiled_path() const;
      bool            content_file_exists(const TCHAR * file_name_p, FString * folder_path_p) const;

      bool load_compiled_scripts(bool ensure_atomics = true, SkClass ** ignore_classes_pp = nullptr, uint32_t ignore_count = 0u);

    // Overridden from SkookumRuntimeBase

      // Binary Serialization / Loading Overrides

        virtual bool             is_binary_hierarchy_existing() override;
        virtual SkBinaryHandle * get_binary_hierarchy() override;
        virtual SkBinaryHandle * get_binary_class_group(const SkClass & cls) override;
        virtual void             release_binary(SkBinaryHandle * handle_p) override;

        #if defined(A_SYMBOL_STR_DB_AGOG)  
          virtual SkBinaryHandle * get_binary_symbol_table() override;
        #endif

      // Flow Methods

        virtual void on_bind_routines() override;
        virtual void on_pre_deinitialize_session() override;

      // Accessors

        bool                                   is_initialized() const          { return m_is_initialized; }

        SkookumScriptListenerManager *         get_listener_manager()          { return &m_listener_manager; }
        SkUEBlueprintInterface *               get_blueprint_interface()       { return &m_blueprint_interface; }
        const SkUEBlueprintInterface *         get_blueprint_interface() const { return &m_blueprint_interface; }
        ISkookumScriptRuntimeEditorInterface * get_editor_interface() const    { return m_editor_interface_p; }

        void                                   set_editor_interface(ISkookumScriptRuntimeEditorInterface * editor_interface_p) { m_editor_interface_p = editor_interface_p; }

  protected:

    // Data Members

      bool                m_is_initialized;

      mutable bool        m_compiled_file_b;
      mutable FString     m_compiled_path;

      SkookumScriptListenerManager m_listener_manager;
      SkUEBlueprintInterface       m_blueprint_interface;

      ISkookumScriptRuntimeEditorInterface * m_editor_interface_p;

  };  // SkUERuntime

