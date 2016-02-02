//=======================================================================================
// SkookumScript C++ library.
// Copyright (c) 2001-2015 Agog Labs Inc. All rights reserved.
//
// SkookumScript atomic Boolean (true/false) class - allows short circuit evaluation.
//
// Author: Conan Reis
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include <SkookumScript/SkClassBinding.hpp>


//---------------------------------------------------------------------------------------
// Notes      SkookumScript Atomic Boolean (true/false) - allows short circuit evaluation.
//            Has same data as SkInstance - only differs in that it has a different
//            virtual method table.
class SK_API SkBoolean : public SkClassBindingSimpleZero<SkBoolean, SkBooleanType>
  {
  public:

    static void       register_bindings();
    static SkClass *  get_class();

  };

