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
// Binding classes for UE4 
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include "SkookumScriptBehaviorComponent.h"

#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"

#include <SkookumScript/SkClassBindingBase.hpp>
#include <SkookumScript/SkString.hpp>
#include <SkookumScript/SkList.hpp>

//---------------------------------------------------------------------------------------

class FSkookumScriptRuntimeGenerator;

//---------------------------------------------------------------------------------------
// Helper class providing useful global variables and static methods
class SKOOKUMSCRIPTRUNTIME_API SkUEClassBindingHelper
  {
  public:

    enum
      {
      Raw_data_info_offset_shift    = 0,      // Byte offset inside structure where this property is stored
      Raw_data_info_offset_mask     = 0xFFFF, // Stored as 16 bit
      Raw_data_info_type_shift      = 16,     // See Raw_data_type_... below
      Raw_data_info_type_mask       = 0xFFFF, // See Raw_data_type_... below
      Raw_data_info_elem_type_shift = 32,
      Raw_data_info_elem_type_mask  = 0xFFFF,

      Raw_data_type_size_shift  = 0,      // Size of this data type
      Raw_data_type_size_mask   = 0x3FF,
      Raw_data_type_extra_shift = 10,     // Extra type-specific information stored here
      Raw_data_type_extra_mask  = 0x3F,
      };

    static UWorld *       get_world(); // Get tha world
    static void           set_world(UWorld * world_p);

  #if WITH_EDITORONLY_DATA
    static void           reset_dynamic_class_mappings();
  #endif
    static void           reset_static_class_mappings(uint32_t reserve);
    static void           reset_static_struct_mappings(uint32_t reserve);
    static void           reset_static_enum_mappings(uint32_t reserve);
    static void           add_slack_to_static_class_mappings(uint32_t slack);
    static void           add_slack_to_static_struct_mappings(uint32_t slack);
    static void           add_slack_to_static_enum_mappings(uint32_t slack);
    static void           forget_sk_classes_in_all_mappings();
    static void           register_static_class(UClass * ue_class_p);
    static void           register_static_struct(UStruct * ue_struct_p);
    static void           register_static_enum(UEnum * ue_enum_p);
    static bool           is_static_class_registered(UClass * ue_class_p);
    static bool           is_static_struct_registered(UStruct * ue_struct_p);
    static bool           is_static_enum_registered(UEnum * ue_enum_p);
    static void           add_static_class_mapping(SkClass * sk_class_p, UClass * ue_class_p);
    static void           add_static_struct_mapping(SkClass * sk_class_p, UStruct * ue_struct_p);
    static void           add_static_enum_mapping(SkClass * sk_class_p, UEnum * ue_enum_p);
    static SkClass *      get_sk_class_from_ue_class(UClass * ue_class_p);
    static UClass *       get_ue_class_from_sk_class(SkClassDescBase * sk_class_p);
    static UClass *       get_static_ue_class_from_sk_class(SkClassDescBase * sk_class_p);
    static UClass *       get_static_ue_class_from_sk_class_super(SkClassDescBase * sk_class_p);
    static SkClass *      get_static_sk_class_from_ue_struct(UStruct * ue_struct_p);
    static UStruct *      get_static_ue_struct_from_sk_class(SkClassDescBase * sk_class_p);
    static SkClass *      get_static_sk_class_from_ue_enum(UEnum * ue_enum_p);
    static SkClass *      find_most_derived_super_class_known_to_ue(SkClass * sk_class_p);
    static SkClass *      find_most_derived_super_class_known_to_ue(SkClass * sk_class_p, UClass ** out_ue_class_pp);
    static SkClass *      find_most_derived_super_class_known_to_sk(UClass * ue_class_p);
    static SkClass *      get_object_class(UObject * obj_p, UClass * def_uclass_p = nullptr, SkClass * def_class_p = nullptr); // Determine SkookumScript class from UClass
    static SkInstance *   get_actor_component_instance(AActor * actor_p); // Return SkInstance of an actor's SkookumScriptClassDataComponent if present, nullptr otherwise

    static FString        get_ue_class_name_sans_c(UClass * ue_class_p);

    static tSkRawDataInfo compute_raw_data_info(UProperty * ue_var_p);
    static bool           resolve_raw_data_static(SkClass * class_p);
    static void           resolve_raw_data(SkClass * class_p, UStruct * ue_struct_or_class_p);
    static void           resolve_raw_data_struct(SkClass * class_p, const TCHAR * ue_struct_name_p);
    static bool           resolve_raw_data_funcs(SkClass * class_p);

    static void *         get_raw_pointer_entity(SkInstance * obj_p);
    static SkInstance *   access_raw_data_entity(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p);
    static SkInstance *   access_raw_data_boolean(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p);
    static SkInstance *   access_raw_data_integer(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p);
    static SkInstance *   access_raw_data_string(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p);
    static SkInstance *   access_raw_data_enum(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p);
    static SkInstance *   access_raw_data_color(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p);
    static SkInstance *   access_raw_data_list(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p);

    template<class _BindingClass>
    static SkInstance *   access_raw_data_struct(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p);

    template<class _BindingClass, typename _DataType>
    static SkInstance *   new_instance(const _DataType & value) { return _BindingClass::new_instance(value); }

    template<class _BindingClass, typename _DataType, typename _CastType = _DataType>
    static void           set_from_instance(_DataType * out_value_p, SkInstance * instance_p) { *out_value_p = (_CastType)instance_p->as<_BindingClass>(); }

    template<class _BindingClass, typename _DataType>
    static void           initialize_list_from_array(SkInstanceList * out_instance_list_p, const TArray<_DataType> & array);

    template<class _BindingClass, typename _DataType>
    static void           initialize_empty_list_from_array(SkInstanceList * out_instance_list_p, const TArray<_DataType> & array);

    template<class _BindingClass, typename _DataType>
    static SkInstance *   list_from_array(const TArray<_DataType> & array);

    template<class _BindingClass, typename _DataType, typename _CastType = _DataType>
    static void           initialize_array_from_list(TArray<_DataType> * out_array_p, const SkInstanceList & list);

  protected:

    // Copy of bool property with public members so we can extract its private data
    struct HackedBoolProperty : UProperty
      {
      /** Size of the bitfield/bool property. Equal to ElementSize but used to check if the property has been properly initialized (0-8, where 0 means uninitialized). */
      uint8 FieldSize;
      /** Offset from the member variable to the byte of the property (0-7). */
      uint8 ByteOffset;
      /** Mask of the byte byte with the property value. */
      uint8 ByteMask;
      /** Mask of the field with the property value. Either equal to ByteMask or 255 in case of 'bool' type. */
      uint8 FieldMask;
      };

    // Copy of TArray so we access protected methods
    struct HackedTArray : TArray<uint8>
      {
      public:
        // Set size and leave array uninitialized
        FORCEINLINE void resize_uninitialized(int32 num_elements, int32 num_bytes_per_element)
          {
          if (num_elements > ArrayMax)
            {
            resize_to(num_elements, num_bytes_per_element);
            }
          SetNumUninitialized(num_elements);
          }

      protected:
        FORCENOINLINE void resize_to(int32 new_max, int32 num_bytes_per_element);
      };

  #if WITH_EDITORONLY_DATA
    static UClass *     add_dynamic_class_mapping(SkClassDescBase * sk_class_desc_p);
    static SkClass *    add_dynamic_class_mapping(UBlueprint * blueprint_p);
  #endif
    static UClass *     add_static_class_mapping(SkClassDescBase * sk_class_desc_p);
    static SkClass *    add_static_class_mapping(UClass * ue_class_p);

    static TMap<UClass*, SkClass*>                            ms_static_class_map_u2s; // Maps UClasses to their respective SkClasses
    static TMap<SkClassDescBase*, UClass*>                    ms_static_class_map_s2u; // Maps SkClasses to their respective UClasses
    static TMap<UStruct*, SkClass*>                           ms_static_struct_map_u2s; // Maps UStructs to their respective SkClasses
    static TMap<SkClassDescBase*, UStruct*>                   ms_static_struct_map_s2u; // Maps SkClasses to their respective UStructs
    static TMap<UEnum*, SkClass*>                             ms_static_enum_map_u2s; // Maps UEnums to their respective SkClasses

  #if WITH_EDITORONLY_DATA
    static TMap<UBlueprint*, SkClass*>                        ms_dynamic_class_map_u2s; // Maps Blueprints to their respective SkClasses
    static TMap<SkClassDescBase*, TWeakObjectPtr<UBlueprint>> ms_dynamic_class_map_s2u; // Maps SkClasses to their respective Blueprints
  #endif

    static int32_t      get_world_data_idx();
    static int32_t      ms_world_data_idx;

  };

