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
// SkookumScript Plugin for Unreal Engine 4
//=======================================================================================

#pragma once

#include "Misc/FileHelper.h"

//---------------------------------------------------------------------------------------
// Mode the plugin is currently in 
enum eSkProjectMode
  {
  SkProjectMode_read_only, // IDE is used as a REPL only, no scripts exist in project itself
  SkProjectMode_editable   // Project depends on Sk scripts and plugin features
  };

//---------------------------------------------------------------------------------------
// This class provides functionality for processing UE4 runtime type information
// and for generating Sk script files

class FSkookumScriptGeneratorBase
  {
  public:

    //---------------------------------------------------------------------------------------
    // Types

    enum eSkTypeID
      {
      SkTypeID_none = 0,
      SkTypeID_Integer,
      SkTypeID_Real,
      SkTypeID_Boolean,
      SkTypeID_String,
      SkTypeID_Vector2,
      SkTypeID_Vector3,
      SkTypeID_Vector4,
      SkTypeID_Rotation,
      SkTypeID_RotationAngles,
      SkTypeID_Transform,
      SkTypeID_Color,
      SkTypeID_Name,
      SkTypeID_Enum,
      SkTypeID_UStruct,
      SkTypeID_UClass,
      SkTypeID_UObject,
      SkTypeID_UObjectWeakPtr,
      SkTypeID_List,

      SkTypeID__count
      };

    typedef bool (*tSourceControlCheckoutFunc)(const FString & file_path);

    struct FSuperClassEntry
      {
      FString   m_name;
      UStruct * m_class_or_struct_p;

      FSuperClassEntry(const FString & name, UStruct * class_or_struct_p) : m_name(name), m_class_or_struct_p(class_or_struct_p) {}
      };

    // (keep these in sync with the same enum in SkTextProgram.hpp)
    enum ePathDepth
      {
      PathDepth_any      = -1, // No limit to path depth
      PathDepth_archived = -2  // All chunks stored in a single archive file
      };

    // What kind of variable we are dealing with
    enum eVarScope
      {
      VarScope_local,
      VarScope_instance,
      VarScope_class
      };

    //---------------------------------------------------------------------------------------
    // Interface

    enum eReferenced
      {
      Referenced_by_game_module   = 1 << 0,
      Referenced_as_binding_class = 1 << 1,
      };

    virtual bool          can_export_property(UProperty * property_p, int32 include_priority, uint32 referenced_flags) = 0;
    virtual void          on_type_referenced(UField * type_p, int32 include_priority, uint32 referenced_flags) = 0;
    virtual void          report_error(const FString & message) = 0;

    //---------------------------------------------------------------------------------------
    // Methods

    static FString        get_or_create_project_file(const FString & ue_project_directory_path, const TCHAR * project_name_p, eSkProjectMode * project_mode_p = nullptr, bool * created_p = nullptr);
    bool                  compute_scripts_path_depth(FString project_ini_file_path, const FString & overlay_name);
    void                  save_text_file(const FString & file_path, const FString & contents);
    bool                  save_text_file_if_changed(const FString & file_path, const FString & new_file_contents); // Helper to change a file only if needed
    void                  flush_saved_text_files(tSourceControlCheckoutFunc checkout_f = nullptr); // Puts generated files into place after all code generation is done

    static bool           is_property_type_supported(UProperty * property_p);
    static bool           is_struct_type_supported(UStruct * struct_p);
    static bool           is_pod(UStruct * struct_p);
    static bool           does_class_have_static_class(UClass * class_p);
    static UEnum *        get_enum(UField * field_p); // Returns the Enum if it is an enum, nullptr otherwise

    static FString        skookify_class_name(const FString & name);
    static FString        skookify_method_name(const FString & name, UProperty * return_property_p = nullptr);
    static FString        skookify_var_name(const FString & name, bool append_question_mark, eVarScope scope);
    static bool           compare_var_name_skookified(const TCHAR * ue_var_name_p, const ANSICHAR * sk_var_name_p);
    static bool           is_skookum_reserved_word(const FString & name);
    static FString        get_skookum_class_name(UField * type_p);
    FString               get_skookum_parent_name(UField * type_p, int32 include_priority, uint32 referenced_flags, UStruct ** out_parent_pp = nullptr);
    FString               get_skookum_class_path(UField * type_p, int32 include_priority, uint32 referenced_flags, FString * out_class_name_p = nullptr);
    FString               get_skookum_method_file_name(const FString & script_function_name, bool is_static);
    static eSkTypeID      get_skookum_struct_type(UStruct * struct_p);
    static eSkTypeID      get_skookum_property_type(UProperty * property_p, bool allow_all);
    static FString        get_skookum_property_type_name(UProperty * property_p);
    static uint32         get_skookum_symbol_id(const FString & string);
    static FString        get_comment_block(UField * field_p);

    FString               generate_class_meta_file_body(UField * type_p);
    FString               generate_class_instance_data_file_body(UStruct * class_or_struct_p, int32 include_priority, uint32 referenced_flags);

    void                  generate_class_meta_file(UField * type_p, const FString & class_path, const FString & skookum_class_name);

    //---------------------------------------------------------------------------------------
    // Data

    static const FFileHelper::EEncodingOptions::Type  ms_script_file_encoding;

    static const FString        ms_sk_type_id_names[SkTypeID__count]; // Names belonging to the ids above
    static const FString        ms_reserved_keywords[]; // = Forbidden variable names
    static const FName          ms_meta_data_key_function_category;
    static const FName          ms_meta_data_key_blueprint_type;
    static const FName          ms_meta_data_key_display_name;
    static const FString        ms_asset_name_key; // Label used to extract asset name from Sk class meta file
    static const FString        ms_package_name_key; // Label used to extract package name from Sk class meta file
    static const FString        ms_package_path_key; // Label used to extract package path from Sk class meta file
    static TCHAR const * const  ms_editable_ini_settings_p; // ini file settings to describe that a project is not editable
    static TCHAR const * const  ms_overlay_name_bp_p; // Name of overlay used for Sk classes generated from Blueprints
    static TCHAR const * const  ms_overlay_name_bp_old_p; // Legacy 2016-07-18 - remove after some time
    static TCHAR const * const  ms_overlay_name_cpp_p; // Name of overlay used for Sk classes generated from C++ reflection macros

    FString               m_overlay_path;       // Folder where to place generated script files
    int32                 m_overlay_path_depth; // Amount of super classes until we start flattening the script file hierarchy due to the evil reign of Windows MAX_PATH. 1 = everything is right under 'Object', 0 is not allowed, -1 means "no limit" and -2 means single archive file

    TArray<FString>       m_temp_file_paths;    // Keep track of temp files generated by save_files_if_changed()
  };
