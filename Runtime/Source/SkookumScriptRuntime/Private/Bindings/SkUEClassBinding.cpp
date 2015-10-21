//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Binding classes for UE4 
//
// Author: Markus Breyer
//=======================================================================================

#include "../SkookumScriptRuntimePrivatePCH.h"
#include "SkUEClassBinding.hpp"
#include "SkUERuntime.hpp"
#include "../Classes/SkookumScriptComponent.h"
#include <SkUEWorld.generated.hpp>
#include "Engine/SkUEEntity.hpp"
#include "VectorMath/SkColor.hpp"

#include "../../../../Generator/Source/SkookumScriptGenerator/Private/SkookumScriptGeneratorBase.inl"

//---------------------------------------------------------------------------------------

TMap<UClass*, SkClass*>                             SkUEClassBindingHelper::ms_static_class_map_u2s;
TMap<SkClassDescBase*, UClass*>                     SkUEClassBindingHelper::ms_static_class_map_s2u;
TMap<UStruct*, SkClass*>                            SkUEClassBindingHelper::ms_static_struct_map_u2s;
TMap<SkClassDescBase*, UStruct*>                    SkUEClassBindingHelper::ms_static_struct_map_s2u;
TMap<UEnum*, SkClass*>                              SkUEClassBindingHelper::ms_static_enum_map_u2s;

#if WITH_EDITORONLY_DATA
TMap<SkClassDescBase*, TWeakObjectPtr<UBlueprint>>  SkUEClassBindingHelper::ms_dynamic_class_map_s2u;
TMap<UBlueprint*, SkClass*>                         SkUEClassBindingHelper::ms_dynamic_class_map_u2s;
#endif

int32_t                                             SkUEClassBindingHelper::ms_world_data_idx = -1;

//---------------------------------------------------------------------------------------
// Compute data idx to world variable
int32_t SkUEClassBindingHelper::get_world_data_idx()
  {
  if (ms_world_data_idx < 0)
    {
    if (!SkBrain::ms_object_class_p->get_class_data().find(ASymbolX_c_world, AMatch_first_found, (uint32_t*)&ms_world_data_idx))
      {
      SK_ASSERTX(false, "Couldn't find the @@world class member variable!");
      }
    }

  return ms_world_data_idx;
  }

//---------------------------------------------------------------------------------------
// Get pointer to UWorld from global variable
UWorld * SkUEClassBindingHelper::get_world()
  {
  if (!SkookumScript::is_flag_set(SkookumScript::Flag_evaluate))
    {
    return nullptr;
    }

  SkInstance * world_var_p = SkBrain::ms_object_class_p->get_class_data_value_by_idx(get_world_data_idx());
  SK_ASSERTX(world_var_p && (world_var_p == SkBrain::ms_nil_p || world_var_p->get_class() == SkBrain::get_class(ASymbol_World)), "@@world variable does not have proper type."); // nil is ok
  return world_var_p == SkBrain::ms_nil_p ? nullptr : world_var_p->as<SkUEWorld>();
  }

//---------------------------------------------------------------------------------------
// Set the game world (C++ and Skookum variable) to a specific world object
void SkUEClassBindingHelper::set_world(UWorld * world_p)
  {
  if (SkookumScript::is_flag_set(SkookumScript::Flag_evaluate))
    {
    SkBrain::ms_object_class_p->set_class_data_value_by_idx_no_ref(get_world_data_idx(), world_p ? SkUEWorld::new_instance(world_p) : SkBrain::ms_nil_p);
    }
  }

//---------------------------------------------------------------------------------------
// Determine SkookumScript class from UClass
SkClass * SkUEClassBindingHelper::get_object_class(UObject * obj_p, UClass * def_uclass_p /*= nullptr*/, SkClass * def_class_p /*= nullptr*/)
  {
  SkClass * class_p = def_class_p;

  if (obj_p)
    {
    UClass * obj_uclass_p = obj_p->GetClass();
    if (obj_uclass_p != def_uclass_p)
      {
      // Crawl up class hierarchy until we find a class known to Sk
      SkClass * obj_class_p = nullptr;
      for (; !obj_class_p && obj_uclass_p; obj_uclass_p = obj_uclass_p->GetSuperClass())
        {
        obj_class_p = get_sk_class_from_ue_class(obj_uclass_p);
        }
      SK_ASSERTX(obj_class_p, a_str_format("UObject of type '%S' has no matching SkookumScript type!", *obj_p->GetClass()->GetName()));
      class_p = obj_class_p;
      }
    }

  return class_p;
  }

