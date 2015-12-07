//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// SkookumScript Actor Component
// 
// Author: Conan Reis, Markus Breyer
//=======================================================================================


//=======================================================================================
// Includes
//=======================================================================================

#include "SkookumScriptRuntimePrivatePCH.h"
#include "../Classes/SkookumScriptComponent.h"
#include "Bindings/SkUEBindings.hpp"
#include "SkUEActor.generated.hpp"


//=======================================================================================
// Class Data
//=======================================================================================

//=======================================================================================
// Method Definitions
//=======================================================================================

//---------------------------------------------------------------------------------------
USkookumScriptComponent::USkookumScriptComponent(const FObjectInitializer& ObjectInitializer)
  : Super(ObjectInitializer)
  {
  PrimaryComponentTick.bCanEverTick = false;
  bTickInEditor = false;
  bAutoActivate = true;
  bWantsInitializeComponent = true;
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::create_sk_instance()
  {
  SK_ASSERTX(!m_instance_p, "Tried to create instance when instance already present!");

  // Find the actor I belong to
  AActor * actor_p = GetOwner();
  SK_ASSERTX(actor_p, "USkookumScriptComponent must be attached to an actor.");

  // Determine SkookumScript class of my actor
  SkClass * class_p = nullptr;
  FString class_name = ScriptClassName;
  if (!class_name.IsEmpty())
    {
    AString class_name_ascii(*class_name, class_name.Len());
    class_p = SkBrain::get_class(class_name_ascii.as_cstr());
    SK_ASSERTX(class_p, a_cstr_format("Cannot find Script Class Name '%s' specified in SkookumScriptComponent of '%S'. Misspelled?", class_name_ascii.as_cstr(), *actor_p->GetName()));
    if (!class_p) goto set_default_class; // Recover from bad user input

    // Do some extra checking in non-shipping builds
    #if (SKOOKUM & SK_DEBUG)
      // Find most derived SkookumScript class known to UE4
      SkClass * known_super_class_p;
      for (known_super_class_p = class_p; known_super_class_p; known_super_class_p = known_super_class_p->get_superclass())
        {
        if (SkUEClassBindingHelper::get_ue_class_from_sk_class(known_super_class_p)) break;
        }

      // Find most derived UE4 class known to SkookumScript
      SkClass * mapped_super_class_p = nullptr;
      for (UClass * obj_uclass_p = actor_p->GetClass(); !mapped_super_class_p && obj_uclass_p; obj_uclass_p = obj_uclass_p->GetSuperClass())
        {
        mapped_super_class_p = SkUEClassBindingHelper::get_sk_class_from_ue_class(obj_uclass_p);
        }
      SK_ASSERTX(class_p->is_mind_class() || (mapped_super_class_p && mapped_super_class_p == known_super_class_p), a_cstr_format("Script Class Name '%s' in SkookumScriptComponent of '%S' is not properly related to Actor. Either the Script Class Name must be derived from Mind, or both the Script Class Name '%s' and the UE4 class of '%S' ('%S') must share the topmost ancestor class known to both SkookumScript and UE4. Right now these ancestor classes are different ('%s' for '%s' and '%s' for '%S').", class_name_ascii.as_cstr(), *actor_p->GetName(), class_name_ascii.as_cstr(), *actor_p->GetName(), *actor_p->GetClass()->GetName(), known_super_class_p ? known_super_class_p->get_name_cstr_dbg() : "<none>", class_name_ascii.as_cstr(), mapped_super_class_p ? mapped_super_class_p->get_name_cstr_dbg() : "<none>", *actor_p->GetClass()->GetName()));
    #endif
    }
  else
    {
  set_default_class:
    // Find most derived UE4 class known to SkookumScript
    class_p = nullptr; // Is already null when we get here, but set again for clarity
    for (UClass * obj_uclass_p = actor_p->GetClass(); !class_p && obj_uclass_p; obj_uclass_p = obj_uclass_p->GetSuperClass())
      {
      class_p = SkUEClassBindingHelper::get_sk_class_from_ue_class(obj_uclass_p);
      }
    SK_ASSERTX(class_p, a_cstr_format("No parent class of %S is known to SkookumScript!", *actor_p->GetClass()->GetName()));
    if (!class_p)
      {
      class_p = SkBrain::get_class(ASymbol_Actor); // Recover from bad user input
      }
    }

  // Based on the desired class, create SkInstance or SkDataInstance
  // Currently, we support only actors and minds
  SK_ASSERTX(class_p->is_actor_class() || class_p->is_mind_class(), a_str_format("Trying to create a SkookumScriptComponent of class '%s' which is neither an actor nor a mind.", class_p->get_name_cstr_dbg()));
  SkInstance * instance_p = class_p->new_instance();
  if (class_p->is_actor_class())
    {
    instance_p->construct<SkUEActor>(actor_p); // Keep track of owner actor
    }
  m_instance_p = instance_p;
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::delete_sk_instance()
  {
  SK_ASSERTX(m_instance_p, "No Sk instance to delete!");
  m_instance_p->clear_coroutines();
  m_instance_p->dereference();
  m_instance_p = nullptr;
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::OnRegister()
  {
  Super::OnRegister();
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::InitializeComponent()
  {
  Super::InitializeComponent();

  // Call SkookumScript constructor, but only if we are located inside the game world
  if (GetOwner()->GetWorld() == SkUEClassBindingHelper::get_world())
    {
    SK_ASSERTX(SkookumScript::is_flag_set(SkookumScript::Flag_evaluate), "SkookumScript must be in initialized state when InitializeComponent() is invoked.");
    create_sk_instance();
    m_instance_p->call_default_constructor();
    }
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::UninitializeComponent()
  {
  // Call SkookumScript destructor, but only if we are located inside the game world
  if (m_instance_p && GetOwner()->GetWorld() == SkUEClassBindingHelper::get_world())
    {
    SK_ASSERTX(SkookumScript::is_flag_set(SkookumScript::Flag_evaluate), "SkookumScript must be in initialized state when UninitializeComponent() is invoked.");
    delete_sk_instance();
    }

  Super::UninitializeComponent();
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::OnUnregister()
  {
  Super::OnUnregister();
  }
