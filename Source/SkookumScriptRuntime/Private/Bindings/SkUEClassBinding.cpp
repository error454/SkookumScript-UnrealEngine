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

#include "Bindings/SkUEClassBinding.hpp"
#include "ISkookumScriptRuntime.h"
#include "SkUERuntime.hpp"
#include "SkUESymbol.hpp"
#include "SkUEUtils.hpp"
#include "SkookumScriptClassDataComponent.h"
#include "Engine/SkUEEntity.hpp"
#include "VectorMath/SkColor.hpp"
#include <SkUEWorld.generated.hpp>
#include "../SkookumScriptRuntimeGenerator.h"

#include "UObject/Package.h"

#include <AgogCore/AMath.hpp>
#include <SkookumScript/SkBoolean.hpp>
#include <SkookumScript/SkEnum.hpp>
#include <SkookumScript/SkInteger.hpp>
#include <SkookumScript/SkList.hpp>


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
  if (SkookumScript::get_initialization_level() < SkookumScript::InitializationLevel_program)
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
  if (SkookumScript::get_initialization_level() >= SkookumScript::InitializationLevel_program)
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
    USkookumScriptClassDataComponent * component_p = static_cast<USkookumScriptClassDataComponent *>(actor_p->GetComponentByClass(USkookumScriptClassDataComponent::StaticClass()));
    if (component_p)
      {
      return component_p->get_sk_actor_instance();
      }
    }

  return nullptr;
  }

//---------------------------------------------------------------------------------------
// Chop off trailing "_C" if exists
FString SkUEClassBindingHelper::get_ue_class_name_sans_c(UClass * ue_class_p)
  {
  FString class_name = ue_class_p->GetName();
  class_name.RemoveFromEnd(TEXT("_C"));
  return class_name;
  }

//---------------------------------------------------------------------------------------
// Resolve the raw data info of each raw data member of the given class
void SkUEClassBindingHelper::resolve_raw_data(SkClass * class_p, UStruct * ue_struct_or_class_p)
  {
  // This loop assumes that the data members of the Sk class were created from this very UE4 class
  // I.e. that therefore, except for unsupported properties, they must be in the same order
  // So all we should have to do is loop forward and skip the occasional non-exported UE4 property
  UProperty * ue_var_p = nullptr;
  FString ue_var_name;
  AString sk_var_name;
  TFieldIterator<UProperty> property_it(ue_struct_or_class_p, EFieldIteratorFlags::ExcludeSuper);
  tSkTypedNameRawArray & raw_data = class_p->get_instance_data_raw_for_resolving();
  bool is_all_resolved = true;
  for (auto var_p : raw_data)
    {
    // Skip variable if already resolved
    if (var_p->m_raw_data_info != SkRawDataInfo_Invalid)
      { 
      continue;
      }

    // Try to find it in the UE4 reflection data
    bool found_match = false;
    while (property_it)
      {
      ue_var_p = *property_it;
      ++property_it;
      ue_var_p->GetName(ue_var_name);
      sk_var_name = var_p->get_name_str();
      found_match = FSkookumScriptGeneratorBase::compare_var_name_skookified(*ue_var_name, sk_var_name.as_cstr());
      // If it's unknown it might be due to case insensitivity of FName - e.g. "Blocked" might really be "bLocked"
      if (!found_match && ue_var_p->IsA(UBoolProperty::StaticClass()) && ue_var_name.Len() >= 2 && ue_var_name[0] == 'B')
        {
        ue_var_name[0] = FChar::ToLower(ue_var_name[0]);
        ue_var_name[1] = FChar::ToUpper(ue_var_name[1]);
        found_match = FSkookumScriptGeneratorBase::compare_var_name_skookified(*ue_var_name, sk_var_name.as_cstr());
        }
      if (found_match) break;
      }

    // Store raw data info in the raw data member object
    if (found_match)
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
      SK_ASSERTX(FModuleManager::Get().GetModulePtr<ISkookumScriptRuntime>("SkookumScriptRuntime")->is_freshen_binaries_pending(), a_str_format("Sk Variable '%s.%s' not found in UE4 reflection data.", class_p->get_name_cstr_dbg(), var_p->get_name_cstr()));
      is_all_resolved = false;
      }
    }
  // Remember if all raw data was resolved
  class_p->set_raw_data_resolved(is_all_resolved);
  }