//---------------------------------------------------------------------------------------
// Return instance of an actor's SkookumScriptComponent, if any
SkInstance * SkUEClassBindingHelper::get_actor_component_instance(AActor * actor_p)
  {
  // If the actor has component, return the instance contained in the component
  if (actor_p)
    {
    USkookumScriptComponent * component_p = static_cast<USkookumScriptComponent *>(actor_p->GetComponentByClass(USkookumScriptComponent::StaticClass()));
    if (component_p)
      {
      return component_p->get_sk_instance();
      }
    }

  return nullptr;
  }

//---------------------------------------------------------------------------------------
// Resolve the raw data info of each raw data member of the given class
void SkUEClassBindingHelper::resolve_raw_data(tSkTypedNameRawArray & raw_data, SkClass * class_p)
  {
  // First check if it's a class
  UClass * ue_class_p = get_ue_class_from_sk_class(class_p);
  UStruct * ue_struct_or_class_p = ue_class_p;
  if (!ue_struct_or_class_p)
    {
    // Not a class, must be a struct then
    ue_struct_or_class_p = get_static_ue_struct_from_sk_class(class_p);
    }

  // Resolve pointer and accessor functions for 
  // 1) enums since they are the same for all enums
  // 2) Blueprint-generated classes
  SkClass * super_class_p = class_p->get_superclass();
  if (super_class_p && (super_class_p == SkEnum::ms_class_p || (ue_class_p && ue_class_p->ClassGeneratedBy != nullptr)))
    {
    class_p->register_raw_pointer_func(super_class_p->get_raw_pointer_func());
    class_p->register_raw_accessor_func(super_class_p->get_raw_accessor_func());
    }

  // Resolve raw data
  SK_ASSERTX(raw_data.is_empty() || ue_struct_or_class_p, a_str_format("Class '%s' has raw data but no known class mapping to UE4 for resolving.", class_p->get_name_cstr_dbg()));
  if (!raw_data.is_empty() && ue_struct_or_class_p)
    {
    // This loop assumes that the data members of the Sk class were created from this very UE4 class
    // I.e. that therefore, except for unsupported properties, they must be in the same order
    // So all we should have to do is loop forward and skip the occasional non-exported UE4 property
    UProperty * ue_var_p = nullptr;
    ASymbol ue_var_name;
    TFieldIterator<UProperty> property_it(ue_struct_or_class_p, EFieldIteratorFlags::ExcludeSuper);
    for (auto var_p : raw_data)
      {
      while (property_it)
        {
        ue_var_p = *property_it;
        ue_var_name = ASymbol::create_existing(FStringToAString(FSkookumScriptGeneratorBase::skookify_var_name(ue_var_p->GetName(), ue_var_p->IsA(UBoolProperty::StaticClass()), true)));
        ++property_it;
        if (var_p->get_name() == ue_var_name) break;
        }

      // Store raw data info in the raw data member object
      if (var_p->get_name() == ue_var_name)
        {
        var_p->m_raw_data_info = compute_raw_data_info(ue_var_p);
        }
      else
        {
        // Oops didn't find matching variable
        // This is probably due to an unsaved blueprint variable change in the UE4 editor during the previous session
        // If this is the case, a recompile would have been triggered when this class was loaded by get_ue_class_from_sk_class()
        // Which means binaries would be recompiled and reloaded once more, fixing this issue
        // So make sure this assumption is true
        SK_ASSERTX(FModuleManager::Get().GetModulePtr<ISkookumScriptRuntime>("SkookumScriptRuntime")->is_freshen_binaries_pending(), a_str_format("Sk Variable '%s' not found in UE4 reflection data.", var_p->get_name_cstr()));
        var_p->m_raw_data_info = 0;
        }
      }
    }
  }

