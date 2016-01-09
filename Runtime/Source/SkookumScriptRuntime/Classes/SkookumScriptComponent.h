//=======================================================================================
// SkookumScript Plugin for Unreal Engine 4
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// SkookumScript Actor Component
// 
// Author: Conan Reis, Markus Breyer
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include "Components/ActorComponent.h"
#include "SkookumScriptComponent.generated.h"


//=======================================================================================
// Global Defines / Macros
//=======================================================================================


//=======================================================================================
// Global Structures
//=======================================================================================

//---------------------------------------------------------------------------------------
// Adds SkookumScript text-based scripting capabilities to an actor.
UCLASS(classGroup=Scripting, editinlinenew, BlueprintType, meta=(BlueprintSpawnableComponent), hideCategories=(Object, ActorComponent), EarlyAccessPreview)
class SKOOKUMSCRIPTRUNTIME_API USkookumScriptComponent : public UActorComponent, public AListNode<USkookumScriptComponent>
  {

    GENERATED_UCLASS_BODY()

  public:

  // Public Data Members

    // SkookumScript class type - used to create appropriate Skookum object instance
    // Uses class of this actor's Blueprint if left blank.
    UPROPERTY(Category = Script, EditAnywhere, BlueprintReadOnly)
    FString ScriptClassName;


  // Methods

    // Gets our SkookumScript instance
    SkInstance * get_sk_instance() const { return m_instance_p; }

    // Begin UActorComponent interface

      virtual void OnRegister() override;
      virtual void InitializeComponent() override;
      virtual void UninitializeComponent() override;
      virtual void OnUnregister() override;

  protected:

    // Creates/deletes our SkookumScript instance
    void create_sk_instance();
    void delete_sk_instance();

    // Keep the SkookumScript instance belonging to this actor around
    AIdPtr<SkInstance> m_instance_p;

  };  // USkookumScriptComponent


//=======================================================================================
// Inline Functions
//=======================================================================================