//---------------------------------------------------------------------------------------
// Template specializations
template<> inline SkInstance * SkUEClassBindingHelper::new_instance<SkString, FString>(const FString & value) { return SkString::new_instance(AString(*value, value.Len())); }
template<> inline void         SkUEClassBindingHelper::set_from_instance<SkString, FString>(FString * out_value_p, SkInstance * instance_p) { *out_value_p = FString(instance_p->as<SkString>().as_cstr()); }

//---------------------------------------------------------------------------------------
// Customized version of the UE weak pointer

template<class _UObjectType>
class SkUEWeakObjectPtr
  {
  public:
    SkUEWeakObjectPtr() {}
    SkUEWeakObjectPtr(_UObjectType * obj_p) : m_ptr(obj_p) {}

    bool is_valid() const               { return m_ptr.IsValid(); }
    _UObjectType * get_obj() const      { return m_ptr.Get(); }
    operator _UObjectType * () const    { return m_ptr.Get(); } // Cast myself to UObject pointer so it can be directly assigned to UObject pointer
    _UObjectType * operator -> () const { return m_ptr.Get(); }

    void operator = (const _UObjectType * obj_p)                    { m_ptr = obj_p; }
    void operator = (const SkUEWeakObjectPtr<_UObjectType> & other) { m_ptr = other.m_ptr; }

  protected:
    TWeakObjectPtr<_UObjectType>  m_ptr;
  };

