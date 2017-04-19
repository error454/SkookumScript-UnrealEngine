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
// SkookumScript Unreal Engine Bindings
//=======================================================================================


//=======================================================================================
// Includes
//=======================================================================================

#include "SkUEBindings.hpp"

#include "VectorMath/SkVector2.hpp"
#include "VectorMath/SkVector3.hpp"
#include "VectorMath/SkVector4.hpp"
#include "VectorMath/SkRotation.hpp"
#include "VectorMath/SkRotationAngles.hpp"
#include "VectorMath/SkTransform.hpp"
#include "VectorMath/SkColor.hpp"

#include "Engine/SkUEName.hpp"
#include "Engine/SkUEActor.hpp"
#include "Engine/SkUEActorComponent.hpp"
#include "Engine/SkUEEntity.hpp"
#include "Engine/SkUEEntityClass.hpp"
#include "Engine/SkUESkookumScriptBehaviorComponent.hpp"

//=======================================================================================
// Engine-Generated
//=======================================================================================

// Massive code file containing thousands of generated bindings
#include <SkUEEngineGeneratedBindings.generated.inl> 

static SkUEEngineGeneratedBindings s_engine_generated_bindings;
static bool s_engine_ue_types_registered = false;
static SkUEBindingsInterface * s_project_ue_types_registered_p = nullptr;

//=======================================================================================
// SkUEBindings Methods
//=======================================================================================

//---------------------------------------------------------------------------------------
// Registers UE classes, structs and enums
void SkUEBindings::ensure_static_ue_types_registered(SkUEBindingsInterface * project_generated_bindings_p)
  {
  // Register generated classes
  if (!s_engine_ue_types_registered)
    {
    s_engine_generated_bindings.register_static_ue_types();

    // Manually register additional classes
    SkUESkookumScriptBehaviorComponent::ms_uclass_p = USkookumScriptBehaviorComponent::StaticClass();
    SkUEClassBindingHelper::register_static_class(SkUESkookumScriptBehaviorComponent::ms_uclass_p);
    #if WITH_EDITOR || HACK_HEADER_GENERATOR
      SkUESkookumScriptBehaviorComponent::ms_uclass_p->SetMetaData(TEXT("IsBlueprintBase"), TEXT("true")); // So it will get recognized as a component allowing Blueprints to be derived from
    #endif

    s_engine_ue_types_registered = true;
    }
  if (project_generated_bindings_p && project_generated_bindings_p != s_project_ue_types_registered_p)
    {
    project_generated_bindings_p->register_static_ue_types();
    s_project_ue_types_registered_p = project_generated_bindings_p;
    }
  }

//---------------------------------------------------------------------------------------
// Maps engine ue classes to sk classes
void SkUEBindings::begin_register_bindings()
  {
  // Register built-in bindings at this point
  SkBrain::register_builtin_bindings();

  // Core Overlay
  SkBoolean::get_class()->register_raw_accessor_func(&SkUEClassBindingHelper::access_raw_data_boolean);
  SkInteger::get_class()->register_raw_accessor_func(&SkUEClassBindingHelper::access_raw_data_integer);
  SkReal::get_class()->register_raw_accessor_func(&SkUEClassBindingHelper::access_raw_data_struct<SkReal>);
  SkString::get_class()->register_raw_accessor_func(&SkUEClassBindingHelper::access_raw_data_string);
  SkEnum::get_class()->register_raw_accessor_func(&SkUEClassBindingHelper::access_raw_data_enum);
  SkList::get_class()->register_raw_accessor_func(&SkUEClassBindingHelper::access_raw_data_list);

  // VectorMath Overlay
  SkVector2::register_bindings();
  SkVector3::register_bindings();
  SkVector4::register_bindings();
  SkRotation::register_bindings();
  SkRotationAngles::register_bindings();
  SkTransform::register_bindings();
  SkColor::register_bindings();

  // Engine-Generated/Project-Generated-C++ Overlay
  // Register static Sk types on both overlays, but register bindings only on one of them
  s_engine_generated_bindings.register_static_sk_types();
  s_engine_generated_bindings.register_bindings();

  // Engine Overlay
  SkUEEntity_Ext::register_bindings();
  SkUEEntityClass_Ext::register_bindings();
  SkUEActor_Ext::register_bindings();
  SkUEActorComponent_Ext::register_bindings();
  SkUESkookumScriptBehaviorComponent::register_bindings();
  SkUEName::register_bindings();
  SkUEName::get_class()->register_raw_accessor_func(&SkUEClassBindingHelper::access_raw_data_struct<SkUEName>);
  }

//---------------------------------------------------------------------------------------
// Registers bindings for SkookumScript
void SkUEBindings::finish_register_bindings(SkUEBindingsInterface * project_generated_bindings_p)
  {
  if (project_generated_bindings_p)
    {
    project_generated_bindings_p->register_static_sk_types();
    project_generated_bindings_p->register_bindings();
    }
  }
