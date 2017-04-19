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

#include "SkookumScriptGeneratorBase.h"

#include <CoreUObject.h>
#include <Regex.h>

#if WITH_EDITOR
  #include "Engine/Blueprint.h"
#endif

//=======================================================================================
// FSkookumScriptGeneratorBase Implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

const FString FSkookumScriptGeneratorBase::ms_sk_type_id_names[FSkookumScriptGeneratorBase::SkTypeID__count] =
  {
  TEXT("nil"),
  TEXT("Integer"),
  TEXT("Real"),
  TEXT("Boolean"),
  TEXT("String"),
  TEXT("Vector2"),
  TEXT("Vector3"),
  TEXT("Vector4"),
  TEXT("Rotation"),
  TEXT("RotationAngles"),
  TEXT("Transform"),
  TEXT("Color"),
  TEXT("Name"),
  TEXT("Enum"),
  TEXT("UStruct"),
  TEXT("EntityClass"),  // UClass
  TEXT("Entity"),       // UObject
  TEXT("List"),
  };

const FString FSkookumScriptGeneratorBase::ms_reserved_keywords[] =
  {
  TEXT("branch"),
  TEXT("case"),
  TEXT("change"),
  TEXT("eh"),
  TEXT("else"),
  TEXT("exit"),
  TEXT("false"),
  TEXT("if"),
  TEXT("loop"),
  TEXT("nil"),
  TEXT("race"),
  TEXT("random"),
  TEXT("rush"),
  TEXT("skip"),
  TEXT("sync"),
  TEXT("this"),
  TEXT("this_class"),
  TEXT("this_code"),
  TEXT("this_mind"),
  TEXT("true"),
  TEXT("unless"),
  TEXT("when"),

  // Boolean word operators
  TEXT("and"),
  TEXT("nand"),
  TEXT("nor"),
  TEXT("not"),
  TEXT("nxor"),
  TEXT("or"),
  TEXT("xor"),
  };

const FName         FSkookumScriptGeneratorBase::ms_meta_data_key_function_category(TEXT("Category"));
const FName         FSkookumScriptGeneratorBase::ms_meta_data_key_blueprint_type(TEXT("BlueprintType"));
const FName         FSkookumScriptGeneratorBase::ms_meta_data_key_display_name(TEXT("DisplayName"));
const FString       FSkookumScriptGeneratorBase::ms_asset_name_key(TEXT("// UE4 Asset Name: "));
const FString       FSkookumScriptGeneratorBase::ms_package_name_key(TEXT("// UE4 Package Name: \""));
const FString       FSkookumScriptGeneratorBase::ms_package_path_key(TEXT("// UE4 Package Path: \""));
TCHAR const * const FSkookumScriptGeneratorBase::ms_editable_ini_settings_p(TEXT("Editable=false\r\nCanMakeEditable=true\r\n"));
TCHAR const * const FSkookumScriptGeneratorBase::ms_overlay_name_bp_p(TEXT("Project-Generated-BP"));
TCHAR const * const FSkookumScriptGeneratorBase::ms_overlay_name_bp_old_p(TEXT("Project-Generated"));
TCHAR const * const FSkookumScriptGeneratorBase::ms_overlay_name_cpp_p(TEXT("Project-Generated-C++"));

