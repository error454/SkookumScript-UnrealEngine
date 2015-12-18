//=======================================================================================
// SkookumScript C++ library.
// Copyright (c) 2001 Agog Labs Inc.,
// All rights reserved.
//
// Typed name and typed data classes
// Author(s):   Conan Reis
// Notes:          
//=======================================================================================


#ifndef __SKTYPED_HPP
#define __SKTYPED_HPP


//=======================================================================================
// Includes
//=======================================================================================

#include <AgogCore/ANamed.hpp>
#include <AgogCore/APArray.hpp>
#include <AgogCore/APSorted.hpp>
#include <AgogCore/ARefCount.hpp>
#include <SkookumScript/SkClassDescBase.hpp>


//=======================================================================================
// Global Structures
//=======================================================================================

// Pre-declarations
class SkClassUnaryBase;
class SkInstance;
class SkExpressionBase;


//---------------------------------------------------------------------------------------
// Notes      SkookumScript Typed Name
// See Also   SkQualifier
// Author(s)  Conan Reis
struct SkTypedName : ANamed
  {
	SK_NEW_OPERATORS(SkTypedName);
  
  // Data Members

    // Used as a hint to parser/compiler so that correct data member usage can be assured.
    // $Revisit - CReis In theory this hint should not be needed during run-time if not
    // debugging or parsing - i.e. if only SK_COMPILED_IN is defined.  Currently only used
    // if SK_CODE_IN, SK_CODE_OUT or SK_COMPILED_OUT is defined.]
    ARefPtr<SkClassDescBase> m_type_p;

  // Common Methods

    SkTypedName() : m_type_p(nullptr) {}
    SkTypedName(const ASymbol & name, const SkClassDescBase * type_p);
    SkTypedName(const SkTypedName & source);
    ~SkTypedName();

  // Comparison Methods

    bool operator==(const SkTypedName & typed) const  { return (m_name == typed.m_name) && (m_type_p->compare(*typed.m_type_p) == AEquate_equal); }
    bool operator<(const SkTypedName & typed) const   { return (m_name < typed.m_name) || ((m_name == typed.m_name) && (m_type_p->compare(*typed.m_type_p) == AEquate_less)); }

  // Converter Methods

    #if (SKOOKUM & SK_COMPILED_IN)
      SkTypedName(const void ** binary_pp);
      void assign_binary(const void ** binary_pp);
    #endif

    #if (SKOOKUM & SK_COMPILED_OUT)
      void     as_binary(void ** binary_pp) const;
      uint32_t as_binary_length() const                 { return 4u + m_type_p->as_binary_ref_typed_length(); }
    #endif

  };  // SkTypedName

typedef APSortedLogical<SkTypedName, ASymbol> tSkTypedNames;

//---------------------------------------------------------------------------------------
// SkookumScript Typed Name + Raw data info
struct SkTypedNameRaw : SkTypedName
  {
  SK_NEW_OPERATORS(SkTypedNameRaw);

  // Common Methods

  SkTypedNameRaw() : m_raw_data_info(SkRawDataInfo_Invalid) {}
  SkTypedNameRaw(const ASymbol & name, const SkClassDescBase * type_p) : SkTypedName(name, type_p), m_raw_data_info(SkRawDataInfo_Invalid) {}

  #if (SKOOKUM & SK_COMPILED_IN)
    SkTypedNameRaw(const void ** binary_pp) : SkTypedName(binary_pp) {}
  #endif

  // Data Members

  // User data for the engine to store information about the 
  // offset, size, bit shift etc. of a raw data member if this is one
  // This value is for runtime use only and not serialized into the compiled binary
  tSkRawDataInfo m_raw_data_info;

  };

typedef APSortedLogical<SkTypedNameRaw, ASymbol> tSkTypedNamesRaw;

//---------------------------------------------------------------------------------------
// SkookumScript Typed Name + Runtime Data Index
struct SkTypedNameIndexed : SkTypedName
  {
  SK_NEW_OPERATORS(SkTypedNameIndexed);

  // Common Methods

    SkTypedNameIndexed() : m_data_idx(0), m_has_been_bound(false) {}
    SkTypedNameIndexed(const ASymbol & name, const SkClassDescBase * type_p, uint32_t data_idx) : SkTypedName(name, type_p), m_data_idx(data_idx), m_has_been_bound(false) {}

  // Data Members

    uint32_t  m_data_idx;
    bool      m_has_been_bound; // Keep track if this variable has ever been bound

  };

typedef APSortedLogical<SkTypedNameIndexed, ASymbol> tSkTypedNamesIndexed;


//=======================================================================================
// Inline Functions
//=======================================================================================

#ifndef A_INL_IN_CPP
  #include <SkookumScript/SkTyped.inl>
#endif


#endif  // __SKTYPED_HPP

