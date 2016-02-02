//=======================================================================================
// SkookumScript C++ library.
// Copyright (c) 2001 Agog Labs Inc.,
// All rights reserved.
//
// Data structure for simplest type of object in language - instance of a
//             class without data members
// Author(s):   Conan Reis
// Notes:          
//=======================================================================================


//=======================================================================================
// Includes
//=======================================================================================

#include <AgogCore/AObjReusePool.hpp>


//=======================================================================================
// SkDataInstance Inline Methods
//=======================================================================================

//---------------------------------------------------------------------------------------
// Constructor
// Returns:    itself
// Arg         class_p - Class type for this instance
// Author(s):   Conan Reis
A_INLINE SkDataInstance::SkDataInstance(
  SkClass * class_p // = nullptr
  ) :
  SkInstance(class_p)
  {
  add_data_members();
  }

//---------------------------------------------------------------------------------------
// 
A_INLINE void SkDataInstance::set_data_by_idx(uint32_t data_idx, SkInstance * obj_p)
  {
  m_data.set_at(data_idx, *obj_p);
  }

//---------------------------------------------------------------------------------------
// Returns dynamic reference pool. Pool created first call and reused on successive calls.
// 
// #Notes
//   Uses Scott Meyers' tip "Make sure that objects are initialized before they're used"
//   from "Effective C++" [Item 47 in 1st & 2nd Editions and Item 4 in 3rd Edition]
//   This is instead of using a non-local static object for a singleton.
//   
// #Modifiers  static
// #Author(s)  Conan Reis
A_INLINE AObjReusePool<SkDataInstance> & SkDataInstance::get_pool()
  {
  static AObjReusePool<SkDataInstance> s_pool(SkookumScript::get_app_info()->get_pool_init_data_instance(), SkookumScript::get_app_info()->get_pool_incr_data_instance());

  return s_pool;
  }

//---------------------------------------------------------------------------------------
//  Retrieves an instance object from the dynamic pool and initializes it for
//              use.  This method should be used instead of 'new' because it prevents
//              unnecessary allocations by reusing previously allocated objects.
// Returns:     a dynamic SkInstance
// See:         pool_delete() 
// Notes:       To 'deallocate' an object that was retrieved with this method, use
//              'pool_delete()' rather than 'delete'.
// Modifiers:    static
// Author(s):    Conan Reis
A_INLINE SkDataInstance * SkDataInstance::new_instance(SkClass * class_p)
  {
  SkDataInstance * instance_p = get_pool().allocate();

  instance_p->m_class_p = class_p;
  instance_p->m_ref_count = 1u;
  instance_p->m_ptr_id = ++ms_ptr_id_prev;
  instance_p->add_data_members();

  return instance_p;
  }

