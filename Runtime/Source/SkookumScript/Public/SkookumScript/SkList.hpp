//=======================================================================================
// SkookumScript C++ library.
// Copyright (c) 2001-2015 Agog Labs Inc. All rights reserved.
//
// SkookumScript atomic List (dynamic array of typed objects) class
//
// Author: Conan Reis
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include <SkookumScript/SkClassBinding.hpp>
#include <SkookumScript/SkInstanceList.hpp>

//---------------------------------------------------------------------------------------
// SkookumScript atomic List (dynamic array of typed objects) class
class SK_API SkList : public SkClassBindingSimple<SkList, SkInstanceList>
  {
  public:

    static void       register_bindings();
    static SkClass *  get_class();

  };