//---------------------------------------------------------------------------------------
// Binding class encapsulating a (weak pointer to a) UObject
template<class _BindingClass, class _UObjectType>
class SkUEClassBindingEntity : public SkClassBindingBase<_BindingClass, SkUEWeakObjectPtr<_UObjectType>>
  {
  public:

    typedef SkUEWeakObjectPtr<_UObjectType>                     tDataType;
    typedef SkClassBindingAbstract<_BindingClass>               tBindingAbstract;
    typedef SkClassBindingBase<_BindingClass, tDataType>        tBindingBase;
    typedef SkUEClassBindingEntity<_BindingClass, _UObjectType> tBindingEntity;

    // Don't generate these per class - inherit copy constructor and assignment operator from SkUEEntity
    enum { Binding_has_ctor      = false }; // If to generate constructor
    enum { Binding_has_ctor_copy = false }; // If to generate copy constructor
    enum { Binding_has_assign    = false }; // If to generate assignment operator
    enum { Binding_has_dtor      = false }; // If to generate destructor

    // Class Data

    static UClass * ms_uclass_p; // Pointer to the UClass belonging to this binding

    // Class Methods

    //---------------------------------------------------------------------------------------
    // Allocate and initialize a new instance of this SkookumScript type with given sub class
    static SkInstance * new_instance(_UObjectType * obj_p, SkClass * sk_class_p)
      {
      if (obj_p)
        {
        if (sk_class_p->is_actor_class())
          {
          SkInstance * instance_p = SkUEClassBindingHelper::get_actor_component_instance(static_cast<AActor *>(static_cast<UObject *>(obj_p)));
          if (instance_p)
            {
            instance_p->reference();
            return instance_p;
            }
          }
        else if (sk_class_p->is_component_class())
          {
          USkookumScriptBehaviorComponent * component_p = static_cast<USkookumScriptBehaviorComponent *>(static_cast<UObject *>(obj_p));
          SkInstance * instance_p = component_p->get_sk_component_instance();
          instance_p->reference();
          return instance_p;
          }
        }

      SkInstance * instance_p = SkInstance::new_instance(sk_class_p);
      static_cast<tBindingBase *>(instance_p)->construct(obj_p);
      return instance_p;
      }

    //---------------------------------------------------------------------------------------
    // Allocate and initialize a new instance of this SkookumScript type
    // We override this so we can properly determine the actual class of the SkInstance
    // which may be a sub class of tBindingAbstract::ms_class_p 
    static SkInstance * new_instance(_UObjectType * obj_p, UClass * def_uclass_p = nullptr, SkClass * def_class_p = nullptr)
      {
      SkClass * sk_class_p = SkUEClassBindingHelper::get_object_class(obj_p, def_uclass_p ? def_uclass_p : ms_uclass_p, def_class_p ? def_class_p : tBindingAbstract::ms_class_p);
      return new_instance(obj_p, sk_class_p);
      }

  protected:

    // Make method bindings known to SkookumScript
    static void register_bindings()
      {
      // Bind basic methods
      tBindingBase::register_bindings();

      // Bind raw pointer callback function
      tBindingAbstract::ms_class_p->register_raw_pointer_func(&SkUEClassBindingHelper::get_raw_pointer_entity);

      // Bind raw accessor callback function
      tBindingAbstract::ms_class_p->register_raw_accessor_func(&SkUEClassBindingHelper::access_raw_data_entity);
      }

    // Convenience methods - initialize Sk class and bind methods
    static void register_bindings(ASymbol class_name)         { tBindingAbstract::initialize_class(class_name); register_bindings(); }
    static void register_bindings(const char * class_name_p)  { tBindingAbstract::initialize_class(class_name_p); register_bindings(); }
    static void register_bindings(uint32_t class_name_id)     { tBindingAbstract::initialize_class(class_name_id); register_bindings(); }

  };

