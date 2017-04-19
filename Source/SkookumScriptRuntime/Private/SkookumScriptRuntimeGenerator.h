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
// SkookumScript Unreal Engine Runtime Script Generator
//=======================================================================================

#pragma once

#include "../../SkookumScriptGenerator/Private/SkookumScriptGeneratorBase.h" // Need this even in cooked builds

#if WITH_EDITORONLY_DATA

//---------------------------------------------------------------------------------------

class ISkookumScriptRuntimeInterface
  {
  public:

    // Types

    enum eChangeType
      {
      ChangeType_created,
      ChangeType_modified,
      ChangeType_deleted
      };

    // Methods

    virtual bool  is_static_class_known_to_skookum(UClass * class_p) const = 0;
    virtual bool  is_static_struct_known_to_skookum(UStruct * struct_p) const = 0;
    virtual bool  is_static_enum_known_to_skookum(UEnum * enum_p) const = 0;

    virtual void  on_class_scripts_changed_by_generator(const FString & class_name, eChangeType change_type) = 0;
    virtual bool  check_out_file(const FString & file_path) const = 0;

  };

//---------------------------------------------------------------------------------------

class FSkookumScriptRuntimeGenerator : public FSkookumScriptGeneratorBase
  {
  public:

    // Methods

                   FSkookumScriptRuntimeGenerator(ISkookumScriptRuntimeInterface * runtime_interface_p);
                   ~FSkookumScriptRuntimeGenerator();

    eSkProjectMode get_project_mode() const { return m_project_mode; }
    FString        get_project_file_path();
    FString        get_default_project_file_path();
    int32          get_overlay_path_depth() const;
    void           delete_all_class_script_files();
    void           generate_all_class_script_files();
    FString        make_project_editable();
    UBlueprint *   load_blueprint_asset(const FString & class_path, bool * sk_class_deleted_p);

    void           generate_class_script_files(UClass * ue_class_p, bool generate_data, bool even_if_not_game_class, bool check_if_reparented);
    void           rename_class_script_files(UClass * ue_class_p, const FString & old_class_name);
    void           rename_class_script_files(UClass * ue_class_p, const FString & old_class_name, const FString & new_class_name);
    void           delete_class_script_files(UClass * ue_class_p);
    void           generate_used_class_script_files();

    // FSkookumScriptGeneratorBase interface implementation

    virtual bool   can_export_property(UProperty * property_p, int32 include_priority, uint32 referenced_flags) override final;
    virtual void   on_type_referenced(UField * type_p, int32 include_priority, uint32 referenced_flags) override final;
    virtual void   report_error(const FString & message) override final;

  protected:

    void           initialize_paths();
    void           set_overlay_path();

    // Types

    typedef TSet<UStruct *> tUsedClasses;

    // Data members

    ISkookumScriptRuntimeInterface * m_runtime_interface_p;

    eSkProjectMode  m_project_mode;

    FString         m_project_file_path;
    FString         m_default_project_file_path;

    FString         m_package_name_key;
    FString         m_package_path_key;

    tUsedClasses    m_used_classes;       // All classes used as types (by parameters, properties etc.)

  };

#endif // WITH_EDITORONLY_DATA