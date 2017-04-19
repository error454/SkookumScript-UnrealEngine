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
// SkookumScript C++ library.
//
// Actor class - i.e. named simulation objects with a list of all instances stored in
// their classes
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include <SkookumScript/SkDataInstance.hpp>
#include <SkookumScript/SkClassBindingAbstract.hpp>

//=======================================================================================
// Global Structures
//=======================================================================================

// Pre-declarations
class SkActorClass;

//---------------------------------------------------------------------------------------
// Named simulation object with a list of all instances stored in their classes (SkActorClass).
// 
// $Revisit - CReis Some actors may not need data members so there could be a version of
// `SkActor` that derives from `SkInstance` rather than `SkDataInstance`.
class SK_API SkActor :
  public SkClassBindingAbstract<SkActor>, // Adds static class pointer
  public ANamed,         // Adds name
  public SkDataInstance  // Adds instance info and data members.
  {
  public:

  // Common Methods

    SK_NEW_OPERATORS(SkActor);

    SkActor(const ASymbol & name = ASymbol::get_null(), SkActorClass * class_p = nullptr, bool add_to_instance_list = true);
    virtual ~SkActor() override;

  // Accessor Methods


  // Methods

    AString        as_string() const;
    SkActorClass * get_class_actor() const                         { return reinterpret_cast<SkActorClass *>(m_class_p); }
    void           rename(const ASymbol & name);

    // Overriding from SkInstance -> SkDataInstance

      virtual void delete_this() override;

    // Overriding from SkInstance

       #if defined(SK_AS_STRINGS)
         virtual AString         as_string_debug() const override  { return as_string(); }
         virtual const ASymbol & get_name_debug() const override   { return m_name; }
       #endif

    // Overriding from SkObjectBase

      virtual eSkObjectType get_obj_type() const override               { return SkObjectType_actor; } 

 // Class Methods

    static AString generate_unique_name_str(const AString & name_root, uint32_t * create_idx_p = nullptr);
    static ASymbol generate_unique_name_sym(const AString & name_root, uint32_t * create_idx_p = nullptr);


 // SkookumScript Atomics

    static void register_bindings();

  };  // SkActor


//=======================================================================================
// Inline Methods
//=======================================================================================

#ifndef A_INL_IN_CPP
  #include <SkookumScript/SkActor.inl>
#endif