//---------------------------------------------------------------------------------------
// Binding class encapsulating a (weak pointer to an) AActor
template<class _BindingClass, class _AActorType>
class SkUEClassBindingActor : public SkUEClassBindingEntity<_BindingClass, _AActorType>
  {
  public:

    typedef SkClassBindingAbstract<_BindingClass>               tBindingAbstract;
    typedef SkUEClassBindingEntity<_BindingClass, _AActorType>  tBindingEntity;

    // Class Methods

    //---------------------------------------------------------------------------------------
    // Allocate and initialize a new instance of this SkookumScript type
    // We override this so we can properly determine the actual class of the SkInstance
    // which may be a sub class of tBindingAbstract::ms_class_p 
    // The actor may also contain its own SkInstance inside its SkookumScriptClassDataComponent
    static SkInstance * new_instance(_AActorType * actor_p, UClass * def_uclass_p = nullptr, SkClass * def_class_p = nullptr)
      {
      // Check if we can get an instance from a SkookumScriptClassDataComponent
      // If not, create new entity instance
      SkInstance * instance_p = SkUEClassBindingHelper::get_actor_component_instance(actor_p);
      if (instance_p)
        {
        instance_p->reference();
        return instance_p;
        }
      SK_ASSERTX(!def_uclass_p || def_uclass_p == tBindingEntity::ms_uclass_p || def_uclass_p->IsChildOf(tBindingEntity::ms_uclass_p), "If you pass in def_uclass_p, it must be the same as or a super class of ms_uclass_p.");
      SK_ASSERTX(!def_class_p || def_class_p->is_class(*tBindingAbstract::ms_class_p), "If you pass in def_class_p, it must be the same as or a super class of ms_class_p.");
      return tBindingEntity::new_instance(actor_p, def_uclass_p ? def_uclass_p : tBindingEntity::ms_uclass_p, def_class_p ? def_class_p : tBindingAbstract::ms_class_p);
      }

  };

