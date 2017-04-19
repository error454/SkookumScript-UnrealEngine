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
// SkookumScript Plugin for Unreal Engine 4
//
// Class for managing functions exposed to Blueprint graphs 
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include "UObject/WeakObjectPtr.h"
#include "UObject/Class.h"

#include <AgogCore/AFunctionArg.hpp>
#include <AgogCore/APArray.hpp>
#include <AgogCore/ASymbol.hpp>
#include <SkookumScript/SkBrain.hpp>
#include <SkookumScript/SkClass.hpp>
#include <SkookumScript/SkInvokableBase.hpp>
#include <SkookumScript/SkMethod.hpp>
#include <SkookumScript/SkParameters.hpp>

class SkClass;
class SkClassDescBase;
class SkInstance;
class SkInvokedMethod;
struct FFrame;

typedef AFunctionArgBase<UClass *>            tSkUEOnClassUpdatedFunc;
typedef AFunctionArgBase2<UFunction *, bool>  tSkUEOnFunctionUpdatedFunc;
typedef AFunctionArgBase<UClass *>            tSkUEOnFunctionRemovedFromClassFunc;

//---------------------------------------------------------------------------------------
// Class for managing functions exposed to Blueprint graphs
class SkUEBlueprintInterface
  {
  public:
              SkUEBlueprintInterface();
              ~SkUEBlueprintInterface();

    static SkUEBlueprintInterface * get() { return ms_singleton_p; }

    void      clear(tSkUEOnFunctionRemovedFromClassFunc * on_function_removed_from_class_f);
    bool      sync_all_bindings_from_binary(tSkUEOnFunctionRemovedFromClassFunc * on_function_removed_from_class_f);
    bool      sync_bindings_from_binary(SkClass * sk_class_p, tSkUEOnFunctionRemovedFromClassFunc * on_function_removed_from_class_f);
    bool      expose_all_bindings(tSkUEOnFunctionUpdatedFunc * on_function_updated_f, bool is_final);

    bool      is_skookum_blueprint_function(UFunction * function_p) const;
    bool      is_skookum_blueprint_event(UFunction * function_p) const;

  protected:

    // We place this magic number in the rep offset to be able to tell if a UFunction is an Sk event
    // Potential false positive is ok since we are using it only to select which graph nodes to update
    enum { EventMagicRepOffset = 0xBEEF };

    // To keep track of a parameter's name, size and type
    struct TypedName
      {
      ASymbol     m_name;
      ASymbol     m_sk_class_name;
      SkClass *   m_sk_class_p;
      uint32_t    m_byte_size;

      TypedName(const ASymbol & name, SkClass * sk_class_p) : m_name(name), m_sk_class_name(sk_class_p->get_name()), m_sk_class_p(sk_class_p), m_byte_size(0) {}
      void rebind_sk_class() { m_sk_class_p = SkBrain::get_class(m_sk_class_name); }
      };

    typedef SkInstance *  (*tK2ParamFetcher)(FFrame & stack, const TypedName & typed_name);
    typedef uint32_t      (*tSkValueGetter)(void * const result_p, SkInstance * value_p, const TypedName & typed_name);

    enum eBindingType
      {
      BindingType_Function,  // Call from Blueprints into Sk
      BindingType_Event,     // Call from Sk into Blueprints
      };

    // Keep track of a binding between Blueprints and Sk
    struct BindingEntry
      {
      ASymbol                   m_sk_invokable_name;  // Copy of m_sk_invokable_p->get_name() in case m_sk_invokable_p goes bad
      ASymbol                   m_sk_class_name;      // Copy of m_sk_invokable_p->get_scope()->get_name() in case m_sk_invokable_p goes bad
      SkInvokableBase *         m_sk_invokable_p;
      TWeakObjectPtr<UClass>    m_ue_class_p;         // Copy of m_ue_function_p->GetOwnerClass() to detect if a deleted UFunction leaves dangling pointers
      TWeakObjectPtr<UFunction> m_ue_function_p;
      uint16_t                  m_num_params;
      bool                      m_is_class_member;    // Copy of m_sk_invokable_p->is_class_member() in case m_sk_invokable_p goes bad
      bool                      m_marked_for_delete_class;
      bool                      m_marked_for_delete_all;
      eBindingType              m_type;

      BindingEntry(SkInvokableBase * sk_invokable_p, uint32_t num_params, eBindingType type)
        : m_sk_invokable_name(sk_invokable_p->get_name())
        , m_sk_class_name(sk_invokable_p->get_scope()->get_name())
        , m_sk_invokable_p(sk_invokable_p)
        , m_ue_class_p(nullptr)
        , m_ue_function_p(nullptr)
        , m_num_params(num_params)
        , m_is_class_member(sk_invokable_p->is_class_member())
        , m_marked_for_delete_class(false)
        , m_marked_for_delete_all(false)
        , m_type(type)
        {}


      void rebind_sk_invokable();

      };

    // Parameter being passed into Sk from Blueprints
    struct SkParamEntry : TypedName
      {
      tK2ParamFetcher m_fetcher_p;

      SkParamEntry(const ASymbol & name, SkClass * sk_class_p) : TypedName(name, sk_class_p), m_fetcher_p(nullptr) {}
      };

    // Function binding (call from Blueprints into Sk)
    struct FunctionEntry : public BindingEntry
      {
      TypedName       m_result_type;
      tSkValueGetter  m_result_getter;

      FunctionEntry(SkInvokableBase * sk_invokable_p, uint32_t num_params, SkClass * result_sk_class_p)
        : BindingEntry(sk_invokable_p, num_params, BindingType_Function)
        , m_result_type(ASymbol::ms_null, result_sk_class_p)
        , m_result_getter(nullptr)
        {}

      // The parameter entries are stored behind this structure in memory
      SkParamEntry *       get_param_entry_array()       { return (SkParamEntry *)(this + 1); }
      const SkParamEntry * get_param_entry_array() const { return (const SkParamEntry *)(this + 1); }
      };

    // Parameter being passed into Blueprints into Sk
    struct K2ParamEntry : TypedName
      {
      tSkValueGetter  m_getter_p;
      uint32_t        m_offset;

      K2ParamEntry(const ASymbol & name, SkClass * sk_class_p) : TypedName(name, sk_class_p), m_getter_p(nullptr), m_offset(0) {}
      };

    // Event binding (call from Sk into Blueprints)
    struct EventEntry : public BindingEntry
      {
      mutable TWeakObjectPtr<UFunction> m_ue_function_to_invoke_p; // The copy of our method we actually can invoke

      EventEntry(SkMethodBase * sk_method_p, uint32_t num_params)
        : BindingEntry(sk_method_p, num_params, BindingType_Event)
        {}

      // The parameter entries are stored behind this structure in memory
      K2ParamEntry *       get_param_entry_array()       { return (K2ParamEntry *)(this + 1); }
      const K2ParamEntry * get_param_entry_array() const { return (const K2ParamEntry *)(this + 1); }
      };

    struct ParamInfo
      {
      UProperty *       m_ue_param_p;
      tK2ParamFetcher   m_k2_param_fetcher_p;
      tSkValueGetter    m_sk_value_getter_p;
      };

    void                exec_method(FFrame & stack, void * const result_p, SkClass * class_scope_p, SkInstance * this_p);
    void                exec_class_method(FFrame & stack, void * const result_p);
    void                exec_instance_method(FFrame & stack, void * const result_p);
    void                exec_coroutine(FFrame & stack, void * const result_p);

    static void         mthd_trigger_event(SkInvokedMethod * scope_p, SkInstance ** result_pp);

    bool                sync_bindings_from_binary_recursively(SkClass * sk_class_p, tSkUEOnFunctionRemovedFromClassFunc * on_function_removed_from_class_f);
    bool                try_add_binding_entry(SkInvokableBase * sk_invokable_p);
    bool                try_update_binding_entry(SkInvokableBase * sk_invokable_p, int32_t * out_binding_index_p);
    bool                add_function_entry(SkInvokableBase * sk_invokable_p);
    bool                add_event_entry(SkMethodBase * sk_method_p);
    bool                expose_binding_entry(uint32_t i, tSkUEOnFunctionUpdatedFunc * on_function_updated_f, bool is_final);
    int32_t             store_binding_entry(BindingEntry * binding_entry_p, int32_t binding_index_to_use);
    void                delete_binding_entry(uint32_t binding_index);
    UFunction *         build_ue_function(UClass * ue_class_p, SkInvokableBase * sk_invokable_p, eBindingType binding_type, uint32_t binding_index, ParamInfo * out_param_info_array_p, bool is_final);
    UProperty *         build_ue_param(UFunction * ue_function_p, SkClassDescBase * sk_parameter_class_p, const FName & param_name, ParamInfo * out_param_info_p, bool is_final);
    static void         bind_event_method(SkMethodBase * sk_method_p);
    
    template<class _TypedName>
    static bool         have_identical_signatures(const tSkParamList & param_list, const _TypedName * param_array_p);

    static SkInstance * fetch_k2_param_boolean(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_integer(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_real(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_string(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_vector2(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_vector3(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_vector4(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_rotation_angles(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_transform(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_struct_val(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_struct_ref(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_entity(FFrame & stack, const TypedName & typed_name);
    static SkInstance * fetch_k2_param_enum(FFrame & stack, const TypedName & typed_name);

    static uint32_t     get_sk_value_boolean(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_integer(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_real(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_string(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_vector2(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_vector3(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_vector4(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_rotation_angles(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_transform(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_struct_val(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_struct_ref(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_entity(void * const result_p, SkInstance * value_p, const TypedName & typed_name);
    static uint32_t     get_sk_value_enum(void * const result_p, SkInstance * value_p, const TypedName & typed_name);

    APArray<BindingEntry> m_binding_entry_array;

    UScriptStruct *       m_struct_vector2_p;
    UScriptStruct *       m_struct_vector3_p;
    UScriptStruct *       m_struct_vector4_p;
    UScriptStruct *       m_struct_rotation_angles_p;
    UScriptStruct *       m_struct_transform_p;

    static SkUEBlueprintInterface * ms_singleton_p; // Hack, make it easy to access for callbacks
        
  };