tSkRawDataInfo SkUEClassBindingHelper::compute_raw_data_info(UProperty * ue_var_p)
  {
  tSkRawDataInfo raw_data_info = (tSkRawDataInfo(ue_var_p->GetOffset_ForInternal()) << Raw_data_info_offset_shift) | (tSkRawDataInfo(ue_var_p->GetSize()) << (Raw_data_info_type_shift + Raw_data_type_size_shift)); // Set raw_data_info to generic value

  FSkookumScriptGeneratorBase::eSkTypeID type_id = FSkookumScriptGeneratorBase::get_skookum_property_type(ue_var_p);
  if (type_id == FSkookumScriptGeneratorBase::SkTypeID_Integer)
    {
    // If integer, specify sign bit
    if (ue_var_p->IsA(UInt64Property::StaticClass())
      || ue_var_p->IsA(UIntProperty::StaticClass())
      || ue_var_p->IsA(UInt16Property::StaticClass())
      || ue_var_p->IsA(UInt8Property::StaticClass()))
      { // Mark as signed
      raw_data_info |= tSkRawDataInfo(1) << (Raw_data_info_type_shift + Raw_data_type_extra_shift);
      }
    }
  else if (type_id == FSkookumScriptGeneratorBase::SkTypeID_Boolean)
    {
    // If boolean, store bit shift as well
    static_assert(sizeof(HackedBoolProperty) == sizeof(UBoolProperty), "Must match so this hack will work.");
    HackedBoolProperty * bool_var_p = static_cast<HackedBoolProperty *>(ue_var_p);
    SK_ASSERTX(bool_var_p->FieldSize != 0, "BoolProperty must be initialized.");
    raw_data_info = (tSkRawDataInfo(ue_var_p->GetOffset_ForInternal() + bool_var_p->ByteOffset) << Raw_data_info_offset_shift) // Add byte offset into member offset
      | (tSkRawDataInfo(1) << (Raw_data_info_type_shift + Raw_data_type_size_shift)) // Size is always one byte
      | (tSkRawDataInfo(a_ceil_log_2((uint)bool_var_p->ByteMask)) << (Raw_data_info_type_shift + Raw_data_type_extra_shift));
    }
  else if (type_id == FSkookumScriptGeneratorBase::SkTypeID_List)
    {
    // If a list, store type information for elements as well
    const UArrayProperty * array_property_p = Cast<UArrayProperty>(ue_var_p);
    tSkRawDataInfo item_raw_data_info = compute_raw_data_info(array_property_p->Inner);
    raw_data_info |= ((item_raw_data_info >> Raw_data_info_type_shift) & Raw_data_info_type_mask) << Raw_data_info_elem_type_shift;
    }

  return raw_data_info;
  }

//---------------------------------------------------------------------------------------
// Get a raw pointer to the underlying UObject of a Sk Entity
void * SkUEClassBindingHelper::get_raw_pointer_entity(SkInstance * obj_p)
  {
  return obj_p->as<SkUEEntity>().get_obj();
  }

//---------------------------------------------------------------------------------------
// Access an Entity/UObject reference
SkInstance * SkUEClassBindingHelper::access_raw_data_entity(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p)
  {
  uint32_t byte_offset = (raw_data_info >> Raw_data_info_offset_shift) & Raw_data_info_offset_mask;
  SK_ASSERTX(((raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_size_shift)) & Raw_data_type_size_mask) == sizeof(SkUEWeakObjectPtr<UObject>), "Size of data type and data member must match!");
  
  UObject ** data_p = (UObject **)((uint8_t*)obj_p + byte_offset);

  // Set or get?
  if (value_p)
    {
    // Set value
    *data_p = value_p->as<SkUEEntity>();
    return nullptr;
    }

  // Get value
  return SkUEEntity::new_instance(*data_p, data_type_p->get_key_class());
  }

//---------------------------------------------------------------------------------------
// Access a Boolean
SkInstance * SkUEClassBindingHelper::access_raw_data_boolean(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p)
  {
  uint32_t byte_offset = (raw_data_info >> Raw_data_info_offset_shift) & Raw_data_info_offset_mask;
  uint32_t byte_size = (raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_size_shift)) & Raw_data_type_size_mask;
  uint32_t bit_shift = (raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_extra_shift)) & Raw_data_type_extra_mask;

  uint8_t * data_p = (uint8_t*)obj_p + byte_offset;

  // Set or get?
  uint32_t bit_mask = 1 << bit_shift;
  if (value_p)
    {
    // Set value
    uint8_t data = *data_p & ~bit_mask;
    if (value_p->as<SkBoolean>())
      {
      data |= bit_mask;
      }
    *data_p = data;
    return nullptr;
    }

  // Get value
  return SkBoolean::new_instance(*data_p & bit_mask);
  }