//---------------------------------------------------------------------------------------
// Class binding for UStruct
template<class _BindingClass, typename _DataType>
class SkUEClassBindingStruct : public SkClassBindingBase<_BindingClass, _DataType>
  {
  public:

    typedef SkClassBindingAbstract<_BindingClass>             tBindingAbstract;
    typedef SkClassBindingBase<_BindingClass, _DataType>      tBindingBase;
    typedef SkUEClassBindingStruct<_BindingClass, _DataType>  tBindingStruct;

    static UStruct * ms_ustruct_p; // Pointer to the UStruct belonging to this binding

  protected:

    // Make method bindings known to SkookumScript
    static void register_bindings()
      {
      // Bind basic methods
      tBindingBase::register_bindings();

      // Bind raw accessor callback function
      tBindingAbstract::ms_class_p->register_raw_accessor_func(&SkUEClassBindingHelper::access_raw_data_struct<_BindingClass>);
      }

    // Convenience methods - initialize Sk class and bind methods
    static void register_bindings(ASymbol class_name)        { tBindingAbstract::initialize_class(class_name); register_bindings(); }
    static void register_bindings(const char * class_name_p) { tBindingAbstract::initialize_class(class_name_p); register_bindings(); }
    static void register_bindings(uint32_t class_name_id)    { tBindingAbstract::initialize_class(class_name_id); register_bindings(); }

  };

//---------------------------------------------------------------------------------------
// Class binding for UStruct with plain old data (assign/copy with memcpy)
template<class _BindingClass, typename _DataType>
class SkUEClassBindingStructPod : public SkUEClassBindingStruct<_BindingClass,_DataType>
  {
  public:

  #ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdynamic-class-memaccess"
  #endif

    // Copy constructor and assignment use memcpy
    static void mthd_ctor_copy(SkInvokedMethod * scope_p, SkInstance ** result_pp) { ::memcpy(&scope_p->this_as<_BindingClass>(), &scope_p->get_arg<_BindingClass>(SkArg_1), sizeof(typename _BindingClass::tDataType)); }
    static void mthd_op_assign(SkInvokedMethod * scope_p, SkInstance ** result_pp) { ::memcpy(&scope_p->this_as<_BindingClass>(), &scope_p->get_arg<_BindingClass>(SkArg_1), sizeof(typename _BindingClass::tDataType)); }

  #ifdef __clang__
    #pragma clang diagnostic pop
  #endif
  };

//---------------------------------------------------------------------------------------
// Class binding for types with a constructor that takes an EForceInit argument
template<class _BindingClass, typename _DataType>
class SkClassBindingSimpleForceInit : public SkClassBindingBase<_BindingClass, _DataType>
  {
  public:
    // Constructor initializes with ForceInitToZero
    static void mthd_ctor(SkInvokedMethod * scope_p, SkInstance ** result_pp) { scope_p->get_this()->construct<_BindingClass>(ForceInitToZero); }
  };

//=======================================================================================
// Class Data Definitions
//=======================================================================================

//---------------------------------------------------------------------------------------
// Pointer to the UClass belonging to this binding
template<class _BindingClass, class _UObjectType>
UClass * SkUEClassBindingEntity<_BindingClass, _UObjectType>::ms_uclass_p = nullptr;

//---------------------------------------------------------------------------------------
// Pointer to the UStruct belonging to this binding
template<class _BindingClass, typename _DataType>
UStruct * SkUEClassBindingStruct<_BindingClass, _DataType>::ms_ustruct_p = nullptr;

//=======================================================================================
// Inline Function Definitions
//=======================================================================================

//---------------------------------------------------------------------------------------

inline bool SkUEClassBindingHelper::is_static_class_registered(UClass * ue_class_p)
  {
  return ms_static_class_map_u2s.Find(ue_class_p) != nullptr;
  }

//---------------------------------------------------------------------------------------

inline bool SkUEClassBindingHelper::is_static_struct_registered(UStruct * ue_struct_p)
  {
  return ms_static_struct_map_u2s.Find(ue_struct_p) != nullptr;
  }

//---------------------------------------------------------------------------------------

inline bool SkUEClassBindingHelper::is_static_enum_registered(UEnum * ue_enum_p)
  {
  return ms_static_enum_map_u2s.Find(ue_enum_p) != nullptr;
  }

//---------------------------------------------------------------------------------------

