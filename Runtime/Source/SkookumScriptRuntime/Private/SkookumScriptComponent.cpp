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

// Global list of all SkookumScript components
AList<USkookumScriptComponent> USkookumScriptComponent::ms_registered_skookumscript_components;

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

  //SkDebug::print_ide(a_str_format("Actor '%S' Ctor() 0x%p\n", *GetOwner()->GetName(), GetOwner()), SkLocale_all, SkDPrintType_trace);
  //SkDebug::print_ide(a_str_format("USkookumScriptComponent::USkookumScriptComponent() 0x%p\n", this), SkLocale_ide, SkDPrintType_trace);
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
      SkClass * mapped_class_p = nullptr;
      for (UClass * obj_uclass_p = actor_p->GetClass(); !mapped_class_p && obj_uclass_p; obj_uclass_p = obj_uclass_p->GetSuperClass())
        {
        mapped_class_p = SkUEClassBindingHelper::get_sk_class_from_ue_class(obj_uclass_p);
        }
      SK_ASSERTX(mapped_class_p && mapped_class_p == known_super_class_p, a_cstr_format("Script Class Name '%s' in SkookumScriptComponent of '%S' is not properly related to Actor. Both the Script Class Name '%s' and the UE4 class of '%S' ('%S') must share the topmost ancestor class known to both SkookumScript and UE4. Right now these ancestor classes are different ('%s' for '%s' and '%s' for '%S').", class_name_ascii.as_cstr(), *actor_p->GetName(), class_name_ascii.as_cstr(), *actor_p->GetName(), *actor_p->GetClass()->GetName(), known_super_class_p ? known_super_class_p->get_name_cstr_dbg() : "<none>", class_name_ascii.as_cstr(), mapped_class_p ? mapped_class_p->get_name_cstr_dbg() : "<none>", *actor_p->GetClass()->GetName()));
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
  SkInstance * instance_p = class_p->new_instance();
  instance_p->construct<SkUEActor>(actor_p); // Keep track of owner actor
  m_instance_p = instance_p;
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::delete_sk_instance()
  {
  SK_ASSERTX(m_instance_p, "No Sk instance to delete!");
  m_instance_p->dereference();
  m_instance_p = nullptr;
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::create_registered_sk_instances()
  {
  for (USkookumScriptComponent * component_p = ms_registered_skookumscript_components.get_first(); component_p != ms_registered_skookumscript_components.get_last(); component_p = component_p->get_next())
    {
    component_p->create_sk_instance();
    }
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::delete_registered_sk_instances()
  {
  for (USkookumScriptComponent * component_p = ms_registered_skookumscript_components.get_first(); component_p != ms_registered_skookumscript_components.get_last(); component_p = component_p->get_next())
    {
    if (component_p->m_instance_p)
      {
      component_p->delete_sk_instance();
      }
    }
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::OnRegister()
  {
  Super::OnRegister();

  ms_registered_skookumscript_components.append(this);

  if (SkookumScript::is_flag_set(SkookumScript::Flag_evaluate) 
   && GetOwner()->GetWorld() == SkUEClassBindingHelper::get_world())
    {
    create_sk_instance();
    }
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::InitializeComponent()
  {
  Super::InitializeComponent();

  // Call SkookumScript constructor, but only if we are located inside the game world
  if (GetOwner()->GetWorld() == SkUEClassBindingHelper::get_world())
    {
    //SkDebug::print_ide(a_str_format("Actor '%S' InitializeComponent() A:%p C:%p\n", *owner_p->GetName(), owner_p, this), SkLocale_all, SkDPrintType_trace);
    //SkDebug::print_ide(a_str_format("USkookumScriptComponent::InitializeComponent() 0x%p\n", this), SkLocale_ide, SkDPrintType_trace);
    SK_ASSERTX(SkookumScript::is_flag_set(SkookumScript::Flag_evaluate), "SkookumScript must be in initialized state when InitializeComponent() is invoked.");
    m_instance_p->call_default_constructor();
    }
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::UninitializeComponent()
  {
  Super::UninitializeComponent();

  // Call SkookumScript destructor, but only if we are located inside the game world
  if (GetOwner()->GetWorld() == SkUEClassBindingHelper::get_world())
    {
    //SkDebug::print_ide(a_str_format("USkookumScriptComponent::UninitializeComponent() 0x%p\n", this), SkLocale_ide, SkDPrintType_trace);
    SK_ASSERTX(SkookumScript::is_flag_set(SkookumScript::Flag_evaluate), "SkookumScript must be in initialized state when UninitializeComponent() is invoked.");
    m_instance_p->call_destructor();
    }
  }

//---------------------------------------------------------------------------------------
void USkookumScriptComponent::OnUnregister()
  {
  Super::OnUnregister();

  ms_registered_skookumscript_components.remove(this);

  //SkDebug::print_ide(a_str_format("USkookumScriptComponent::OnUnregister() 0x%p\n", this), SkLocale_ide, SkDPrintType_trace);
  if (SkookumScript::is_flag_set(SkookumScript::Flag_evaluate) && m_instance_p)
    {
    delete_sk_instance();
    }
  }
