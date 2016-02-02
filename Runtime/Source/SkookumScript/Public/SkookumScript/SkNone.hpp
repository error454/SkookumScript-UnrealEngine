//=======================================================================================
// SkookumScript C++ library.
// Copyright (c) 2001-2015 Agog Labs Inc. All rights reserved.
//
// Special None class for the single nil instance.
//
// Author: Conan Reis
//=======================================================================================

#pragma once

//=======================================================================================
// Includes
//=======================================================================================

#include <SkookumScript/SkClassBindingAbstract.hpp>


//=======================================================================================
// Global Structures
//=======================================================================================

//---------------------------------------------------------------------------------------
class SK_API SkNone : public SkClassBindingAbstract<SkNone>, public SkInstanceUnreffed
  {
  public:
    
    SK_NEW_OPERATORS(SkNone);

    SkNone();

    static void       initialize();
    static SkClass *  get_class();

  };  // SkNone