inline SkClass * SkUEClassBindingHelper::get_sk_class_from_ue_class(UClass * ue_class_p)
  {
  // First see if it's a known static (Engine) class
  SkClass ** sk_class_pp = ms_static_class_map_u2s.Find(ue_class_p);
  if (sk_class_pp) return *sk_class_pp;
  #if WITH_EDITORONLY_DATA
    // If not, it might be a dynamic (Blueprint) class
    UBlueprint * blueprint_p = UBlueprint::GetBlueprintFromClass(ue_class_p);
    if (!blueprint_p) return nullptr;
    // It's a blueprint class, see if we know it already
    sk_class_pp = ms_dynamic_class_map_u2s.Find(blueprint_p);
    if (sk_class_pp) return *sk_class_pp;
    // (Yet) unknown, try to look it up by name and add to map
    SkClass * sk_class_p = add_dynamic_class_mapping(blueprint_p);
    if (sk_class_p) return sk_class_p;
  #endif
  return add_static_class_mapping(ue_class_p);
  }

//---------------------------------------------------------------------------------------
// Find our (static!) UE counterpart
inline UClass * SkUEClassBindingHelper::get_static_ue_class_from_sk_class(SkClassDescBase * sk_class_p)
  {
  UClass ** ue_class_pp = ms_static_class_map_s2u.Find(sk_class_p);
  if (ue_class_pp) return *ue_class_pp;
  return add_static_class_mapping(sk_class_p);
  }

//---------------------------------------------------------------------------------------
// Find our (static!) UE counterpart
// If there is no direct match, crawl up the class hierarchy until we find a match
inline UClass * SkUEClassBindingHelper::get_static_ue_class_from_sk_class_super(SkClassDescBase * sk_class_p)
  {
  SkClass * sk_ue_class_p = sk_class_p->get_key_class();
  do
    {
    UClass ** ue_class_pp = ms_static_class_map_s2u.Find(sk_ue_class_p);
    if (ue_class_pp) return *ue_class_pp;
    sk_ue_class_p = sk_ue_class_p->get_superclass();
    } while (sk_ue_class_p);

  return nullptr;
  }

//---------------------------------------------------------------------------------------

inline SkClass * SkUEClassBindingHelper::get_static_sk_class_from_ue_struct(UStruct * ue_struct_p)
  {
  SkClass ** sk_class_pp = ms_static_struct_map_u2s.Find(ue_struct_p);
  return sk_class_pp ? *sk_class_pp : nullptr;
  }

//---------------------------------------------------------------------------------------

inline UStruct * SkUEClassBindingHelper::get_static_ue_struct_from_sk_class(SkClassDescBase * sk_class_p)
  {
  UStruct ** ue_struct_pp = ms_static_struct_map_s2u.Find(sk_class_p);
  return ue_struct_pp ? *ue_struct_pp : nullptr;
  }

//---------------------------------------------------------------------------------------

inline SkClass * SkUEClassBindingHelper::get_static_sk_class_from_ue_enum(UEnum * ue_enum_p)
  {
  SkClass ** sk_class_pp = ms_static_enum_map_u2s.Find(ue_enum_p);
  return sk_class_pp ? *sk_class_pp : nullptr;
  }

//---------------------------------------------------------------------------------------

inline UClass * SkUEClassBindingHelper::get_ue_class_from_sk_class(SkClassDescBase * sk_class_p)
  {
  // First see if it's a known static (Engine) class
  UClass ** ue_class_pp = ms_static_class_map_s2u.Find(sk_class_p);
  if (ue_class_pp) return *ue_class_pp;
  #if WITH_EDITORONLY_DATA
    // If not, see if it's a known dynamic (Blueprint) class
    TWeakObjectPtr<UBlueprint> * blueprint_pp = ms_dynamic_class_map_s2u.Find(sk_class_p);
    if (blueprint_pp)
      {
      UBlueprint * blueprint_p = blueprint_pp->Get();
      if (blueprint_p) return blueprint_p->GeneratedClass; // Note: GeneratedClass might be null here
      }
    // (Yet) unknown (or blueprint was rebuilt/reloaded), try to look it up by name and add to map
    UClass * ue_class_p = add_dynamic_class_mapping(sk_class_p);
    if (ue_class_p) return ue_class_p;
  #endif
  return add_static_class_mapping(sk_class_p);
  }

//---------------------------------------------------------------------------------------
// Given a SkookumScript class, find most derived SkookumScript class known to UE4
inline SkClass * SkUEClassBindingHelper::find_most_derived_super_class_known_to_ue(SkClass * sk_class_p)
  {
  for (; sk_class_p; sk_class_p = sk_class_p->get_superclass())
    {
    if (get_ue_class_from_sk_class(sk_class_p)) break;
    }
  return sk_class_p;
  }