//---------------------------------------------------------------------------------------
// Access an Integer
SkInstance * SkUEClassBindingHelper::access_raw_data_integer(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p)
  {
  uint32_t byte_offset = (raw_data_info >> Raw_data_info_offset_shift) & Raw_data_info_offset_mask;
  uint32_t byte_size = (raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_size_shift)) & Raw_data_type_size_mask;
  uint32_t is_signed = (raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_extra_shift)) & Raw_data_type_extra_mask;
  SK_ASSERTX(byte_size == 1 || byte_size == 2 || byte_size == 4 || byte_size == 8, "Integer must have proper size.");

  uint8_t * raw_data_p = (uint8_t*)obj_p + byte_offset;

  // Set or get?
  if (value_p)
    {
    // Set value
    SkIntegerType value = value_p->as<SkInteger>();
    if (byte_size == 4)
      {
      if (is_signed)
        {
        *(int32_t *)raw_data_p = value;
        }
      else
        {
        *(uint32_t *)raw_data_p = value;
        }
      }
    else if (byte_size == 2)
      {
      if (is_signed)
        {
        *(int16_t *)raw_data_p = (int16_t)value;
        }
      else
        {
        *(uint16_t *)raw_data_p = (uint16_t)value;
        }
      }
    else if (byte_size == 1)
      {
      if (is_signed)
        {
        *(int8_t *)raw_data_p = (int8_t)value;
        }
      else
        {
        *(uint8_t *)raw_data_p = (uint8_t)value;
        }
      }
    else // byte_size == 8
      {
      if (is_signed)
        {
        *(int64_t *)raw_data_p = value;
        }
      else
        {
        *(uint64_t *)raw_data_p = value;
        }
      }

    return nullptr;
    }

  // Get value
  SkIntegerType value;
  if (byte_size == 4)
    {
    if (is_signed)
      {
      value = *(int32_t *)raw_data_p;
      }
    else
      {
      value = *(uint32_t *)raw_data_p;
      }
    }
  else if (byte_size == 2)
    {
    if (is_signed)
      {
      value = *(int16_t *)raw_data_p;
      }
    else
      {
      value = *(uint16_t *)raw_data_p;
      }
    }
  else if (byte_size == 1)
    {
    if (is_signed)
      {
      value = *(int8_t *)raw_data_p;
      }
    else
      {
      value = *(uint8_t *)raw_data_p;
      }
    }
  else // byte_size == 8
    {
    if (is_signed)
      {
      value = (SkIntegerType)*(int64_t *)raw_data_p;
      }
    else
      {
      value = (SkIntegerType)*(uint64_t *)raw_data_p;
      }
    }

  return SkInteger::new_instance(value);
  }

//---------------------------------------------------------------------------------------
// Access a String
SkInstance * SkUEClassBindingHelper::access_raw_data_string(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p)
  {
  uint32_t byte_offset = (raw_data_info >> Raw_data_info_offset_shift) & Raw_data_info_offset_mask;
  SK_ASSERTX(((raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_size_shift)) & Raw_data_type_size_mask) == sizeof(FString), "Size of data type and data member must match!");

  FString * data_p = (FString *)((uint8_t*)obj_p + byte_offset);

  // Set or get?
  if (value_p)
    {
    // Set value
    *data_p = AStringToFString(value_p->as<SkString>());
    return nullptr;
    }

  // Get value
  return SkString::new_instance(FStringToAString(*data_p));
  }

//---------------------------------------------------------------------------------------
// Access an Enum
SkInstance * SkUEClassBindingHelper::access_raw_data_enum(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p)
  {
  uint32_t byte_offset = (raw_data_info >> Raw_data_info_offset_shift) & Raw_data_info_offset_mask;
  SK_ASSERTX(((raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_size_shift)) & Raw_data_type_size_mask) == 1, "Only byte enums supported at this point!");

  SkEnumType * data_p = (SkEnumType *)((uint8_t*)obj_p + byte_offset);

  // Set or get?
  if (value_p)
    {
    // Set value
    *data_p = value_p->as<SkEnum>();
    return nullptr;
    }

  // Get value
  return SkEnum::new_instance(*data_p, data_type_p->get_key_class());
  }