const FFileHelper::EEncodingOptions::Type FSkookumScriptGeneratorBase::ms_script_file_encoding = FFileHelper::EEncodingOptions::ForceAnsi;

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::get_or_create_project_file(const FString & ue_project_directory_path, const TCHAR * project_name_p, eSkProjectMode * project_mode_p, bool * created_p)
  {
  eSkProjectMode project_mode = SkProjectMode_editable;
  bool created = false;

  // 1) Check permanent location
  FString project_file_path = ue_project_directory_path / TEXT("Scripts/Skookum-project.ini");
  if (!FPaths::FileExists(project_file_path))
    {
    // No project file exists means we are in read only/REPL only mode
    project_mode = SkProjectMode_read_only;

    // 2) Check/create temp location
    // Check temporary location (in `Intermediate` folder)
    FString temp_root_path(ue_project_directory_path / TEXT("Intermediate/SkookumScript"));
    FString temp_scripts_path(temp_root_path / TEXT("Scripts"));
    project_file_path = temp_scripts_path / TEXT("Skookum-project.ini");
    if (!FPaths::FileExists(project_file_path))
      {
      // If in neither folder, create new project in temporary location
      // $Revisit MBreyer - read ini file from default_project_path and patch it up to carry over customizations
      FString proj_ini = FString::Printf(TEXT("[Project]\r\nProjectName=%s\r\nStrictParse=true\r\nUseBuiltinActor=false\r\nCustomActorClass=Actor\r\nStartupMind=Master\r\n%s"), project_name_p, ms_editable_ini_settings_p);
      proj_ini += TEXT("[Output]\r\nCompileManifest=false\r\nCompileTo=../Content/SkookumScript/Classes.sk-bin\r\n");
      proj_ini += TEXT("[Script Overlays]\r\nOverlay1=*Core|Core\r\nOverlay2=-*Core-Sandbox|Core-Sandbox\r\nOverlay3=*VectorMath|VectorMath\r\nOverlay4=*Engine-Generated|Engine-Generated|A\r\nOverlay5=*Engine|Engine\r\nOverlay6=*");
      proj_ini += ms_overlay_name_bp_p;
      proj_ini += TEXT("|");
      proj_ini += ms_overlay_name_bp_p;
      proj_ini += TEXT("|1\r\nOverlay7=*");
      proj_ini += ms_overlay_name_cpp_p;
      proj_ini += TEXT("|");
      proj_ini += ms_overlay_name_cpp_p;
      proj_ini += TEXT("|A\r\n");
      if (FFileHelper::SaveStringToFile(proj_ini, *project_file_path, FFileHelper::EEncodingOptions::ForceAnsi))
        {
        IFileManager::Get().MakeDirectory(*(temp_root_path / TEXT("Content/SkookumScript")), true);
        IFileManager::Get().MakeDirectory(*(temp_scripts_path / ms_overlay_name_bp_p / TEXT("Object")), true);
        FString overlay_sk = TEXT("$$ .\n");
        if (FFileHelper::SaveStringToFile(overlay_sk, *(temp_scripts_path / ms_overlay_name_cpp_p / TEXT("!Overlay.sk")), FFileHelper::EEncodingOptions::ForceAnsi))
          {
          created = true;
          }
        }
      else
        {
        // Silent failure since we don't want to disturb people's workflow
        project_file_path.Empty();
        }
      }

    // Since project does not exist in the permanent location, make sure the binaries don't exist in the permanent location either
    // Otherwise we'd get inconsistencies/malfunction when binaries are loaded
    FString binary_path_stem(ue_project_directory_path / TEXT("Content/SkookumScript/Classes"));
    IFileManager::Get().Delete(*(binary_path_stem + TEXT(".sk-bin")), false, true, true);
    IFileManager::Get().Delete(*(binary_path_stem + TEXT(".sk-sym")), false, true, true);
    }

  if (project_mode_p) *project_mode_p = project_mode;
  if (created_p) *created_p = created;
  return project_file_path;
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGeneratorBase::compute_scripts_path_depth(FString project_ini_file_path, const FString & overlay_name)
  {
  // Try to figure the path depth from ini file
  m_overlay_path_depth = 1; // Set to sensible default in case we don't find it in the ini file
  FString ini_file_text;
  if (FFileHelper::LoadFileToString(ini_file_text, *project_ini_file_path))
    {
    // Find the substring overlay_name|*|
    FString search_text = overlay_name + TEXT("|");
    int32 pos = 0;
    int32 lf_pos = 0;
    // Skip commented out lines
    do
      {
      pos = ini_file_text.Find(search_text, ESearchCase::CaseSensitive, ESearchDir::FromStart, pos + 1);
      if (pos >= 0)
        {
        // Make sure we are not on a commented out line
        lf_pos = ini_file_text.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, pos);
        }
      } while (pos >= 0 && lf_pos >= 0 && ini_file_text[lf_pos + 1] == ';');

    if (pos >= 0)
      {
      pos += search_text.Len();
      while (ini_file_text[pos] != '|')
        {
        if (ini_file_text[pos] == '\n') return false;
        if (++pos >= ini_file_text.Len()) return false;
        }

      // Look what's behind the last bar
      const TCHAR * depth_text_p = &ini_file_text[pos + 1];

      if (*depth_text_p == 'A')
        {
        m_overlay_path_depth = PathDepth_archived;
        return true;
        }

      int32 path_depth = FCString::Atoi(depth_text_p);
      if (path_depth > 0 || (path_depth == 0 && *depth_text_p == '0'))
        {
        m_overlay_path_depth = path_depth;
        return true;
        }
      }
    }

  return false;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGeneratorBase::save_text_file(const FString & file_path, const FString & contents)
  {
  if (!FFileHelper::SaveStringToFile(contents, *file_path, ms_script_file_encoding))
    {
    report_error(FString::Printf(TEXT("Could not save file: %s"), *file_path));
    }
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGeneratorBase::save_text_file_if_changed(const FString & file_path, const FString & new_file_contents)
  {
  FString original_file_local;
  FFileHelper::LoadFileToString(original_file_local, *file_path);

  const bool has_changed = original_file_local.Len() == 0 || FCString::Strcmp(*original_file_local, *new_file_contents);
  if (has_changed)
    {
    // save the updated version to a tmp file so that the user can see what will be changing
    const FString temp_file_path = file_path + TEXT(".tmp");

    // delete any existing temp file
    IFileManager::Get().Delete(*temp_file_path, false, true);
    if (!FFileHelper::SaveStringToFile(new_file_contents, *temp_file_path, ms_script_file_encoding))
      {
      report_error(FString::Printf(TEXT("Failed to save file: '%s'"), *temp_file_path));
      }
    else
      {
      m_temp_file_paths.AddUnique(temp_file_path);
      }
    }

  return has_changed;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGeneratorBase::flush_saved_text_files(tSourceControlCheckoutFunc checkout_f)
  {
  // Rename temp files
  for (auto & temp_file_path : m_temp_file_paths)
    {
    FString file_path = temp_file_path.Replace(TEXT(".tmp"), TEXT(""));
    IFileManager::Get().Delete(*file_path, false, true, true); // Delete potentially existing version of the file
    if (!IFileManager::Get().Move(*file_path, *temp_file_path, true, true)) // Move new file into its place
      {
      report_error(FString::Printf(TEXT("Couldn't write file '%s'"), *file_path));
      }
    // If source control function provided, make sure file is checked out from source control
    if (checkout_f)
      {
      (*checkout_f)(file_path);
      }
    }

  m_temp_file_paths.Reset();
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGeneratorBase::is_property_type_supported(UProperty * property_p)
  {
  if (property_p->HasAnyPropertyFlags(CPF_EditorOnly)
   || property_p->IsA(ULazyObjectProperty::StaticClass())
   || property_p->IsA(UAssetObjectProperty::StaticClass())
   || property_p->IsA(UAssetClassProperty::StaticClass()))
    {
    return false;
    }

  return (get_skookum_property_type(property_p, false) != SkTypeID_none);
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGeneratorBase::is_struct_type_supported(UStruct * struct_p)
  {
  UScriptStruct * script_struct = Cast<UScriptStruct>(struct_p);  
  return (script_struct && (script_struct->HasDefaults() || (script_struct->StructFlags & STRUCT_RequiredAPI)
#if WITH_EDITOR || HACK_HEADER_GENERATOR
    || script_struct->HasMetaData(ms_meta_data_key_blueprint_type)
#endif
    ));
#if 0
  return script_struct
    && (script_struct->StructFlags & (STRUCT_CopyNative|STRUCT_IsPlainOldData|STRUCT_RequiredAPI))
    && !(script_struct->StructFlags & STRUCT_NoExport);
#endif
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGeneratorBase::is_pod(UStruct * struct_p)
  {
  UScriptStruct * script_struct = Cast<UScriptStruct>(struct_p);
  return (script_struct && (script_struct->StructFlags & STRUCT_IsPlainOldData));
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGeneratorBase::does_class_have_static_class(UClass * class_p)
  {
  return class_p->HasAnyClassFlags(CLASS_RequiredAPI | CLASS_MinimalAPI);
  }

//---------------------------------------------------------------------------------------

UEnum * FSkookumScriptGeneratorBase::get_enum(UField * field_p)
  {
  const UEnumProperty * enum_property_p = Cast<UEnumProperty>(field_p);
  if (enum_property_p)
    {
    return enum_property_p->GetEnum();
    }
  const UByteProperty * byte_property_p = Cast<UByteProperty>(field_p);
  return byte_property_p ? byte_property_p->Enum : nullptr;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::skookify_class_name(const FString & name)
  {
  if (name == TEXT("Object")) return TEXT("Entity");
  if (name == TEXT("Class"))  return TEXT("EntityClass");
  if (name == TEXT("Entity")) return TEXT("GameEntity"); // In case someone defined a class named Entity, make sure it does not collide with SkookumScript's native Entity
  if (name == TEXT("Vector")) return TEXT("Vector3"); // These are the same class
  if (name == TEXT("Enum"))   return TEXT("Enum2"); // HACK to avoid collision with Skookum built-in Enum class

  // SkookumScript shortcuts for static function libraries as their names occur very frequently in code
  if (name == TEXT("DataTableFunctionLibrary"))          return TEXT("DataLib");
  if (name == TEXT("GameplayStatics"))                   return TEXT("GameLib");
  if (name == TEXT("HeadMountedDisplayFunctionLibrary")) return TEXT("VRLib");
  if (name == TEXT("KismetArrayLibrary"))                return TEXT("ArrayLib");
  if (name == TEXT("KismetGuidLibrary"))                 return TEXT("GuidLib");
  if (name == TEXT("KismetInputLibrary"))                return TEXT("InputLib");
  if (name == TEXT("KismetMaterialLibrary"))             return TEXT("MaterialLib");
  if (name == TEXT("KismetMathLibrary"))                 return TEXT("MathLib");
  if (name == TEXT("KismetNodeHelperLibrary"))           return TEXT("NodeLib");
  if (name == TEXT("KismetStringLibrary"))               return TEXT("StringLib");
  if (name == TEXT("KismetSystemLibrary"))               return TEXT("SystemLib");
  if (name == TEXT("KismetTextLibrary"))                 return TEXT("TextLib");
  if (name == TEXT("VisualLoggerKismetLibrary"))         return TEXT("LogLib");

  if (name.IsEmpty()) return TEXT("Unnamed");

  // Make sure class name conforms to Sk naming requirements
  FString skookum_name;
  skookum_name.Reserve(name.Len() + 16);

  bool was_underscore = true;
  for (int32 i = 0; i < name.Len(); ++i)
    {
    TCHAR c = name[i];

    // Ensure first character is uppercase
    if (skookum_name.IsEmpty())
      {
      if (islower(c))
        {
        c = toupper(c);
        }
      else if (!isupper(c))
        {
        // If name starts with neither upper nor lowercase letter, prepend "Sk"
        skookum_name.Append(TEXT("Sk"));
        }
      }

    // Is it [A-Za-z0-9]?
    if ((c >= TCHAR('0') && c <= TCHAR('9'))
     || (c >= TCHAR('A') && c <= TCHAR('Z'))
     || (c >= TCHAR('a') && c <= TCHAR('z')))
      {
      // Yes, append it
      skookum_name.AppendChar(c);
      was_underscore = false;
      }
    else
      {
      // No, insert underscore, but only one
      if (!was_underscore)
        {
        skookum_name.AppendChar('_');
        was_underscore = true;
        }
      }
    }

  return skookum_name;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::skookify_method_name(const FString & name, UProperty * return_property_p)
  {
  FString method_name = skookify_var_name(name, false, VarScope_local);
  bool is_boolean = false;

  // Remove K2 (Kismet 2) prefix if present
  if (method_name.Len() > 3 && !method_name.Mid(3, 1).IsNumeric())
    {
    if (method_name.RemoveFromStart(TEXT("k2_"), ESearchCase::CaseSensitive))
      {
      // Check if removing the k2_ turned it into a Sk reserved word
      if (is_skookum_reserved_word(method_name))
        {
        method_name.AppendChar('_');
        }
      }
    }

  if (method_name.Len() > 4 && !method_name.Mid(4, 1).IsNumeric())
    {
    // If name starts with "get_", remove it
    if (method_name.RemoveFromStart(TEXT("get_"), ESearchCase::CaseSensitive))
      {
      // Check if removing the get_ turned it into a Sk reserved word
      if (is_skookum_reserved_word(method_name))
        {
        method_name.AppendChar('_');
        }

      // Allow question mark
      is_boolean = true;
      }
    // If name starts with "set_", remove it and append "_set" instead
    else if (method_name.RemoveFromStart(TEXT("set_"), ESearchCase::CaseSensitive))
      {
      method_name.Append(TEXT("_set"));
      }
    }

  // If name starts with "is_", "has_" or "can_" also append question mark
  if ((name.Len() > 2 && name[0] == 'b' && isupper(name[1]))
   || method_name.Find(TEXT("is_"), ESearchCase::CaseSensitive) == 0
   || method_name.Find(TEXT("has_"), ESearchCase::CaseSensitive) == 0
   || method_name.Find(TEXT("can_"), ESearchCase::CaseSensitive) == 0)
    {
    is_boolean = true;
    }

  // Append question mark if determined to be boolean
  if (is_boolean && return_property_p && return_property_p->IsA(UBoolProperty::StaticClass()))
    {
    method_name += TEXT("?");
    }

  return method_name;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::skookify_var_name(const FString & name, bool append_question_mark, eVarScope scope)
  {
  if (name.IsEmpty()) return name;

  // Change title case to lower case with underscores
  FString skookum_name;
  skookum_name.Reserve(name.Len() + 16);
  skookum_name.AppendChars(TEXT("@@"), scope == VarScope_instance ? 1 : (scope == VarScope_class ? 2 : 0));
  bool is_boolean = name.Len() > 2 && name[0] == 'b' && isupper(name[1]);
  bool was_upper = true;
  bool was_underscore = true;
  for (int32 i = int32(is_boolean); i < name.Len(); ++i)
    {
    TCHAR c = name[i];

    // Skip special characters
    if (c == TCHAR('?'))
      {
      continue;
      }

    // Is it [A-Za-z0-9]?
    if ((c >= TCHAR('0') && c <= TCHAR('9'))
     || (c >= TCHAR('A') && c <= TCHAR('Z'))
     || (c >= TCHAR('a') && c <= TCHAR('z')))
      {
      // Yes, append it
      bool is_upper = isupper(c) != 0 || isdigit(c) != 0;
      if (is_upper && !was_upper && !was_underscore)
        {
        skookum_name.AppendChar('_');
        }
      skookum_name.AppendChar(tolower(c));
      was_upper = is_upper;
      was_underscore = false;
      }
    else
      {
      // No, insert underscore, but only one
      if (!was_underscore)
        {
        skookum_name.AppendChar('_');
        was_underscore = true;
        }
      }
    }

  // Check for reserved keywords and append underscore if found
  if ((scope == VarScope_local && is_skookum_reserved_word(skookum_name))
   || (scope == VarScope_class && (skookum_name == TEXT("@@world") || skookum_name == TEXT("@@random"))))
    {
    skookum_name.AppendChar('_');
    }

  // Check if there's an MD5 checksum appended to the name - if so, chop it off
  int32 skookum_name_len = skookum_name.Len();
  if (skookum_name_len > 33)
    {
    const TCHAR * skookum_name_p = &skookum_name[skookum_name_len - 33];
    if (skookum_name_p[0] == TCHAR('_'))
      {
      for (int32 i = 1; i <= 32; ++i)
        {
        uint32_t c = skookum_name_p[i];
        if ((c - '0') > 9u && (c - 'a') > 5u) goto no_md5;
        }
      // We chop off most digits of the MD5 and leave only the first four, 
      // assuming that that's distinctive enough for just a few of them at a time
      skookum_name = skookum_name.Left(skookum_name_len - 28);
    no_md5:;
      }
    }

  if (append_question_mark)
    {
    skookum_name.AppendChar(TCHAR('?'));
    }

  return skookum_name;
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGeneratorBase::compare_var_name_skookified(const TCHAR * ue_var_name_p, const ANSICHAR * sk_var_name_p)
  {
  uint32_t ue_len = FCString::Strlen(ue_var_name_p);
  uint32_t sk_len = FCStringAnsi::Strlen(sk_var_name_p);

  // Check if there's an MD5 checksum appended to the name - if so, leave only first four digits
  if (ue_len > 33 && ue_var_name_p[ue_len - 33] == '_')
    {
    for (int32 i = 0; i < 32; ++i)
      {
      uint32_t c = ue_var_name_p[ue_len - 32 + i];
      if ((c - '0') > 9u && (c - 'A') > 5u) goto no_md5;
      }
      // We chop off most digits of the MD5 and leave only the first four, 
      // assuming that that's distinctive enough for just a few of them at a time
      ue_len -= 28;
    no_md5:;
    }

  uint32_t ue_i = uint32_t(ue_len >= 2 && ue_var_name_p[0] == 'b' && FChar::IsUpper(ue_var_name_p[1])); // Skip Boolean `b` prefix if present
  uint32_t sk_i = 0;
  do
    {
    // Skip non-alphanumeric characters
    while (ue_i < ue_len && !FChar::IsAlnum(ue_var_name_p[ue_i])) ++ue_i;
    while (sk_i < sk_len && !FChar::IsAlnum(sk_var_name_p[sk_i])) ++sk_i;
    } while (ue_i < ue_len && sk_i < sk_len && FChar::ToLower(ue_var_name_p[ue_i++]) == FChar::ToLower(sk_var_name_p[sk_i++]));
  // Did we find a match?
  return (ue_i == ue_len && sk_i == sk_len);
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGeneratorBase::is_skookum_reserved_word(const FString & name)
  {
  for (uint32 i = 0; i < sizeof(ms_reserved_keywords) / sizeof(ms_reserved_keywords[0]); ++i)
    {
    if (name == ms_reserved_keywords[i]) return true;
    }

  return false;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::get_skookum_class_name(UField * type_p)
  {
  UObject * obj_p = type_p;
  #if WITH_EDITOR
    UClass * class_p = Cast<UClass>(type_p);
    if (class_p)
      {
      UBlueprint * blueprint_p = UBlueprint::GetBlueprintFromClass(class_p);
      if (blueprint_p) obj_p = blueprint_p;
      }
  #endif
  return skookify_class_name(obj_p->GetName());
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::get_skookum_parent_name(UField * type_p, int32 include_priority, uint32 referenced_flags, UStruct ** out_parent_pp)
  {
  UStruct * struct_or_class_p = Cast<UStruct>(type_p);
  if (struct_or_class_p)
    {
    UStruct * parent_p = struct_or_class_p->GetSuperStruct();
    if (out_parent_pp) *out_parent_pp = parent_p;
    if (!parent_p)
      {
      return get_skookum_struct_type(struct_or_class_p) == SkTypeID_UStruct ? TEXT("UStruct") : TEXT("Object");
      }

    // Mark parent as needed
    on_type_referenced(parent_p, include_priority + 1, referenced_flags);

    return get_skookum_class_name(parent_p);
    }

  if (out_parent_pp) *out_parent_pp = nullptr;
  return TEXT("Enum");
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::get_skookum_class_path(UField * type_p, int32 include_priority, uint32 referenced_flags, FString * out_class_name_p)
  {
  // Remember class name
  FString class_name = get_skookum_class_name(type_p);
  if (out_class_name_p)
    {
    *out_class_name_p = class_name;
    }

  UStruct * struct_or_class_p = Cast<UStruct>(type_p);
  UClass * class_p = Cast<UClass>(struct_or_class_p);
  bool is_class = (class_p != nullptr);

  // Make array of the super classes
  TArray<FSuperClassEntry> super_class_stack;
  super_class_stack.Reserve(32);
  if (struct_or_class_p)
    {
    bool parent_to_sk_ustruct = (get_skookum_struct_type(struct_or_class_p) == SkTypeID_UStruct);
    UStruct * super_p = struct_or_class_p;
    while ((super_p = super_p->GetSuperStruct()) != nullptr)
      {
      super_class_stack.Push(FSuperClassEntry(get_skookum_class_name(super_p), super_p));
      on_type_referenced(super_p, ++include_priority, referenced_flags); // Mark all parents as needed
      // Turn `Vector` into built-in `Vector3`:
      if (get_skookum_struct_type(super_p) != SkTypeID_UStruct)
        {
        parent_to_sk_ustruct = false;
        break;
        }
      }
    // If it's a UStruct, group under virtual parent class "UStruct"
    if (parent_to_sk_ustruct)
      {
      super_class_stack.Push(FSuperClassEntry(TEXT("UStruct"), nullptr));
      }
    }
  else
    {
    // If not struct must be enum at this point or something is fishy
    UEnum * enum_p = CastChecked<UEnum>(type_p);
    super_class_stack.Push(FSuperClassEntry(TEXT("Enum"), nullptr));
    }

  // Build path
  int32 max_super_class_nesting = FMath::Max(m_overlay_path_depth - 1, 0);
  FString class_path = m_overlay_path / TEXT("Object");
  for (int32 i = 0; i < max_super_class_nesting && super_class_stack.Num(); ++i)
    {
    class_path /= super_class_stack.Pop().m_name;
    }
  if (super_class_stack.Num())
    {
    class_name = super_class_stack[0].m_name + TEXT(".") + class_name;
    }
  return class_path / class_name;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::get_skookum_method_file_name(const FString & script_function_name, bool is_static)
  {
  return script_function_name.Replace(TEXT("?"), TEXT("-Q")) + (is_static ? TEXT("()C.sk") : TEXT("().sk"));
  }

//---------------------------------------------------------------------------------------

FSkookumScriptGeneratorBase::eSkTypeID FSkookumScriptGeneratorBase::get_skookum_struct_type(UStruct * struct_p)
  {
  static FName name_Vector2D("Vector2D");
  static FName name_Vector("Vector");
  static FName name_Vector_NetQuantize("Vector_NetQuantize");
  static FName name_Vector_NetQuantizeNormal("Vector_NetQuantizeNormal");  
  static FName name_Vector4("Vector4");
  static FName name_Quat("Quat");
  static FName name_Rotator("Rotator");
  static FName name_Transform("Transform");
  static FName name_LinearColor("LinearColor");
  static FName name_Color("Color");

  const FName struct_name = struct_p->GetFName();

  if (struct_name == name_Vector2D)                 return SkTypeID_Vector2;
  if (struct_name == name_Vector)                   return SkTypeID_Vector3;
  if (struct_name == name_Vector_NetQuantize)       return SkTypeID_Vector3;
  if (struct_name == name_Vector_NetQuantizeNormal) return SkTypeID_Vector3;
  if (struct_name == name_Vector4)                  return SkTypeID_Vector4;
  if (struct_name == name_Quat)                     return SkTypeID_Rotation;
  if (struct_name == name_Rotator)                  return SkTypeID_RotationAngles;
  if (struct_name == name_Transform)                return SkTypeID_Transform;
  if (struct_name == name_Color)                    return SkTypeID_Color;
  if (struct_name == name_LinearColor)              return SkTypeID_Color;

  return Cast<UClass>(struct_p) ? SkTypeID_UClass : SkTypeID_UStruct;
  }

//---------------------------------------------------------------------------------------

FSkookumScriptGeneratorBase::eSkTypeID FSkookumScriptGeneratorBase::get_skookum_property_type(UProperty * property_p, bool allow_all)
  {
  // Check for simple types first
  if (property_p->IsA(UNumericProperty::StaticClass()))
    {
    UNumericProperty * numeric_property_p = static_cast<UNumericProperty *>(property_p);
    if (numeric_property_p->IsInteger() && !numeric_property_p->IsEnum())
      {
      return SkTypeID_Integer;
      }
    }
  if (property_p->IsA(UFloatProperty::StaticClass()))       return SkTypeID_Real;
  if (property_p->IsA(UStrProperty::StaticClass()))         return SkTypeID_String;
  if (property_p->IsA(UNameProperty::StaticClass()))        return SkTypeID_Name;
  if (property_p->IsA(UBoolProperty::StaticClass()))        return SkTypeID_Boolean;

  // Any known struct?
  if (property_p->IsA(UStructProperty::StaticClass()))
    {
    UStructProperty * struct_prop_p = CastChecked<UStructProperty>(property_p);
    eSkTypeID type_id = get_skookum_struct_type(struct_prop_p->Struct);
    return (allow_all || type_id != SkTypeID_UStruct || is_struct_type_supported(struct_prop_p->Struct)) ? type_id : SkTypeID_none;
    }

  if (get_enum(property_p))                                 return SkTypeID_Enum;
  if (property_p->IsA(UClassProperty::StaticClass()))       return SkTypeID_UClass;

  if (property_p->IsA(UObjectPropertyBase::StaticClass()))
    {
    UClass * class_p = Cast<UObjectPropertyBase>(property_p)->PropertyClass;
    if (allow_all || does_class_have_static_class(class_p) || class_p->HasAnyClassFlags(CLASS_HasInstancedReference) || class_p->GetName() == TEXT("Object"))
      {
      return property_p->IsA(UWeakObjectProperty::StaticClass()) ? SkTypeID_UObjectWeakPtr : SkTypeID_UObject;
      }

    return SkTypeID_none;
    }

  if (UArrayProperty * array_property_p = Cast<UArrayProperty>(property_p))
    {
    // Reject arrays of unknown types and arrays of arrays
    return (allow_all || (is_property_type_supported(array_property_p->Inner) && (get_skookum_property_type(array_property_p->Inner, true) != SkTypeID_List))) ? SkTypeID_List : SkTypeID_none;
    }

  // Didn't find a known type
  return SkTypeID_none;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::get_skookum_property_type_name(UProperty * property_p)
  {
  eSkTypeID type_id = get_skookum_property_type(property_p, true);

  if (type_id == SkTypeID_UObject || type_id == SkTypeID_UObjectWeakPtr)
    {
    UObjectPropertyBase * object_property_p = Cast<UObjectPropertyBase>(property_p);
    return get_skookum_class_name(object_property_p->PropertyClass);
    }
  else if (type_id == SkTypeID_UStruct)
    {
    UStruct * struct_p = Cast<UStructProperty>(property_p)->Struct;
    return get_skookum_class_name(struct_p);
    }
  else if (type_id == SkTypeID_Enum)
    {
    UEnum * enum_p = get_enum(property_p);
    return get_skookum_class_name(enum_p);
    }
  else if (type_id == SkTypeID_List)
    {
    return FString::Printf(TEXT("List{%s}"), *get_skookum_property_type_name(Cast<UArrayProperty>(property_p)->Inner));
    }

  return ms_sk_type_id_names[type_id];
  }

//---------------------------------------------------------------------------------------

uint32 FSkookumScriptGeneratorBase::get_skookum_symbol_id(const FString & string)
  {
  char buffer[256];
  char * end_p = FPlatformString::Convert(buffer, sizeof(buffer), *string, string.Len());
  return FCrc::MemCrc32(buffer, end_p - buffer);
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::get_comment_block(UField * field_p)
  {
  #if WITH_EDITOR || HACK_HEADER_GENERATOR
    // Get tool tip from meta data
    FString comment_block = field_p->GetToolTipText().ToString();
    // Convert to comment block
    if (!comment_block.IsEmpty())
      {
      // "Comment out" the comment block
      comment_block = TEXT("// ") + comment_block;
      comment_block.ReplaceInline(TEXT("\n"), TEXT("\n// "));
      comment_block += TEXT("\n");
      // Replace parameter names with their skookified versions
      for (int32 pos = 0;;)
        {
        pos = comment_block.Find(TEXT("@param"), ESearchCase::IgnoreCase, ESearchDir::FromStart, pos);
        if (pos < 0) break;

        pos += 6; // Skip "@param"
        while (pos < comment_block.Len() && FChar::IsWhitespace(comment_block[pos])) ++pos; // Skip white space
        int32 identifier_begin = pos;
        while (pos < comment_block.Len() && FChar::IsIdentifier(comment_block[pos])) ++pos; // Skip identifier
        int32 identifier_length = pos - identifier_begin;
        if (identifier_length > 0)
          {
          // Replace parameter name with skookified version
          FString param_name = skookify_var_name(comment_block.Mid(identifier_begin, identifier_length), false, VarScope_local);
          comment_block.RemoveAt(identifier_begin, identifier_length, false);
          comment_block.InsertAt(identifier_begin, param_name);
          pos += param_name.Len() - identifier_length;
          }
        }
      }

    // Add original name of this object
    FString this_kind =
      field_p->IsA(UFunction::StaticClass()) ? TEXT("method") :
      (field_p->IsA(UClass::StaticClass()) ? TEXT("class") :
      (field_p->IsA(UStruct::StaticClass()) ? TEXT("struct") :
      (field_p->IsA(UProperty::StaticClass()) ? TEXT("property") :
      (get_enum(field_p) ? TEXT("enum") :
      TEXT("field")))));
    comment_block += FString::Printf(TEXT("//\n// UE4 name of this %s: %s\n"), *this_kind, *field_p->GetName());

    // Add display name of this object
    if (field_p->HasMetaData(ms_meta_data_key_display_name))
      {
      FString display_name = field_p->GetMetaData(ms_meta_data_key_display_name);
      comment_block += FString::Printf(TEXT("// Blueprint display name: %s\n"), *display_name);
      }

    // Add Blueprint category
    if (field_p->HasMetaData(ms_meta_data_key_function_category))
      {
      FString category_name = field_p->GetMetaData(ms_meta_data_key_function_category);
      comment_block += FString::Printf(TEXT("// Blueprint category: %s\n"), *category_name);
      }

    return comment_block + TEXT("\n");
  #else
    return FString();
  #endif
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::generate_class_meta_file_body(UField * type_p)
  {
  // Begin with comment bock explaining the class
  FString meta_body = get_comment_block(type_p);

  // Add name and file name of package where it came from if applicable
  bool is_blueprint_class = false;
  #if WITH_EDITOR
    UClass * class_p = Cast<UClass>(type_p);
    if (class_p)
      {
      UBlueprint * blueprint_p = UBlueprint::GetBlueprintFromClass(class_p);
      if (blueprint_p)
        {
        is_blueprint_class = true;

        meta_body += ms_asset_name_key + blueprint_p->GetName() + TEXT("\r\n");
        UPackage * blueprint_package_p = Cast<UPackage>(blueprint_p->GetOuter());
        if (blueprint_package_p)
          {
          meta_body += ms_package_name_key + blueprint_package_p->GetName() + TEXT("\"\r\n");
          meta_body += ms_package_path_key + blueprint_package_p->FileName.ToString() + TEXT("\"\r\n");
          }
        meta_body += TEXT("\r\n");
        }
      }
  #endif

  // Also add annotations
  meta_body += is_blueprint_class ? TEXT("annotations: &reflected_data\r\n") : TEXT("annotations: &reflected_cpp\r\n");

  return meta_body;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGeneratorBase::generate_class_instance_data_file_body(UStruct * struct_or_class_p, int32 include_priority, uint32 referenced_flags)
  {
  FString data_body;

  // Figure out column width of variable types & names
  int32 max_type_length = 0;
  int32 max_name_length = 0;
  for (TFieldIterator<UProperty> property_it(struct_or_class_p, EFieldIteratorFlags::ExcludeSuper); property_it; ++property_it)
    {
    UProperty * var_p = *property_it;
    if (can_export_property(var_p, include_priority, referenced_flags))
      {
      FString type_name = get_skookum_property_type_name(var_p);
      FString var_name = skookify_var_name(var_p->GetName(), var_p->IsA(UBoolProperty::StaticClass()), VarScope_instance);
      max_type_length = FMath::Max(max_type_length, type_name.Len());
      max_name_length = FMath::Max(max_name_length, var_name.Len());
      }
    }

  // Format nicely
  for (TFieldIterator<UProperty> property_it(struct_or_class_p, EFieldIteratorFlags::ExcludeSuper); property_it; ++property_it)
    {
    UProperty * var_p = *property_it;
    FString type_name = get_skookum_property_type_name(var_p);
    FString var_name = skookify_var_name(var_p->GetName(), var_p->IsA(UBoolProperty::StaticClass()), VarScope_instance);
    if (can_export_property(var_p, include_priority, referenced_flags))
      {
      FString comment;
      #if WITH_EDITOR || HACK_HEADER_GENERATOR
        comment = var_p->GetToolTipText().ToString().Replace(TEXT("\n"), TEXT(" "));
      #endif
      data_body += FString::Printf(TEXT("&raw %s !%s // %s%s[%s]\r\n"), *(type_name.RightPad(max_type_length)), *(var_name.RightPad(max_name_length)), *comment, comment.IsEmpty() ? TEXT("") : TEXT(" "), *var_p->GetName());
      }
    else
      {
      data_body += FString::Printf(TEXT("// &raw %s !%s // Currently unsupported\r\n"), *(type_name.RightPad(max_type_length)), *(var_name.RightPad(max_name_length)));
      }
    }

  // Prepend empty line if anything there
  if (!data_body.IsEmpty())
    {
    data_body = TEXT("\r\n") + data_body;
    }

  return data_body;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGeneratorBase::generate_class_meta_file(UField * type_p, const FString & class_path, const FString & skookum_class_name)
  {
  const FString meta_file_path = class_path / TEXT("!Class.sk-meta");
  FString body = type_p ? generate_class_meta_file_body(type_p) : TEXT("// ") + skookum_class_name + TEXT("\r\n");
  if (!FFileHelper::SaveStringToFile(body, *meta_file_path, ms_script_file_encoding))
    {
    report_error(FString::Printf(TEXT("Could not save file: %s"), *meta_file_path));
    }
  }