//---------------------------------------------------------------------------------------
// Given a SkookumScript class, find most derived SkookumScript class known to UE4
inline SkClass * SkUEClassBindingHelper::find_most_derived_super_class_known_to_ue(SkClass * sk_class_p, UClass ** out_ue_class_pp)
  {
  UClass * ue_class_p = nullptr;
  for (; sk_class_p; sk_class_p = sk_class_p->get_superclass())
    {
    ue_class_p = get_ue_class_from_sk_class(sk_class_p);
    if (ue_class_p) break;
    }
  *out_ue_class_pp = ue_class_p;
  return sk_class_p;
  }

//---------------------------------------------------------------------------------------
// Given a UE4 class, find most derived UE4 class known to SkookumScript and return its SkookumScript class
inline SkClass * SkUEClassBindingHelper::find_most_derived_super_class_known_to_sk(UClass * ue_class_p)
  {
  SkClass * sk_class_p;
  for (sk_class_p = nullptr; !sk_class_p && ue_class_p; ue_class_p = ue_class_p->GetSuperClass())
    {
    sk_class_p = get_sk_class_from_ue_class(ue_class_p);
    }
  return sk_class_p;
  }

//---------------------------------------------------------------------------------------

template<class _BindingClass>
SkInstance * SkUEClassBindingHelper::access_raw_data_struct(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p)
  {
  uint32_t byte_offset = (raw_data_info >> Raw_data_info_offset_shift) & Raw_data_info_offset_mask;
  SK_ASSERTX(((raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_size_shift)) & Raw_data_type_size_mask) == sizeof(typename _BindingClass::tDataType), "Size of data type and data member must match!");

  typename _BindingClass::tDataType * data_p = (typename _BindingClass::tDataType *)((uint8_t*)obj_p + byte_offset);

  // Set or get?
  if (value_p)
    {
    // Set value
    *data_p = value_p->as<_BindingClass>();
    return nullptr;
    }

  // Get value
  return _BindingClass::new_instance(*data_p);
  }

//---------------------------------------------------------------------------------------

template<class _BindingClass, typename _DataType>
void SkUEClassBindingHelper::initialize_empty_list_from_array(SkInstanceList * out_instance_list_p, const TArray<_DataType> & array)
  {
  APArray<SkInstance> & list_instances = out_instance_list_p->get_instances();
  SK_ASSERTX(list_instances.is_empty(), "initialize_empty_list_from_array() called with non-empty list!");
  list_instances.ensure_size(array.Num());
  for (auto & item : array)
    {
    list_instances.append(*new_instance<_BindingClass, _DataType>(item));
    }
  }

//---------------------------------------------------------------------------------------

template<class _BindingClass, typename _DataType>
inline void SkUEClassBindingHelper::initialize_list_from_array(SkInstanceList * out_instance_list_p, const TArray<_DataType> & array)
  {
  out_instance_list_p->empty();
  initialize_empty_list_from_array<_BindingClass, _DataType>(out_instance_list_p, array);
  }

//---------------------------------------------------------------------------------------

template<class _BindingClass, typename _DataType>
inline SkInstance * SkUEClassBindingHelper::list_from_array(const TArray<_DataType> & array)
  {
  SkInstance * instance_p = SkList::new_instance(array.Num());
  initialize_empty_list_from_array<_BindingClass, _DataType>(&instance_p->as<SkList>(), array);
  return instance_p;
  }

//---------------------------------------------------------------------------------------

template<class _BindingClass, typename _DataType, typename _CastType>
void SkUEClassBindingHelper::initialize_array_from_list(TArray<_DataType> * out_array_p, const SkInstanceList & list)
  {
  APArray<SkInstance> & instances = list.get_instances();
  out_array_p->Reserve(instances.get_length());
  for (auto & instance_p : instances)
    {
    uint32 index = out_array_p->AddUninitialized();
    set_from_instance<_BindingClass, _DataType, _CastType>(&(*out_array_p)[index], instance_p);
    }
  }