//---------------------------------------------------------------------------------------
// Access a Color
SkInstance * SkUEClassBindingHelper::access_raw_data_color(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p)
  {
  uint32_t byte_offset = (raw_data_info >> Raw_data_info_offset_shift) & Raw_data_info_offset_mask;
  uint32_t byte_size = (raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_size_shift)) & Raw_data_type_size_mask;
  SK_ASSERTX(byte_size == sizeof(FColor) || byte_size == sizeof(FLinearColor), "Size of data type and data member must match!");

  // FColor or FLinearColor?
  if (byte_size == sizeof(FColor))
    {
    // FColor
    FColor * data_p = (FColor *)((uint8_t*)obj_p + byte_offset);

    // Set or get?
    if (value_p)
      {
      // Set value
      *data_p = value_p->as<SkColor>().ToFColor(true);
      return nullptr;
      }

    // Get value
    return SkColor::new_instance(*data_p);
    }

  // FLinearColor
  FLinearColor * data_p = (FLinearColor *)((uint8_t*)obj_p + byte_offset);

  // Set or get?
  if (value_p)
    {
    // Set value
    *data_p = value_p->as<SkColor>();
    return nullptr;
    }

  // Get value
  return SkColor::new_instance(*data_p);
  }

//---------------------------------------------------------------------------------------