//---------------------------------------------------------------------------------------
// Resolve the raw data info of each raw data member of the given class
bool SkUEClassBindingHelper::resolve_raw_data_static(SkClass * class_p)
  {
  // Resolve function pointers and determine if there's anything to do
  if (!resolve_raw_data_funcs(class_p))
    {
    return true;
    }

  // First check if it's a class
  UStruct * ue_struct_or_class_p = get_static_ue_class_from_sk_class(class_p);
  if (!ue_struct_or_class_p)
    {
    // Not a class, must be a struct then
    ue_struct_or_class_p = get_static_ue_struct_from_sk_class(class_p);
    }

  if (ue_struct_or_class_p)
    {
    // Resolve raw data
    resolve_raw_data(class_p, ue_struct_or_class_p);
    return true;
    }

  return false;
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::resolve_raw_data_struct(SkClass * class_p, const TCHAR * ue_struct_name_p)
  {
  resolve_raw_data(class_p, FindObjectChecked<UScriptStruct>(UObject::StaticClass()->GetOutermost(), ue_struct_name_p, false));
  }

//---------------------------------------------------------------------------------------
// Returns if there's anything to resolve
bool SkUEClassBindingHelper::resolve_raw_data_funcs(SkClass * class_p)
  {
  // By default, inherit raw pointer and accessor functions from super class
  if (!class_p->get_raw_pointer_func())
    {
    class_p->register_raw_pointer_func(class_p->get_raw_pointer_func_inherited());
    }
  if (!class_p->get_raw_accessor_func())
    {
    class_p->register_raw_accessor_func(class_p->get_raw_accessor_func_inherited());
    }

  // Return if there's anything to resolve
  bool has_no_raw_data = class_p->get_instance_data_raw().is_empty();
  class_p->set_raw_data_resolved(has_no_raw_data);
  return !has_no_raw_data;
  }

//---------------------------------------------------------------------------------------

tSkRawDataInfo SkUEClassBindingHelper::compute_raw_data_info(UProperty * ue_var_p)
  {
  tSkRawDataInfo raw_data_info = (tSkRawDataInfo(ue_var_p->GetOffset_ForInternal()) << Raw_data_info_offset_shift) | (tSkRawDataInfo(ue_var_p->GetSize()) << (Raw_data_info_type_shift + Raw_data_type_size_shift)); // Set raw_data_info to generic value

  FSkookumScriptGeneratorBase::eSkTypeID type_id = FSkookumScriptGeneratorBase::get_skookum_property_type(ue_var_p, true);
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
  else if (type_id == FSkookumScriptGeneratorBase::SkTypeID_UObjectWeakPtr)
    {
    // If weak ptr, store extra bit to indicate that
    raw_data_info |= tSkRawDataInfo(1) << (Raw_data_info_type_shift + Raw_data_type_extra_shift);
    }

  return raw_data_info;
  }

//---------------------------------------------------------------------------------------
// Get a raw pointer to the underlying UObject of a Sk Entity
void * SkUEClassBindingHelper::get_raw_pointer_entity(SkInstance * obj_p)
  {
  #if WITH_EDITORONLY_DATA
    // In editor, due to unreliable delegate callbacks, try a JIT resolve of unresolved classes
    SkClass * sk_class_p = obj_p->get_class();
    if (!sk_class_p->is_raw_data_resolved())
      {
      if (SkUEClassBindingHelper::resolve_raw_data_funcs(sk_class_p))
        {
        UClass * ue_class_p = SkUEClassBindingHelper::get_ue_class_from_sk_class(sk_class_p);
        if (ue_class_p)
          {
          SkUEClassBindingHelper::resolve_raw_data(sk_class_p, ue_class_p);
          }
        }
      }
  #endif

  return obj_p->as<SkUEEntity>().get_obj();
  }

//---------------------------------------------------------------------------------------
// Access an Entity/UObject reference
SkInstance * SkUEClassBindingHelper::access_raw_data_entity(void * obj_p, tSkRawDataInfo raw_data_info, SkClassDescBase * data_type_p, SkInstance * value_p)
  {
  uint32_t byte_offset = (raw_data_info >> Raw_data_info_offset_shift) & Raw_data_info_offset_mask;
  SK_ASSERTX(((raw_data_info >> (Raw_data_info_type_shift + Raw_data_type_size_shift)) & Raw_data_type_size_mask) == sizeof(UObject *), "Size of data type and data member must match!");

  // We know the data lives here
  uint8_t * data_p = (uint8_t*)obj_p + byte_offset;

  // Check special bit indicating it's stored as a weak pointer
  if (raw_data_info & (tSkRawDataInfo(1) << (Raw_data_info_type_shift + Raw_data_type_extra_shift)))
    {
    // Stored a weak pointer
    FWeakObjectPtr * weak_p = (FWeakObjectPtr *)data_p;

    // Set or get?
    if (value_p)
      {
      // Set value
      *weak_p = value_p->as<SkUEEntity>();
      return nullptr;
      }

    // Get value
    return SkUEEntity::new_instance(weak_p->Get(), nullptr, data_type_p->get_key_class());
    }
  else
    {
    // Stored as naked pointer
    UObject ** obj_pp = (UObject **)data_p;

    // Set or get?
    if (value_p)
      {
      // Set value
      *obj_pp = value_p->as<SkUEEntity>();
      return nullptr;
      }

    // Get value
    return SkUEEntity::new_instance(*obj_pp, nullptr, data_type_p->get_key_class());
    }
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

  HackedTArray *    data_p             = (HackedTArray *)((uint8_t*)obj_p + byte_offset);
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
    data_p->resize_uninitialized(num_elements, item_size);
    uint8_t * item_array_p = (uint8_t *)data_p->GetData();
    for (uint32_t i = 0; i < num_elements; ++i)
      {
      item_class_p->assign_raw_data(item_array_p, item_raw_data_info, item_type_p, list_instances[i]);
      item_array_p += item_size;
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

void SkUEClassBindingHelper::add_slack_to_static_class_mappings(uint32_t slack)
  {
  ms_static_class_map_u2s.Reserve(ms_static_class_map_u2s.Num() + slack);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::add_slack_to_static_struct_mappings(uint32_t slack)
  {
  ms_static_struct_map_u2s.Reserve(ms_static_struct_map_u2s.Num() + slack);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::add_slack_to_static_enum_mappings(uint32_t slack)
  {
  ms_static_enum_map_u2s.Reserve(ms_static_enum_map_u2s.Num() + slack);
  }

//---------------------------------------------------------------------------------------
// Forget all Sk classes

void SkUEClassBindingHelper::forget_sk_classes_in_all_mappings()
  {
  // Set all sk classes to null in ue->sk maps
  for (auto pair_iter = ms_static_class_map_u2s.CreateIterator(); pair_iter; ++pair_iter)
    {
    pair_iter.Value() = nullptr;
    }
  for (auto pair_iter = ms_static_struct_map_u2s.CreateIterator(); pair_iter; ++pair_iter)
    {
    pair_iter.Value() = nullptr;
    }
  for (auto pair_iter = ms_static_enum_map_u2s.CreateIterator(); pair_iter; ++pair_iter)
    {
    pair_iter.Value() = nullptr;
    }  

  // Completely clear sk->ue map
  ms_static_class_map_s2u.Reset();
  ms_static_struct_map_s2u.Reset();
  //ms_static_enum_map_s2u.Reset();

  // Also clear out dynamic mappings if we got any
  #if WITH_EDITORONLY_DATA
    reset_dynamic_class_mappings();
  #endif
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::register_static_class(UClass * ue_class_p)
  {
  ms_static_class_map_u2s.Add(ue_class_p, nullptr);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::register_static_struct(UStruct * ue_struct_p)
  {
  ms_static_struct_map_u2s.Add(ue_struct_p, nullptr);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::register_static_enum(UEnum * ue_enum_p)
  {
  ms_static_enum_map_u2s.Add(ue_enum_p, nullptr);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::add_static_class_mapping(SkClass * sk_class_p, UClass * ue_class_p)
  {
  SK_ASSERTX(sk_class_p && ue_class_p, a_str_format("Tried to add static class mapping between `%s` and `%S` one of which is null.", sk_class_p ? sk_class_p->get_name_cstr() : "(null)", ue_class_p ? *ue_class_p->GetName() : TEXT("(null)")));
  ms_static_class_map_u2s.Add(ue_class_p, sk_class_p);
  ms_static_class_map_s2u.Add(sk_class_p, ue_class_p);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::add_static_struct_mapping(SkClass * sk_class_p, UStruct * ue_struct_p)
  {
  SK_ASSERTX(sk_class_p && ue_struct_p, a_str_format("Tried to add static class mapping between `%s` and `%S` one of which is null.", sk_class_p ? sk_class_p->get_name_cstr() : "(null)", ue_struct_p ? *ue_struct_p->GetName() : TEXT("(null)")));
  ms_static_struct_map_u2s.Add(ue_struct_p, sk_class_p);
  ms_static_struct_map_s2u.Add(sk_class_p, ue_struct_p);
  }

//---------------------------------------------------------------------------------------

void SkUEClassBindingHelper::add_static_enum_mapping(SkClass * sk_class_p, UEnum * ue_enum_p)
  {
  SK_ASSERTX(sk_class_p && ue_enum_p, a_str_format("Tried to add static class mapping between `%s` and `%S` one of which is null.", sk_class_p ? sk_class_p->get_name_cstr() : "(null)", ue_enum_p ? *ue_enum_p->GetName() : TEXT("(null)")));
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
    return nullptr;
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

#endif

//---------------------------------------------------------------------------------------

UClass * SkUEClassBindingHelper::add_static_class_mapping(SkClassDescBase * sk_class_desc_p)
  {
  // Get fully derived SkClass
  SkClass * sk_class_p = sk_class_desc_p->get_key_class();

  // Look up the plain class name first (i.e. a C++ class)
  FString class_name(sk_class_p->get_name_cstr());
  UClass * ue_class_p = FindObject<UClass>(ANY_PACKAGE, *class_name);
  if (ue_class_p)
    {
    add_static_class_mapping(sk_class_p, ue_class_p);
    return ue_class_p;
    }

  // In cooked builds, also look up class name + "_C"
  // We don't do this in editor builds as these classes are dynamically generated and might get deleted at any time
  // there we use dynamic mapping of Blueprints instead
  #if !WITH_EDITORONLY_DATA
    ue_class_p = FindObject<UClass>(ANY_PACKAGE, *(class_name + TEXT("_C")));
    if (ue_class_p)
      {
      add_static_class_mapping(sk_class_p, ue_class_p);
      }
  #endif

  return ue_class_p;
  }

//---------------------------------------------------------------------------------------

SkClass * SkUEClassBindingHelper::add_static_class_mapping(UClass * ue_class_p)
  {
  // Look up SkClass by class name
  const FString & ue_class_name = ue_class_p->GetName();
  int32 ue_class_name_len = ue_class_name.Len();
  AString class_name(*ue_class_name, ue_class_name_len);
  ASymbol class_symbol = ASymbol::create_existing(class_name);
  if (!class_symbol.is_null())
    {
    SkClass * sk_class_p = SkBrain::get_class(class_symbol);
    if (sk_class_p)
      {
      // Add to map of known class equivalences
      add_static_class_mapping(sk_class_p, ue_class_p);
      return sk_class_p;
      }
    }

  // In cooked builds, also look up class name - "_C"
  // We don't do this in editor builds as these classes are dynamically generated and might get deleted at any time
  // there we use dynamic mapping of Blueprints instead
  #if !WITH_EDITORONLY_DATA

    // When we get to this function, we are looking for a class name that has "_C" appended at the end
    // So we subtract two from the length to truncate those two characters
    // We don't check here if the last two characters actually _are_ "_C" because it does not matter
    // since in that case it would be an error anyway
    if (ue_class_name_len < 3) return nullptr;
    class_name.set_length(ue_class_name_len - 2);
    class_symbol = ASymbol::create_existing(class_name);
    if (!class_symbol.is_null())
      {
      SkClass * sk_class_p = SkBrain::get_class(ASymbol::create_existing(class_name));
      if (sk_class_p)
        {
        // Add to map of known class equivalences
        add_static_class_mapping(sk_class_p, ue_class_p);
        return sk_class_p;
        }
      }

  #endif

  return nullptr;
  }

//=======================================================================================
// SkUEClassBindingHelper::HackedTArray
//=======================================================================================

FORCENOINLINE void SkUEClassBindingHelper::HackedTArray::resize_to(int32 new_max, int32 num_bytes_per_element)
  {
  if (new_max)
    {
    new_max = AllocatorInstance.CalculateSlackReserve(new_max, num_bytes_per_element);
    }
  if (new_max != ArrayMax)
    {
    ArrayMax = new_max;
    AllocatorInstance.ResizeAllocation(ArrayNum, ArrayMax, num_bytes_per_element);
    }
  }