SkInstance * SkUEClassBindingHelper::access_raw_data_list(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p)
  {
  uint32_t byte_offset = (raw_data_info >> Raw_data_info_offset_shift) & Raw_data_info_offset_mask;
  SK_ASSERTX(((raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_size_shift)) & Raw_data_type_size_mask) == sizeof(TArray<void *>), "Size of data type and data member must match!");

  TArray<uint8> *   data_p             = (TArray<uint8> *)((uint8_t*)obj_p + byte_offset);
  SkClassDescBase * item_type_p        = data_type_p->get_item_type();
  SkClass *         item_class_p       = item_type_p->get_key_class();
  tSkRawDataInfo    item_raw_data_info = ((raw_data_info >> Raw_data_info_elem_type_shift) & Raw_data_info_elem_type_mask) << Raw_data_info_type_shift;
  uint32_t          item_size          = (raw_data_info >> (Raw_data_info_elem_type_shift + Raw_data_type_size_shift)) & Raw_data_type_size_mask;

  // Set or get?
  if (value_p)
    {
    // Set value
    SkInstanceList & list = value_p->as<SkList>();
    APArray<SkInstance> & list_instances = list.get_instances();
    uint32_t num_elements = list_instances.get_length();
    SK_ASSERTX(uint32_t(data_p->Max()) >= num_elements, "Currently cannot assign to a UE4 array when it does not already have enough memory pre-allocated. Sorry - we'll work on that.");
    if (uint32_t(data_p->Max()) >= num_elements)
      {
      data_p->SetNumUninitialized(num_elements);
      uint8_t * item_array_p = (uint8_t *)data_p->GetData();
      for (uint32_t i = 0; i < num_elements; ++i)
        {
        item_class_p->assign_raw_data(item_array_p, item_raw_data_info, item_type_p, list_instances[i]);
        item_array_p += item_size;
        }
      }
    return nullptr;
    }

  // Get value
  SkInstance * instance_p = SkList::new_instance(data_p->Num());
  SkInstanceList & list = instance_p->as<SkList>();
  APArray<SkInstance> & list_instances = list.get_instances();
  uint8_t * item_array_p = (uint8_t *)data_p->GetData();
  for (uint32_t i = data_p->Num(); i; --i)
    {
    list_instances.append(*item_class_p->new_instance_from_raw_data(item_array_p, item_raw_data_info, item_type_p));
    item_array_p += item_size;
    }
  return instance_p;
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::reset_static_class_mappings(uint32_t reserve)
  {
  ms_static_class_map_u2s.Reset();
  ms_static_class_map_s2u.Reset();
  ms_static_class_map_u2s.Reserve(reserve);
  ms_static_class_map_s2u.Reserve(reserve);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::reset_static_struct_mappings(uint32_t reserve)
  {
  ms_static_struct_map_u2s.Reset();
  ms_static_struct_map_s2u.Reset();
  ms_static_struct_map_u2s.Reserve(reserve);
  ms_static_struct_map_s2u.Reserve(reserve);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::reset_static_enum_mappings(uint32_t reserve)
  {
  ms_static_enum_map_u2s.Reset();
  ms_static_enum_map_u2s.Reserve(reserve);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::add_static_class_mapping(SkClass * sk_class_p, UClass * ue_class_p)
  {
  ms_static_class_map_u2s.Add(ue_class_p, sk_class_p);
  ms_static_class_map_s2u.Add(sk_class_p, ue_class_p);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::add_static_struct_mapping(SkClass * sk_class_p, UStruct * ue_struct_p)
  {
  ms_static_struct_map_u2s.Add(ue_struct_p, sk_class_p);
  ms_static_struct_map_s2u.Add(sk_class_p, ue_struct_p);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::add_static_enum_mapping(SkClass * sk_class_p, UEnum * ue_enum_p)
  {
  ms_static_enum_map_u2s.Add(ue_enum_p, sk_class_p);
  }

#if WITH_EDITORONLY_DATA

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::reset_dynamic_class_mappings()
  {
  ms_dynamic_class_map_u2s.Reset();
  ms_dynamic_class_map_s2u.Reset();
  }

//---------------------------------------------------------------------------------------

UClass * SkUEClassBindingHelper::add_dynamic_class_mapping(SkClassDescBase * sk_class_desc_p)
  {
  // Get fully derived SkClass
  SkClass * sk_class_p = sk_class_desc_p->get_key_class();

  // Dynamic classes have blueprints - look it up by name
  FString class_name(sk_class_p->get_name_cstr());
  UBlueprint * blueprint_p = FindObject<UBlueprint>(ANY_PACKAGE, *class_name);
  if (!blueprint_p)
    {
    // If we still can't find the blueprint, try to load it
    ISkookumScriptRuntimeEditorInterface * editor_interface_p = SkUERuntime::get_singleton()->get_editor_interface();
    if (editor_interface_p)
      {
      bool class_deleted = false;
      blueprint_p = editor_interface_p->load_blueprint_asset(AStringToFString(sk_class_desc_p->get_key_class()->get_class_path_str(editor_interface_p->get_scripts_path_depth())), &class_deleted);
      if (class_deleted)
        {
        // Make class inert until we reloaded a new binary without the class
        sk_class_p->clear_members();
        }
      }
    if (!blueprint_p) return nullptr;
    }

  // Add to map of known class equivalences
  ms_dynamic_class_map_u2s.Add(blueprint_p, sk_class_p);
  ms_dynamic_class_map_s2u.Add(sk_class_p, blueprint_p);

  // Return latest generated class belonging to this blueprint
  return blueprint_p->GeneratedClass;
  }

//---------------------------------------------------------------------------------------

SkClass * SkUEClassBindingHelper::add_dynamic_class_mapping(UBlueprint * blueprint_p)
  {
  // Look up SkClass by blueprint name
  AString class_name(*blueprint_p->GetName(), blueprint_p->GetName().Len());
  SkClass * sk_class_p = SkBrain::get_class(ASymbol::create(class_name, ATerm_short));

  // If found, add to map of known class equivalences
  if (sk_class_p)
    {
    ms_dynamic_class_map_u2s.Add(blueprint_p, sk_class_p);
    ms_dynamic_class_map_s2u.Add(sk_class_p, blueprint_p);
    }

  return sk_class_p;
  }

#else // !WITH_EDITORONLY_DATA = cooked data build
//---------------------------------------------------------------------------------------

UClass * SkUEClassBindingHelper::add_static_class_mapping(SkClassDescBase * sk_class_desc_p)
  {
  // Get fully derived SkClass
  SkClass * sk_class_p = sk_class_desc_p->get_key_class();

  // Look it up and remember it
  FString class_name(sk_class_p->get_name_cstr());
  UClass * ue_class_p = FindObject<UClass>(ANY_PACKAGE, *(class_name + TEXT("_C")));
  if (ue_class_p)
    {
    add_static_class_mapping(sk_class_p, ue_class_p);
    }

  return ue_class_p;
  }

//---------------------------------------------------------------------------------------

SkClass * SkUEClassBindingHelper::add_static_class_mapping(UClass * ue_class_p)
  {
  // Look up SkClass by blueprint name
  const FString & ue_class_name = ue_class_p->GetName(); 
  int32 ue_class_name_len = ue_class_name.Len();
  if (ue_class_name_len < 3) return nullptr;

  // When we get to this function, we are looking for a class name that has "_C" appended at the end
  // So we subtract two from the length to truncate those two characters
  // We don't check here if the last two characters actually _are_ "_C" because it does not matter
  // since in that case it would be an error anyway
  AString class_name(*ue_class_name, ue_class_name_len - 2);
  SkClass * sk_class_p = SkBrain::get_class(ASymbol::create(class_name, ATerm_short));

  // If found, add to map of known class equivalences
  if (sk_class_p)
    {
    add_static_class_mapping(sk_class_p, ue_class_p);
    }

  return sk_class_p;
  }

#endif
