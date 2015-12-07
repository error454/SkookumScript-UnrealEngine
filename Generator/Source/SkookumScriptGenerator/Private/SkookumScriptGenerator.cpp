//=======================================================================================
// SkookumScript Unreal Engine Binding Generator
// Copyright (c) 2015 Agog Labs Inc. All rights reserved.
//
// Author: Markus Breyer
//
// Adapted in parts from sample code by Robert Manuszewski of Epic Games Inc.
//=======================================================================================

#include "SkookumScriptGeneratorPrivatePCH.h"

#include "SkookumScriptGeneratorBase.inl"

//#define USE_DEBUG_LOG_FILE

DEFINE_LOG_CATEGORY(LogSkookumScriptGenerator);

//---------------------------------------------------------------------------------------

class FSkookumScriptGenerator : public ISkookumScriptGenerator, public FSkookumScriptGeneratorBase
  {

  //---------------------------------------------------------------------------------------
  // IModuleInterface implementation

  virtual void    StartupModule() override;
  virtual void    ShutdownModule() override;

  //---------------------------------------------------------------------------------------
  // IScriptGeneratorPluginInterface implementation

  virtual FString GetGeneratedCodeModuleName() const override { return TEXT("SkookumScriptRuntime"); }
  virtual bool    ShouldExportClassesForModule(const FString & module_name, EBuildModuleType::Type module_type, const FString & module_generated_include_folder) const;
  virtual bool    SupportsTarget(const FString & target_name) const override { return true; }
  virtual void    Initialize(const FString & root_local_path, const FString & root_build_path, const FString & output_directory, const FString & include_base) override;
  virtual void    ExportClass(UClass * class_p, const FString & source_header_file_name, const FString & generated_header_file_name, bool has_changed) override;
  virtual void    FinishExport() override;
  virtual FString GetGeneratorName() const override;

  //---------------------------------------------------------------------------------------

  // To keep track of bindings generated for a particular class
  struct MethodBinding
    {
    void make_method(UFunction * function_p); // create names for a method
    void make_property_getter(UProperty * property_p);
    void make_property_setter(UProperty * property_p);

    bool operator == (const MethodBinding & other) const { return m_script_name == other.m_script_name; }

    FString   m_script_name;
    FString   m_code_name;
    };

  // To keep track of classes for later exporting
  struct ClassRecord
    {
    ClassRecord(UClass * class_p, const FString & source_header_file_name) : m_class_p(class_p), m_source_header_file_name(source_header_file_name) {}

    bool operator == (const ClassRecord & other) const { return m_class_p == other.m_class_p; }

    UClass *  m_class_p;
    FString   m_source_header_file_name;
    };

  //---------------------------------------------------------------------------------------
  // Data

  FString               m_binding_code_path; // Output folder for generated binding code files
  FString               m_unreal_engine_root_path_local; // Root of "Unreal Engine" folder on local machine
  FString               m_unreal_engine_root_path_build; // Root of "Unreal Engine" folder for builds - may be different to m_unreal_engine_root_local if we're building remotely
  FString               m_runtime_plugin_root_path; // Root of the runtime plugin we're generating the code for - used as base path for include files

  TArray<FString>       m_all_header_file_names; // Keep track of all headers generated
  TArray<FString>       m_all_binding_file_names; // Keep track of all binding files generated

  TSet<UStruct *>       m_exported_classes; // Whenever a class or struct gets exported, it gets added to this list
  TArray<ClassRecord>   m_extra_classes; // Classes rejected to export at first, but possibly exported later if ever used
  TArray<FString>       m_skip_classes; // All classes set to skip in UHT config file (Engine/Programs/UnrealHeaderTool/Config/DefaultEngine.ini)

  TSet<UEnum *>         m_exported_enums;

  TSet<UObject *>       m_class_packages;

  static const FName    ms_meta_data_key_custom_structure_param;
  static const FName    ms_meta_data_key_array_parm;
  static const FName    ms_meta_data_key_module_relative_path;

#ifdef USE_DEBUG_LOG_FILE
  FILE *                m_debug_log_file; // Quick file handle to print debug stuff to, generates log file in output folder
#endif

  //---------------------------------------------------------------------------------------
  // Methods

  void                  generate_class(UClass * class_p, const FString & source_header_file_name); // Generate script and binding files for a class and its methods and properties
  void                  generate_class_script_files(UStruct * class_or_struct_p); // Generate script files for a class and its methods and properties 
  void                  generate_class_header_file(UStruct * class_or_struct_p, const FString & source_header_file_name); // Generate header file for a class
  void                  generate_class_binding_file(UStruct * class_or_struct_p); // Generate binding code source file for a class or struct

  void                  generate_struct_from_property(UProperty * prop_p); // Generate script and binding files for a struct from property
  void                  generate_struct(UStruct * struct_p); // Generate script and binding files for a struct

  void                  generate_enum_from_property(UProperty * prop_p); // Generate files for an enum
  void                  generate_enum(UEnum * enum_p); // Generate files for an enum
  void                  generate_enum_script_files(UEnum * enum_p); // Generate script files for an enum
  void                  generate_enum_binding_files(); // Generate header and inline file that declares and defines all exported enums

  FString               generate_method(const FString & class_name_cpp, UClass * class_p, UFunction * function_p, const MethodBinding & binding); // Generate script file and binding code for a method
  void                  generate_method_script_file(UFunction * function_p, const FString & script_function_name); // Generate script file for a method
  FString               generate_method_binding_code(const FString & class_name_cpp, UClass * class_p, UFunction * function_p, const MethodBinding & binding); // Generate binding code for a method
  FString               generate_method_binding_code_body_via_call(const FString & class_name_cpp, UClass * class_p, UFunction * function_p, const MethodBinding & binding); // Generate binding code for a method
  FString               generate_method_binding_code_body_via_event(const FString & class_name_cpp, UClass * class_p, UFunction * function_p, const MethodBinding & binding); // Generate binding code for a method

  FString               generate_method_binding_declaration(const FString & function_name, bool is_static); // Generate declaration of method binding function
  FString               generate_this_pointer_initialization(const FString & class_name_cpp, UStruct * class_or_struct_p, bool is_static); // Generate code that obtains the 'this' pointer from scope_p
  FString               generate_method_parameter_assignment(UProperty * param_p, int32 param_index, FString assignee_name);
  FString               generate_method_out_parameter_expression(UFunction * function_p, UProperty * param_p, int32 param_index, const FString & param_name);
  FString               generate_property_default_ctor_argument(UProperty * param_p);

  FString               generate_return_value_passing(UProperty * return_value_p, const FString & return_value_name); // Generate code that passes back the return value

  void                  generate_master_binding_file(); // Generate master source file that includes all others

  bool                  can_export_class(UClass * class_p, const FString & source_header_file_name) const;
  bool                  can_export_struct(UStruct * struct_p);
  bool                  can_export_enum(UEnum * enum_p);
  bool                  can_export_method(UClass * class_p, UFunction * function_p);
  bool                  can_export_property(UStruct * class_or_struct_p, UProperty * property_p);

  FString               get_skookum_property_type_name(UProperty * property_p);
  FString               get_skookum_property_binding_class_name(UProperty * property_p);
  static FString        get_cpp_class_name(UStruct * class_or_struct_p);
  static FString        get_cpp_property_type_name(UProperty * property_p);
  static FString        get_cpp_property_cast_name(UProperty * property_p); // Returns the type to be used to cast an assignment before assigning
  FString               get_skookum_default_initializer(UFunction * function_p, UProperty * param_p);

  };

IMPLEMENT_MODULE(FSkookumScriptGenerator, SkookumScriptGenerator)

//=======================================================================================
// IModuleInterface implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::StartupModule()
  {
  IModularFeatures::Get().RegisterModularFeature(TEXT("ScriptGenerator"), this);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::ShutdownModule()
  {
  IModularFeatures::Get().UnregisterModularFeature(TEXT("ScriptGenerator"), this);
  }

//=======================================================================================
// IScriptGeneratorPluginInterface implementation
//=======================================================================================

const FName FSkookumScriptGenerator::ms_meta_data_key_custom_structure_param(TEXT("CustomStructureParam"));
const FName FSkookumScriptGenerator::ms_meta_data_key_array_parm(TEXT("ArrayParm"));
const FName FSkookumScriptGenerator::ms_meta_data_key_module_relative_path(TEXT("ModuleRelativePath"));

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::Initialize(const FString & root_local_path, const FString & root_build_path, const FString & output_directory, const FString & include_base)
  {
  m_binding_code_path = output_directory;
  m_unreal_engine_root_path_local = root_local_path;
  m_unreal_engine_root_path_build = root_build_path;
  m_runtime_plugin_root_path = include_base;

  // Set up output path for scripts
  m_overlay_path = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*(include_base / TEXT("../../Scripts/Engine-Generated")));
  compute_scripts_path_depth(m_overlay_path / TEXT("../Skookum-project-default.ini"), TEXT("Engine-Generated"));

  // Clear contents of scripts folder for a fresh start
  FString directory_to_delete(m_overlay_path / TEXT("Object"));
  IFileManager::Get().DeleteDirectory(*directory_to_delete, false, true);

  // Fetch from ini file which classes to skip during script generation
  // [SkookumScriptGenerator]
  // +SkipClasses=ClassName1
  // +SkipClasses=ClassName2
  GConfig->GetArray(TEXT("SkookumScriptGenerator"), TEXT("SkipClasses"), m_skip_classes, GEngineIni);

  // Create debug log file
#ifdef USE_DEBUG_LOG_FILE
  m_debug_log_file = _wfopen(*(output_directory / TEXT("SkookumScriptGenerator.log.txt")), TEXT("w"));
#endif
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::ShouldExportClassesForModule(const FString & module_name, EBuildModuleType::Type module_type, const FString & module_generated_include_folder) const
  {
  bool can_export = (module_type == EBuildModuleType::Runtime || module_type == EBuildModuleType::Game);
  if (can_export)
    {
    // Only export functions from selected modules
    static struct FSupportedModules
      {
      TArray<FString> supported_script_modules;
      FSupportedModules()
        {
        GConfig->GetArray(TEXT("Plugins"), TEXT("ScriptSupportedModules"), supported_script_modules, GEngineIni);
        }
      } supported_modules;

    can_export = supported_modules.supported_script_modules.Num() == 0 || supported_modules.supported_script_modules.Contains(module_name);
    }
  return can_export;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::ExportClass(UClass * class_p, const FString & source_header_file_name, const FString & generated_header_file_name, bool has_changed)
  {
  // $Revisit MBreyer - (for now) skip and forget classes coming from engine plugins
  if (source_header_file_name.Find(TEXT("Engine/Plugins")) >= 0
    || source_header_file_name.Find(TEXT("Engine\\Plugins")) >= 0)
    {
    return;
    }

  if (!can_export_class(class_p, source_header_file_name))
    {
    m_extra_classes.AddUnique(ClassRecord(class_p, source_header_file_name));
    return;
    }

  generate_class(class_p, source_header_file_name);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::FinishExport()
  {
  // Generate any classes that have been used but not exported yet
  for (auto & extra_class : m_extra_classes)
    {
    // Generate it if it's been used anywhere
    if (m_used_classes.Find(extra_class.m_class_p) >= 0)
      {
      generate_class(extra_class.m_class_p, extra_class.m_source_header_file_name);
      }
    }

  // Now export all structs contained in the same packages as the classes
  TArray<UObject *> obj_array;
  GetObjectsOfClass(UScriptStruct::StaticClass(), obj_array, false, RF_ClassDefaultObject | RF_PendingKill);
  for (auto obj_p : obj_array)
    {
    if (m_class_packages.Contains(obj_p->GetOuter()))
      {
      UScriptStruct * script_struct_p = CastChecked<UScriptStruct>(obj_p);
      generate_struct(script_struct_p);
      }
    }

  #if 0
    // This does currently not work for enums that are within WITH_EDITOR or WITH_EDITORONLY_DATA blocks
    // as we cannot detect from the UEnum if that's the case, causing compile errors in cooked builds

    // Export all enums
    obj_array.Reset();
    GetObjectsOfClass(UEnum::StaticClass(), obj_array, false, RF_ClassDefaultObject | RF_PendingKill);
    for (auto obj_p : obj_array)
      {
      if (m_class_packages.Contains(obj_p->GetOuter()))
        {
        generate_enum(static_cast<UEnum *>(obj_p));
        }
      }
  #endif

  // Now remove all exported classes from the used classes list and see if anything is left
  for (auto exported_class_p : m_exported_classes)
    {
    m_used_classes.Remove(exported_class_p);
    }
  // Anything left is a struct or class that was never seen in IScriptGeneratorPluginInterface::ExportClass() but is needed for the code to compile
  // Export these too and hope for the best
  while (m_used_classes.Num() > 0)
    {
    UStruct * struct_p = m_used_classes.Pop();
    UClass * class_p = Cast<UClass>(struct_p);
    if (class_p)
      {
      generate_class(class_p, class_p->GetMetaData(ms_meta_data_key_module_relative_path));
      }
    else
      {
      generate_struct(struct_p);
      }
    }

  generate_enum_binding_files();
  generate_master_binding_file();
  flush_saved_text_files();

#ifdef USE_DEBUG_LOG_FILE
  fclose(m_debug_log_file);
#endif
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::GetGeneratorName() const
  {
  return TEXT("SkookumScript Binding Generator Plugin");
  }

//=======================================================================================
// FSkookumScriptGenerator implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_class(UClass * class_p, const FString & source_header_file_name)
  {
  UE_LOG(LogSkookumScriptGenerator, Log, TEXT("Generating class %s"), *get_skookum_class_name(class_p));

  m_exported_classes.Add(class_p);
  m_class_packages.Add(class_p->GetOuterUPackage());

  // Generate script files
  generate_class_script_files(class_p);

  // Generate binding code files	
  generate_class_header_file(class_p, source_header_file_name);
  generate_class_binding_file(class_p);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_class_script_files(UStruct * class_or_struct_p)
  {
  const FString class_path = get_skookum_class_path(class_or_struct_p);
  const FString skookum_class_name = get_skookum_class_name(class_or_struct_p);

  // Create class meta file:
  const FString meta_file_path = class_path / TEXT("!Class.sk-meta");
  FString body = get_comment_block(class_or_struct_p);
  if (!FFileHelper::SaveStringToFile(body, *meta_file_path, ms_script_file_encoding))
    {
    FError::Throwf(TEXT("Could not save file: %s"), *meta_file_path);
    }

  // Create raw data member file
  const FString data_file_path = class_path / TEXT("!Data.sk");
  FString data_body = TEXT("\r\n");
  // Figure out column width of variable types
  int32 max_type_length = 0;
  int32 max_name_length = 0;
  for (TFieldIterator<UProperty> property_it(class_or_struct_p, EFieldIteratorFlags::ExcludeSuper); property_it; ++property_it)
    {
    UProperty * var_p = *property_it;
    if (can_export_property(class_or_struct_p, var_p))
      {
      FString type_name = get_skookum_property_type_name(var_p);
      FString var_name = skookify_var_name(var_p->GetName(), var_p->IsA(UBoolProperty::StaticClass()), true);
      max_type_length = FMath::Max(max_type_length, type_name.Len());
      max_name_length = FMath::Max(max_name_length, var_name.Len());
      }
    }
  // Format nicely
  for (TFieldIterator<UProperty> property_it(class_or_struct_p, EFieldIteratorFlags::ExcludeSuper); property_it; ++property_it)
    {
    UProperty * var_p = *property_it;
    if (can_export_property(class_or_struct_p, var_p))
      {
      FString type_name = get_skookum_property_type_name(var_p);
      FString var_name = skookify_var_name(var_p->GetName(), var_p->IsA(UBoolProperty::StaticClass()), true);
      FString comment = var_p->GetToolTipText().ToString().Replace(TEXT("\n"), TEXT(" "));
      data_body += FString::Printf(TEXT("&raw %s !%s // %s%s[%s]\r\n"), *(type_name.RightPad(max_type_length)), *(var_name.RightPad(max_name_length)), *comment, comment.IsEmpty()? TEXT("") : TEXT(" "), *var_p->GetName());
      }
    }
  if (!FFileHelper::SaveStringToFile(data_body, *data_file_path, ms_script_file_encoding))
    {
    FError::Throwf(TEXT("Could not save file: %s"), *meta_file_path);
    }

  // For structs, generate ctor/ctor_copy/op_assign/dtor
  if (!Cast<UClass>(class_or_struct_p))
    {
    // Constructor
    const FString ctor_file_path = class_path / TEXT("!().sk");
    body = FString::Printf(TEXT("() %s\r\n"), *skookum_class_name);
    if (!FFileHelper::SaveStringToFile(body, *ctor_file_path, ms_script_file_encoding))
      {
      FError::Throwf(TEXT("Could not save file: %s"), *ctor_file_path);
      }

    // Copy constructor
    const FString ctor_copy_file_path = class_path / TEXT("!copy().sk");
    body = FString::Printf(TEXT("(%s other) %s\r\n"), *skookum_class_name, *skookum_class_name);
    if (!FFileHelper::SaveStringToFile(body, *ctor_copy_file_path, ms_script_file_encoding))
      {
      FError::Throwf(TEXT("Could not save file: %s"), *ctor_copy_file_path);
      }

    // Assignment operator
    const FString assign_file_path = class_path / TEXT("assign().sk");
    body = FString::Printf(TEXT("(%s other) %s\r\n"), *skookum_class_name, *skookum_class_name);
    if (!FFileHelper::SaveStringToFile(body, *assign_file_path, ms_script_file_encoding))
      {
      FError::Throwf(TEXT("Could not save file: %s"), *assign_file_path);
      }

    // Destructor
    const FString dtor_file_path = class_path / TEXT("!!().sk");
    body = TEXT("()\r\n");
    if (!FFileHelper::SaveStringToFile(body, *dtor_file_path, ms_script_file_encoding))
      {
      FError::Throwf(TEXT("Could not save file: %s"), *dtor_file_path);
      }
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_class_header_file(UStruct * class_or_struct_p, const FString & source_header_file_name)
  {
  const FString class_header_file_name = FString::Printf(TEXT("SkUE%s.generated.hpp"), *get_skookum_class_name(class_or_struct_p));
  m_all_header_file_names.Add(class_header_file_name);

  FString skookum_class_name = get_skookum_class_name(class_or_struct_p);
  FString cpp_class_name = get_cpp_class_name(class_or_struct_p);

  FString generated_code;

  generated_code += TEXT("#pragma once\r\n\r\n");
  generated_code += TEXT("#include <Bindings/SkUEClassBinding.hpp>\r\n");

  // if not defined let's hope it is already known when the compiler gets here
  if (source_header_file_name.Len() > 0)
    {
    FString relative_path(source_header_file_name);
    if (!FPaths::IsRelative(relative_path))
      {
      FPaths::MakePathRelativeTo(relative_path, *m_runtime_plugin_root_path);
      }
    generated_code += FString::Printf(TEXT("#include <%s>\r\n\r\n"), *relative_path);
    }
  else
    {
    generated_code += FString::Printf(TEXT("// Note: Include path for %s was unknown at code generation time, so hopefully the class is already known when compilation gets here\r\n\r\n"), *cpp_class_name);
    }

  UClass * class_p = Cast<UClass>(class_or_struct_p);
  generated_code += FString::Printf(TEXT("class SkUE%s : public SkUEClassBinding%s<SkUE%s, %s>\r\n  {\r\n"),
    *skookum_class_name,
    class_p ? (class_p->HasAnyCastFlag(CASTCLASS_AActor) ? TEXT("Actor") : TEXT("Entity"))
            : (is_pod(class_or_struct_p) ? TEXT("StructPod") : TEXT("Struct")),
    *skookum_class_name,
    *cpp_class_name);

  generated_code += TEXT("  public:\r\n");
  generated_code += TEXT("    static void register_bindings();\r\n");
  generated_code += TEXT("  };\r\n");

  save_text_file_if_changed(m_binding_code_path / class_header_file_name, generated_code);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_class_binding_file(UStruct * class_or_struct_p)
  {
  const FString skookum_class_name = get_skookum_class_name(class_or_struct_p);
  const FString class_binding_file_name = FString::Printf(TEXT("SkUE%s.generated.inl"), *skookum_class_name);
  m_all_binding_file_names.AddUnique(class_binding_file_name);

  const FString class_name_cpp = get_cpp_class_name(class_or_struct_p);

  enum eScope { Scope_instance, Scope_class }; // 0 = instance, 1 = static bindings
  TArray<MethodBinding> bindings[2]; // eScope

  FString generated_code;
  generated_code += FString::Printf(TEXT("\r\nnamespace SkUE%s_Impl\r\n  {\r\n\r\n"), *skookum_class_name);

  UClass * class_p = Cast<UClass>(class_or_struct_p);
  if (class_p)
    {
    MethodBinding binding;
      
    // Export all functions
    for (TFieldIterator<UFunction> func_it(class_or_struct_p); func_it; ++func_it)
      {
      UFunction * function_p = *func_it;
      if (can_export_method(class_p, function_p))
        {
        binding.make_method(function_p);
        if (bindings[Scope_instance].Find(binding) < 0 && bindings[Scope_class].Find(binding) < 0) // If method with this name already bound, assume it does the same thing and skip
          {
          generated_code += generate_method(class_name_cpp, class_p, function_p, binding);
          bindings[function_p->HasAnyFunctionFlags(FUNC_Static) ? Scope_class : Scope_instance].Push(binding);
          }
        }
      }
    }

  // Binding array
  for (uint32 scope = 0; scope < 2; ++scope)
    {
    if (bindings[scope].Num() > 0)
      {
      generated_code += FString::Printf(TEXT("  static const SkClass::MethodInitializerFuncId methods_%c[] =\r\n    {\r\n"), scope ? TCHAR('c') : TCHAR('i'));
      for (auto & binding : bindings[scope])
        {
        generated_code += FString::Printf(TEXT("      { 0x%08x, mthd%s_%s },\r\n"), get_skookum_symbol_id(*binding.m_script_name), scope ? TEXT("c") : TEXT(""), *binding.m_code_name);
        }
      generated_code += TEXT("    };\r\n\r\n");
      }
    }

  // Close namespace
  generated_code += FString::Printf(TEXT("  } // SkUE%s_Impl\r\n\r\n"), *skookum_class_name);

  // Register bindings function
  generated_code += FString::Printf(TEXT("void SkUE%s::register_bindings()\r\n  {\r\n"), *skookum_class_name);

  generated_code += FString::Printf(TEXT("  %s::register_bindings(0x%08x); // \"%s\"\r\n\r\n"), 
    class_p ? TEXT("tBindingEntity") : TEXT("tBindingStruct"),
    get_skookum_symbol_id(*skookum_class_name), *skookum_class_name);

  for (uint32 scope = 0; scope < 2; ++scope)
    {
    if (bindings[scope].Num() > 0)
      {
      generated_code += FString::Printf(TEXT("  ms_class_p->register_method_func_bulk(SkUE%s_Impl::methods_%c, %d, %s);\r\n"), *skookum_class_name, scope ? TCHAR('c') : TCHAR('i'), bindings[scope].Num(), scope ? TEXT("SkBindFlag_class_no_rebind") : TEXT("SkBindFlag_instance_no_rebind"));
      }
    }
  generated_code += TEXT("  }\r\n");

  save_text_file_if_changed(m_binding_code_path / class_binding_file_name, generated_code);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_struct_from_property(UProperty * prop_p)
  {
  UStruct * struct_p = Cast<UStructProperty>(prop_p)->Struct;
  if (!struct_p)
    return;

  generate_struct(struct_p);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_struct(UStruct * struct_p)
  {
  if (can_export_struct(struct_p))
    {
    UE_LOG(LogSkookumScriptGenerator, Log, TEXT("Generating struct %s"), *struct_p->GetName());

    m_exported_classes.Add(struct_p);

    // Generate script files
    generate_class_script_files(struct_p);

    // Generate binding code files    
    generate_class_header_file(struct_p, struct_p->GetMetaData(ms_meta_data_key_module_relative_path));
    generate_class_binding_file(struct_p);
    }

  // Also generate all parents
  UStruct * parent_struct_p = struct_p->GetSuperStruct();
  if (parent_struct_p)
    {
    generate_struct(parent_struct_p);
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_enum_from_property(UProperty * prop_p)
  {
  UEnum * enum_p = get_enum(prop_p);
  if (enum_p)
    {
    generate_enum(enum_p);
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_enum(UEnum * enum_p)
  {
  if (can_export_enum(enum_p))
    {
    m_exported_enums.Add(enum_p);
    generate_enum_script_files(enum_p);
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_enum_script_files(UEnum * enum_p)
  {
  FString enum_type_name = enum_p->GetName();
  FString enum_path = m_overlay_path / TEXT("Object/Enum") / enum_type_name;

  // Class meta information
  FString meta_file_path = enum_path / TEXT("!Class.sk-meta");
  FString meta_body = get_comment_block(enum_p);
  if (!FFileHelper::SaveStringToFile(meta_body, *meta_file_path, ms_script_file_encoding))
    {
    FError::Throwf(TEXT("Could not save file: %s"), *meta_file_path);
    }

  // Class data members and class constructor
  FString data_file_path = enum_path / TEXT("!DataC.sk");
  FString data_body;

  FString constructor_file_path = enum_path / TEXT("!()C.sk");
  FString enum_script_path = enum_p->GetPathName();
  FString constructor_body;

  for (int32 enum_index = 0; enum_index < enum_p->NumEnums() - 1; ++enum_index)
    {
    FString enum_val_name = enum_p->GetEnumName(enum_index);
    FString enum_val_full_name = enum_p->GenerateFullEnumName(*enum_val_name);

    FString skookified_val_name = skookify_var_name(enum_val_name, false, true);
    if (skookified_val_name.Equals(TEXT("world"))
     || skookified_val_name.Equals(TEXT("random")))
      {
      skookified_val_name += TEXT("_");
      }

    FName token = FName(*enum_val_full_name, FNAME_Find);
    if (token != NAME_None)
      {
      int32 enum_value = UEnum::LookupEnumName(token);
      if (enum_value != INDEX_NONE)
        {
        data_body += FString::Printf(TEXT("%s !@%s\r\n"), *enum_type_name, *skookified_val_name);
        constructor_body += FString::Printf(TEXT("  @%s: %s!int(%d)\r\n"), *skookified_val_name, *enum_type_name, enum_value);
        }
      }
    }

  if (!FFileHelper::SaveStringToFile(data_body, *data_file_path, ms_script_file_encoding))
    {
    FError::Throwf(TEXT("Could not save file: %s"), *data_file_path);
    }

  FString file_body = FString::Printf(TEXT("// %s\r\n// EnumPath: %s\r\n\r\n()\r\n\r\n  [\r\n%s  ]\r\n"), *enum_type_name, *enum_script_path, *constructor_body);
  if (!FFileHelper::SaveStringToFile(file_body, *constructor_file_path, ms_script_file_encoding))
    {
    FError::Throwf(TEXT("Could not save file: %s"), *constructor_file_path);
    }

  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_enum_binding_files()
  {
  FString generated_code;

  // Generate header file
  generated_code = TEXT("#pragma once\r\n\r\n");
  TSet<FString> include_files;
  include_files.Reserve(m_exported_enums.Num());
  for (auto enum_p : m_exported_enums)
    {
    include_files.Add(enum_p->GetMetaData(ms_meta_data_key_module_relative_path));
    }
  for (auto include_file : include_files)
    {
    generated_code += FString::Printf(TEXT("#include \"%s\"\r\n"), *include_file);
    }
  generated_code += TEXT("\r\n");
  for (auto enum_p : m_exported_enums)
    {
    generated_code += FString::Printf(TEXT("class SkUE%s : public SkEnum\n  {\r\n  public:\r\n    static SkClass *     ms_class_p;\r\n    static UEnum *       ms_uenum_p;\r\n"), *enum_p->GetName());
    generated_code += FString::Printf(TEXT("    static SkInstance *  new_instance(%s value) { return SkEnum::new_instance((SkEnumType)value, ms_class_p); }\r\n"), *enum_p->CppType);
    generated_code += TEXT("  };\r\n\r\n");
    }
  FString enum_header_file_name = m_binding_code_path / TEXT("SkUEEnums.generated.hpp");
  save_text_file_if_changed(enum_header_file_name, generated_code);
  m_all_header_file_names.Add(enum_header_file_name);

  // Generate implementation file
  generated_code = TEXT("\r\n");
  for (auto enum_p : m_exported_enums)
    {
    generated_code += FString::Printf(TEXT("SkClass * SkUE%s::ms_class_p;\r\nUEnum *   SkUE%s::ms_uenum_p;\r\n"), *enum_p->GetName(), *enum_p->GetName());
    }
  generated_code += TEXT("\r\nnamespace SkUE\r\n  {\r\n\r\n");
  generated_code += TEXT("  void register_enum_bindings()\r\n    {\r\n");
  for (auto enum_p : m_exported_enums)
    {
    generated_code += FString::Printf(TEXT("    SkUE%s::ms_class_p = SkBrain::get_class(ASymbol::create_existing(0x%08x));\r\n"), *enum_p->GetName(), get_skookum_symbol_id(enum_p->GetName()));
    }
  generated_code += TEXT("\r\n");
  for (auto enum_p : m_exported_enums)
    {
    generated_code += FString::Printf(TEXT("    SkUEClassBindingHelper::add_static_enum_mapping(SkUE%s::ms_class_p, SkUE%s::ms_uenum_p);\r\n"), *enum_p->GetName(), *enum_p->GetName());
    }
  generated_code += TEXT("    }\r\n");
  generated_code += TEXT("\r\n  } // SkUE\r\n");
  FString enum_binding_file_name = m_binding_code_path / TEXT("SkUEEnums.generated.inl");
  save_text_file_if_changed(enum_binding_file_name, generated_code);
  m_all_binding_file_names.Add(enum_binding_file_name);
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method(const FString & class_name_cpp, UClass * class_p, UFunction * function_p, const MethodBinding & binding)
  {
  // Generate script file
  generate_method_script_file(function_p, binding.m_script_name);

  // Generate binding code
  return generate_method_binding_code(class_name_cpp, class_p, function_p, binding);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_method_script_file(UFunction * function_p, const FString & script_function_name)
  {
  // Generate function content
  FString function_body = get_comment_block(function_p);
  bool has_params_or_return_value = (function_p->Children != NULL);
  if (has_params_or_return_value)
    {
    function_body += TEXT("(");

    // Figure out column width of variable types & names
    int32 max_type_length = 0;
    int32 max_name_length = 0;
    int32 param_count = 0;
    for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it)
      {
      UProperty * param_p = *param_it;
      if (!(param_p->GetPropertyFlags() & CPF_ReturnParm))
        {
        FString type_name = get_skookum_property_type_name(param_p);
        FString var_name = skookify_var_name(param_p->GetName(), param_p->IsA(UBoolProperty::StaticClass()));
        max_type_length = FMath::Max(max_type_length, type_name.Len());
        max_name_length = FMath::Max(max_name_length, var_name.Len());
        ++param_count;
        }
      }

    // Format nicely
    FString separator;
    FString return_type_name;
    for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it)
      {
      UProperty * param_p = *param_it;
      if (param_p->GetPropertyFlags() & CPF_ReturnParm)
        {
        return_type_name = get_skookum_property_type_name(param_p);
        }
      else
        {
        FString type_name = get_skookum_property_type_name(param_p);
        FString var_name = skookify_var_name(param_p->GetName(), param_p->IsA(UBoolProperty::StaticClass()));
        FString default_initializer = get_skookum_default_initializer(function_p, param_p);
        function_body += separator + type_name.RightPad(max_type_length) + TEXT(" ");
        if (default_initializer.IsEmpty())
          {
          function_body += var_name;
          }
        else
          {
          function_body += var_name.RightPad(max_name_length) + get_skookum_default_initializer(function_p, param_p);
          }
        }
      separator = TEXT(",\n ");
      }

    // Place return type on new line if present and more than one parameter
    if (param_count > 1 && !return_type_name.IsEmpty())
      {
      function_body += TEXT("\n");
      }
    function_body += TEXT(") ") + return_type_name + TEXT("\n");
    }
  else
    {
    function_body = TEXT("()\n");
    }

  // Create script file
  FString function_file_path = get_skookum_method_path(function_p->GetOwnerClass(), script_function_name, function_p->HasAnyFunctionFlags(FUNC_Static));
  if (!FFileHelper::SaveStringToFile(function_body, *function_file_path, ms_script_file_encoding))
    {
    FError::Throwf(TEXT("Could not save file: %s"), *function_file_path);
    }
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_binding_code(const FString & class_name_cpp, UClass * class_p, UFunction * function_p, const MethodBinding & binding)
  {
  // Generate code for the function body
  FString function_body;
  if (function_p->HasAnyFunctionFlags(FUNC_Public) 
   && !function_p->HasMetaData(ms_meta_data_key_custom_structure_param)  // Never call custom thunks directly
   && !function_p->HasMetaData(ms_meta_data_key_array_parm))             // Never call custom thunks directly
    {
    // Public function, might be called via direct call
    if (function_p->HasAnyFunctionFlags(FUNC_RequiredAPI) || class_p->HasAnyClassFlags(CLASS_RequiredAPI))
      {
      // This function is always safe to call directly
      function_body = generate_method_binding_code_body_via_call(class_name_cpp, class_p, function_p, binding);
      }
    else
      {
      // This function is called directly in monolithic builds, via event otherwise
      function_body += TEXT("  #if IS_MONOLITHIC\r\n");
      function_body += generate_method_binding_code_body_via_call(class_name_cpp, class_p, function_p, binding);
      function_body += TEXT("  #else\r\n");
      function_body += generate_method_binding_code_body_via_event(class_name_cpp, class_p, function_p, binding);
      function_body += TEXT("  #endif\r\n");
      }
    }
  else
    {
    // This function is protected and always called via event
    function_body = generate_method_binding_code_body_via_event(class_name_cpp, class_p, function_p, binding);
    }

  // Assemble function definition
  FString generated_code = FString::Printf(TEXT("  %s\r\n    {\r\n"), *generate_method_binding_declaration(*binding.m_code_name, function_p->HasAnyFunctionFlags(FUNC_Static)));
  generated_code += function_body;
  generated_code += TEXT("    }\r\n\r\n");

  return generated_code;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_binding_code_body_via_call(const FString & class_name_cpp, UClass * class_p, UFunction * function_p, const MethodBinding & binding)
  {
  bool is_static = function_p->HasAnyFunctionFlags(FUNC_Static);

  FString function_body;

  // Need this pointer only for instance methods
  if (!is_static)
    {
    function_body += FString::Printf(TEXT("    %s\r\n"), *generate_this_pointer_initialization(class_name_cpp, class_p, is_static));
    }

  FString out_params;
  UProperty * return_value_p = nullptr;
  const bool has_params_or_return_value = (function_p->Children != NULL);
  if (has_params_or_return_value)
    {
    for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it)
      {
      UProperty * param_p = *param_it;
      function_body += FString::Printf(TEXT("    %s %s;\r\n"), *get_cpp_property_type_name(param_p), *param_p->GetName());
      }
    int32 param_index = 0;
    for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it, ++param_index)
      {
      UProperty * param_p = *param_it;

      // Static methods always succeed returning the return value
      if (!is_static || !(param_p->GetPropertyFlags() & CPF_ReturnParm))
        {
        function_body += FString::Printf(TEXT("    %s\r\n"), *generate_method_parameter_assignment(param_p, param_index, param_p->GetName()));
        }

      if (param_p->GetPropertyFlags() & CPF_ReturnParm)
        {
        return_value_p = param_p;
        }
      else if (param_p->GetPropertyFlags() & CPF_OutParm)
        {
        out_params += FString::Printf(TEXT("    %s;\r\n"), *generate_method_out_parameter_expression(function_p, param_p, param_index, param_p->GetName()));
        }
      }
    }

  // Only check this pointer if instance method
  FString function_invocation;
  FString indent;
  if (is_static)
    {
    function_invocation = class_name_cpp + TEXT("::");
    }
  else
    {
    // Only call if this pointer is valid
    function_body += FString::Printf(TEXT("    SK_ASSERTX(this_p, \"Tried to invoke method %s@%s but the %s is null.\");\r\n"), *get_skookum_class_name(class_p), *binding.m_script_name, *get_skookum_class_name(class_p));
    function_body += TEXT("    if (this_p)\r\n      {\r\n");
    indent = TEXT("  ");
    function_invocation = TEXT("this_p->");
    }
  function_invocation += function_p->GetName();

  // Call function directly
  if (return_value_p)
    {
    function_body += indent + FString::Printf(TEXT("    %s = %s("), *return_value_p->GetName(), *function_invocation);
    }
  else
    {
    function_body += indent + FString::Printf(TEXT("    %s("), *function_invocation);
    }
  bool is_first = true;
  for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it)
    {
    UProperty * param_p = *param_it;
    if (!(param_p->GetPropertyFlags() & CPF_ReturnParm))
      {
      if (!is_first)
        {
        function_body += TEXT(", ");
        }
      function_body += param_p->GetName();
      is_first = false;
      }
    }
  function_body += TEXT(");\r\n");

  if (!is_static)
    {
    function_body += TEXT("      }\r\n");
    }

  function_body += out_params;

  if (return_value_p)
    {
    function_body += FString::Printf(TEXT("    %s\r\n"), *generate_return_value_passing(return_value_p, *return_value_p->GetName()));
    }

  return function_body;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_binding_code_body_via_event(const FString & class_name_cpp, UClass * class_p, UFunction * function_p, const MethodBinding & binding)
  {
  bool is_static = function_p->HasAnyFunctionFlags(FUNC_Static);

  FString function_body;
  function_body += FString::Printf(TEXT("    %s\r\n"), *generate_this_pointer_initialization(class_name_cpp, class_p, is_static));

  FString params;
  FString out_params;
  UProperty * return_value_p = nullptr;

  const bool has_params_or_return_value = (function_p->Children != NULL);
  if (has_params_or_return_value)
    {
    params += TEXT("    struct FDispatchParams\r\n      {\r\n");

    for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it)
      {
      UProperty * param_p = *param_it;
      params += FString::Printf(TEXT("      %s %s;\r\n"), *get_cpp_property_type_name(param_p), *param_p->GetName());
      }
    params += TEXT("      } params;\r\n");
    int32 param_index = 0;
    for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it, ++param_index)
      {
      UProperty * param_p = *param_it;

      // Static methods always succeed returning the return value
      if (!is_static || !(param_p->GetPropertyFlags() & CPF_ReturnParm))
        {
        params += FString::Printf(TEXT("    %s\r\n"), *generate_method_parameter_assignment(param_p, param_index, FString::Printf(TEXT("params.%s"), *param_p->GetName())));
        }

      if (param_p->GetPropertyFlags() & CPF_ReturnParm)
        {
        return_value_p = param_p;
        }
      else if (param_p->GetPropertyFlags() & CPF_OutParm)
        {
        FString param_in_struct = FString::Printf(TEXT("params.%s"), *param_p->GetName());
        out_params += FString::Printf(TEXT("    %s;\r\n"), *generate_method_out_parameter_expression(function_p, param_p, param_index, param_in_struct));
        }
      }
    }

  // Only check this pointer if not static
  FString indent;
  if (!is_static)
    {
    params += FString::Printf(TEXT("    SK_ASSERTX(this_p, \"Tried to invoke method %s@%s but the %s is null.\");\r\n"), *get_skookum_class_name(class_p), *binding.m_script_name, *get_skookum_class_name(class_p));
    params += TEXT("    if (this_p)\r\n      {\r\n");
    indent = TEXT("  ");
    }
  params += indent + FString::Printf(TEXT("    static UFunction * function_p = this_p->FindFunctionChecked(TEXT(\"%s\"));\r\n"), *function_p->GetName());

  if (has_params_or_return_value)
    {
    params += indent + TEXT("    check(function_p->ParmsSize <= sizeof(FDispatchParams));\r\n");
    params += indent + TEXT("    this_p->ProcessEvent(function_p, &params);\r\n");
    }
  else
    {
    params += indent + TEXT("    this_p->ProcessEvent(function_p, nullptr);\r\n");
    }

  if (!is_static)
    {
    params += TEXT("      }\r\n");
    }

  function_body += params;
  function_body += out_params;

  if (return_value_p)
    {
    FString return_value_name = FString::Printf(TEXT("params.%s"), *return_value_p->GetName());
    function_body += FString::Printf(TEXT("    %s\r\n"), *generate_return_value_passing(return_value_p, *return_value_name));
    }

  return function_body;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_binding_declaration(const FString & function_name, bool is_static)
  {
  return FString::Printf(TEXT("static void mthd%s_%s(SkInvokedMethod * scope_p, SkInstance ** result_pp)"), is_static ? TEXT("c") : TEXT(""), *function_name);
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_this_pointer_initialization(const FString & class_name_cpp, UStruct * class_or_struct_p, bool is_static)
  {
  FString class_name_skookum = get_skookum_class_name(class_or_struct_p);
  if (is_static)
    {
    return FString::Printf(TEXT("%s * this_p = GetMutableDefault<%s>(SkUE%s::ms_uclass_p);"), *class_name_cpp, *class_name_cpp, *class_name_skookum);
    }
  else
    {
    bool is_class = !!Cast<UClass>(class_or_struct_p);
    return FString::Printf(TEXT("%s * this_p = %sscope_p->this_as<SkUE%s>();"), *class_name_cpp, is_class ? TEXT("") : TEXT("&"), *class_name_skookum);
    }
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_out_parameter_expression(UFunction * function_p, UProperty * param_p, int32 param_index, const FString & param_name)
  {
  FString fmt;

  eSkTypeID type_id = get_skookum_property_type(param_p);
  switch (type_id)
    {
    case SkTypeID_Integer:
    case SkTypeID_Real:
    case SkTypeID_Boolean:
    case SkTypeID_Vector2:
    case SkTypeID_Vector3:
    case SkTypeID_Vector4:
    case SkTypeID_Rotation:
    case SkTypeID_RotationAngles:
    case SkTypeID_Transform:
    case SkTypeID_Color:
    case SkTypeID_Name:
    case SkTypeID_Enum:
    case SkTypeID_UStruct:
    case SkTypeID_UClass:
    case SkTypeID_UObject:         fmt = FString::Printf(TEXT("scope_p->get_arg<%s>(SkArg_%%d) = %%s"), *get_skookum_property_binding_class_name(param_p)); break;
    case SkTypeID_String:          fmt = TEXT("scope_p->get_arg<SkString>(SkArg_%d) = AString(*%s, %s.Len())"); break; // $revisit MBreyer - Avoid copy here
    case SkTypeID_List:
      {
      const UArrayProperty * array_property_p = Cast<UArrayProperty>(param_p);
      UProperty * element_property_p = array_property_p->Inner;
      fmt = FString::Printf(TEXT("SkUEClassBindingHelper::initialize_list_from_array<%s,%s>(&scope_p->get_arg<SkList>(SkArg_%%d), %%s)"),
        *get_skookum_property_binding_class_name(element_property_p),
        *get_cpp_property_type_name(element_property_p));
      }
      break;
    default:  FError::Throwf(TEXT("Unsupported return param type: %s"), *param_p->GetClass()->GetName()); break;
    }

  return FString::Printf(*fmt, param_index + 1, *param_name, *param_name);
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_parameter_assignment(UProperty * param_p, int32 param_index, FString assignee_name)
  {
  // We assume a parameter goes out only if it is either the return value (of course)
  // or if it is marked CPF_OutParm _and_ its name begins with "Out"
  bool is_out_only = (param_p->GetPropertyFlags() & CPF_ReturnParm)
    || ((param_p->GetPropertyFlags() & CPF_OutParm) && param_p->GetName().Find(TEXT("Out")) == 0);
  // If it's not a purely outgoing parameter, fetch it from the caller
  if (!is_out_only)
    {
    FString generated_code;

    eSkTypeID type_id = get_skookum_property_type(param_p);
    switch (type_id)
      {
      case SkTypeID_Real:
      case SkTypeID_Boolean:
      case SkTypeID_Vector2:
      case SkTypeID_Vector3:
      case SkTypeID_Vector4:
      case SkTypeID_Rotation:
      case SkTypeID_RotationAngles:
      case SkTypeID_Transform:
      case SkTypeID_Name:
      case SkTypeID_UStruct:
      case SkTypeID_UClass:
      case SkTypeID_UObject:       generated_code = FString::Printf(TEXT("%s = scope_p->get_arg<%s>(SkArg_%d);"), *assignee_name, *get_skookum_property_binding_class_name(param_p), param_index + 1); break;
      case SkTypeID_Integer:       generated_code = FString::Printf(TEXT("%s = (%s)scope_p->get_arg<%s>(SkArg_%d);"), *assignee_name, *get_cpp_property_cast_name(param_p), *get_skookum_property_binding_class_name(param_p), param_index + 1); break;
      case SkTypeID_Enum:          generated_code = FString::Printf(TEXT("%s = (%s)scope_p->get_arg<SkEnum>(SkArg_%d);"), *assignee_name, *get_cpp_property_cast_name(param_p), param_index + 1); break;
      case SkTypeID_String:        generated_code = FString::Printf(TEXT("%s = scope_p->get_arg<SkString>(SkArg_%d).as_cstr();"), *assignee_name, param_index + 1); break;
      case SkTypeID_Color:
        {
        static FName name_Color("Color");
        bool is_color8 = CastChecked<UStructProperty>(param_p)->Struct->GetFName() == name_Color;
        generated_code = FString::Printf(TEXT("%s = scope_p->get_arg<SkColor>(SkArg_%d)%s;"), *assignee_name, param_index + 1, is_color8 ? TEXT(".ToFColor(true)") : TEXT(""));
        }
        break;
      case SkTypeID_List:
        {
        const UArrayProperty * array_property_p = Cast<UArrayProperty>(param_p);
        UProperty * element_property_p = array_property_p->Inner;
        generated_code = FString::Printf(TEXT("SkUEClassBindingHelper::initialize_array_from_list<%s,%s,%s>(&%s, scope_p->get_arg<SkList>(SkArg_%d));"),
          *get_skookum_property_binding_class_name(element_property_p),
          *get_cpp_property_type_name(element_property_p),
          *get_cpp_property_cast_name(element_property_p),
          *assignee_name,
          param_index + 1);
        }
        break;
      default: FError::Throwf(TEXT("Unsupported function param type: %s"), *param_p->GetClass()->GetName()); break;
      }
    return generated_code;
    }
  else
    {
    FString default_ctor_argument = generate_property_default_ctor_argument(param_p);
    if (default_ctor_argument.IsEmpty()) return FString();
    if (default_ctor_argument == TEXT("ForceInitToZero")) return FString::Printf(TEXT("%s = %s(ForceInitToZero);"), *assignee_name, *get_cpp_property_type_name(param_p));
    return FString::Printf(TEXT("%s = %s;"), *assignee_name, *default_ctor_argument);
    }
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_property_default_ctor_argument(UProperty * param_p)
  {
  eSkTypeID type_id = get_skookum_property_type(param_p);
  switch (type_id)
    {
    case SkTypeID_Integer:         return TEXT("0");
    case SkTypeID_Real:            return TEXT("0.0f");
    case SkTypeID_Boolean:         return TEXT("false");
    case SkTypeID_Enum:            return FString::Printf(TEXT("(%s)0"), *get_cpp_property_type_name(param_p));
    case SkTypeID_List:
    case SkTypeID_String:
    case SkTypeID_Name:
    case SkTypeID_Transform:
    case SkTypeID_UStruct:         return TEXT("");
    case SkTypeID_Vector2:
    case SkTypeID_Vector3:
    case SkTypeID_Vector4:
    case SkTypeID_Rotation:
    case SkTypeID_RotationAngles:
    case SkTypeID_Color:           return TEXT("ForceInitToZero");
    case SkTypeID_UClass:
    case SkTypeID_UObject:         return TEXT("nullptr");
    default:                       FError::Throwf(TEXT("Unsupported property type: %s"), *param_p->GetClass()->GetName()); return TEXT("");
    }
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_return_value_passing(UProperty * return_value_p, const FString & return_value_name)
  {
  if (return_value_p)
    {
    FString fmt;

    eSkTypeID type_id = get_skookum_property_type(return_value_p);
    switch (type_id)
      {
      case SkTypeID_Integer:
      case SkTypeID_Real:
      case SkTypeID_Boolean:
      case SkTypeID_Vector2:
      case SkTypeID_Vector3:
      case SkTypeID_Vector4:
      case SkTypeID_Rotation:
      case SkTypeID_RotationAngles:
      case SkTypeID_Transform:
      case SkTypeID_Color:
      case SkTypeID_Name:
      case SkTypeID_Enum:
      case SkTypeID_UStruct:
      case SkTypeID_UClass:
      case SkTypeID_UObject:         fmt = FString::Printf(TEXT("%s::new_instance(%%s)"), *get_skookum_property_binding_class_name(return_value_p)); break;
      case SkTypeID_String:          fmt = TEXT("SkString::new_instance(AString(*(%s), %s.Len()))"); break; // $revisit MBreyer - Avoid copy here
      case SkTypeID_List:
        {
        const UArrayProperty * array_property_p = Cast<UArrayProperty>(return_value_p);
        UProperty * element_property_p = array_property_p->Inner;
        fmt = FString::Printf(TEXT("SkUEClassBindingHelper::list_from_array<%s,%s>(%%s)"),
          *get_skookum_property_binding_class_name(element_property_p),
          *get_cpp_property_type_name(element_property_p));
        }
        break;
      default:  FError::Throwf(TEXT("Unsupported return param type: %s"), *return_value_p->GetClass()->GetName()); break;
      }

    FString initializer = FString::Printf(*fmt, *return_value_name, *return_value_name);
    return FString::Printf(TEXT("if (result_pp) *result_pp = %s;"), *initializer);
    }
  else
    {
    return TEXT(""); // TEXT("if (result_pp) *result_pp = SkBrain::ms_nil_p;");
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::generate_master_binding_file()
  {
  FString generated_code;

  generated_code += TEXT("\r\n");

  generated_code += TEXT("#include \"SkookumScript/SkClass.hpp\"\r\n");
  generated_code += TEXT("#include \"SkookumScript/SkBrain.hpp\"\r\n");
  generated_code += TEXT("#include \"SkookumScript/SkInvokedMethod.hpp\"\r\n");
  generated_code += TEXT("#include \"SkookumScript/SkInteger.hpp\"\r\n");
  generated_code += TEXT("#include \"SkookumScript/SkEnum.hpp\"\r\n");
  generated_code += TEXT("#include \"SkookumScript/SkReal.hpp\"\r\n");
  generated_code += TEXT("#include \"SkookumScript/SkBoolean.hpp\"\r\n");
  generated_code += TEXT("#include \"SkookumScript/SkString.hpp\"\r\n");

  generated_code += TEXT("\r\n");

  // Include all headers
  for (auto & header_file_name : m_all_header_file_names)
    {
    // Re-base to make sure we're including the right files on a remote machine
    FString NewFilename(FPaths::GetCleanFilename(header_file_name));
    generated_code += FString::Printf(TEXT("#include \"%s\"\r\n"), *NewFilename);
    }

  generated_code += TEXT("\r\n");

  // Include all bindings
  for (auto & binding_file_name : m_all_binding_file_names)
    {
    // Re-base to make sure we're including the right files on a remote machine
    FString NewFilename(FPaths::GetCleanFilename(binding_file_name));
    generated_code += FString::Printf(TEXT("#include \"%s\"\r\n"), *NewFilename);
    }

  generated_code += TEXT("\r\nnamespace SkUE\r\n  {\r\n\r\n");

  //--- register_static_types() ---

  generated_code += TEXT("  void register_static_types()\r\n    {\r\n");
  
  // Store UE class pointers
  uint32_t num_exported_classes = 0;
  for (auto class_or_struct_p : m_exported_classes)
    {
    UClass * class_p = Cast<UClass>(class_or_struct_p);
    if (class_p)
      {
      ++num_exported_classes;
      /* This does not work for a renegade class or two so disable for now
      if (does_class_have_static_class(class_p))
        {
        generated_code += FString::Printf(TEXT("    SkUE%s::ms_uclass_p = %s::StaticClass();\r\n"), *get_skookum_class_name(class_p), *get_cpp_class_name(class_p));
        }
      else
      */
        {
        generated_code += FString::Printf(TEXT("    SkUE%s::ms_uclass_p = FindObject<UClass>(ANY_PACKAGE, TEXT(\"%s\"));\r\n"), *get_skookum_class_name(class_p), *class_p->GetName());
        }
      }
    }
  generated_code += TEXT("\r\n");

  // Store UE struct pointers
  for (auto class_or_struct_p : m_exported_classes)
    {
    if (!Cast<UClass>(class_or_struct_p))
      {
      generated_code += FString::Printf(TEXT("    SkUE%s::ms_ustruct_p = FindObject<UStruct>(ANY_PACKAGE, TEXT(\"%s\"));\r\n"), *get_skookum_class_name(class_or_struct_p), *class_or_struct_p->GetName());
      }
    }
  generated_code += TEXT("\r\n");

  // Store UE enum pointers
  for (auto enum_p : m_exported_enums)
    {
    generated_code += FString::Printf(TEXT("    SkUE%s::ms_uenum_p = FindObject<UEnum>(ANY_PACKAGE, TEXT(\"%s\"));\r\n"), *enum_p->GetName(), *enum_p->GetName());
    }

  // Register static classes
  generated_code += FString::Printf(TEXT("\r\n    SkUEClassBindingHelper::reset_static_class_mappings(%d);\r\n"), num_exported_classes);
  for (auto class_or_struct_p : m_exported_classes)
    {
    if (Cast<UClass>(class_or_struct_p))
      {
      generated_code += FString::Printf(TEXT("    SkUEClassBindingHelper::register_static_class(SkUE%s::ms_uclass_p);\r\n"), *get_skookum_class_name(class_or_struct_p));
      }
    }

  // Register static structs
  generated_code += FString::Printf(TEXT("\r\n    SkUEClassBindingHelper::reset_static_struct_mappings(%d);\r\n"), m_exported_classes.Num() - num_exported_classes);
  for (auto class_or_struct_p : m_exported_classes)
    {
    if (!Cast<UClass>(class_or_struct_p))
      {
      generated_code += FString::Printf(TEXT("    SkUEClassBindingHelper::register_static_struct(SkUE%s::ms_ustruct_p);\r\n"), *get_skookum_class_name(class_or_struct_p));
      }
    }

  // Register static enums
  generated_code += FString::Printf(TEXT("\r\n    SkUEClassBindingHelper::reset_static_enum_mappings(%d);\r\n"), m_exported_enums.Num());
  for (auto enum_p : m_exported_enums)
    {
    generated_code += FString::Printf(TEXT("    SkUEClassBindingHelper::register_static_enum(SkUE%s::ms_uenum_p);\r\n"), *enum_p->GetName());
    }

  generated_code += TEXT("    }\r\n\r\n");

  //--- register_bindings() ---

  generated_code += TEXT("  void register_bindings()\r\n    {\r\n");

  // Bind all classes & structs
  for (auto class_or_struct_p : m_exported_classes)
    {
    generated_code += FString::Printf(TEXT("    SkUE%s::register_bindings();\r\n"), *get_skookum_class_name(class_or_struct_p));
    }

  // Set up enum classes
  generated_code += TEXT("\r\n    register_enum_bindings();\r\n\r\n");

  // Clear out old sk class pointers if any
  generated_code += TEXT("\r\n    SkUEClassBindingHelper::forget_sk_classes_in_all_mappings();\r\n\r\n");  

  // Generate static class mappings
  for (auto class_p : m_exported_classes)
    {
    if (Cast<UClass>(class_p))
      {
      generated_code += FString::Printf(TEXT("    SkUEClassBindingHelper::add_static_class_mapping(SkUE%s::ms_class_p, SkUE%s::ms_uclass_p);\r\n"), *get_skookum_class_name(class_p), *get_skookum_class_name(class_p));
      }
    }

  // Generate static struct mappings
  for (auto class_p : m_exported_classes)
    {
    if (!Cast<UClass>(class_p))
      {
      generated_code += FString::Printf(TEXT("    SkUEClassBindingHelper::add_static_struct_mapping(SkUE%s::ms_class_p, SkUE%s::ms_ustruct_p);\r\n"), *get_skookum_class_name(class_p), *get_skookum_class_name(class_p));
      }
    }
  generated_code += TEXT("\r\n    }\r\n");

  generated_code += TEXT("\r\n  } // SkUE\r\n");

  FString master_binding_file_name = m_binding_code_path / TEXT("SkUE.generated.inl");
  save_text_file_if_changed(master_binding_file_name, generated_code);
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::can_export_class(UClass * class_p, const FString & source_header_file_name) const
  {
  FString class_name = *class_p->GetName();

  return does_class_have_static_class(class_p) // Don't export classes that don't export DLL symbols
    && !m_exported_classes.Contains(class_p) // Don't export classes that have already been exported
    && !m_skip_classes.Contains(class_name); // Don't export classes that set to skip in UHT config file
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::can_export_struct(UStruct * struct_p)
  {
  if (m_exported_classes.Contains(struct_p)
   || m_skip_classes.Contains(struct_p->GetName())
   || get_skookum_struct_type(struct_p) != SkTypeID_UStruct) // do not export the special types Vector2, Vector3, Color etc.
    {
    return false;
    }

  return is_struct_type_supported(struct_p);
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::can_export_enum(UEnum * enum_p)
  {
  return !m_exported_enums.Contains(enum_p);
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::can_export_method(UClass * class_p, UFunction * function_p)
  {
  // If this function is inherited, do nothing as SkookumScript will inherit it for us
  if (function_p->GetOwnerClass() != class_p)
    {
    return false;
    }

  // We don't support delegates and non-public functions
  if ((function_p->FunctionFlags & FUNC_Delegate))
    {
    return false;
    }

  // HACK - custom thunk
  if (function_p->GetName() == TEXT("StackTrace"))
    {
    return false;
    }

  // Reject if any of the parameter types is unsupported yet
  for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it)
    {
    UProperty * param_p = *param_it;

    if (param_p->IsA(UDelegateProperty::StaticClass()) ||
        param_p->IsA(UMulticastDelegateProperty::StaticClass()) ||
        param_p->IsA(UWeakObjectProperty::StaticClass()) ||
        param_p->IsA(UInterfaceProperty::StaticClass()))
      {
      return false;
      }

    if (!is_property_type_supported(param_p))
      {
      return false;
      }

    // Property is supported - use the opportunity to make it known to SkookumScript
    eSkTypeID type_id = get_skookum_property_type(param_p);
    if (type_id == SkTypeID_Enum)
      {
      generate_enum_from_property(param_p);
      }
    else if (type_id == SkTypeID_UStruct)
      {
      generate_struct_from_property(param_p);
      }
    }

  return true;
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::can_export_property(UStruct * class_or_struct_p, UProperty * property_p)
  {
  // If this property is inherited, do nothing as SkookumScript will inherit it for us
  UClass * class_p = Cast<UClass>(class_or_struct_p);
  if (class_p && property_p->GetOwnerClass() != class_p)
    return false;
  if (property_p->GetOwnerStruct() != class_or_struct_p)
    return false;

  // Check if property type is supported
  if (!is_property_type_supported(property_p))
    return false;

  // Property is supported - use the opportunity to make it known to SkookumScript
  eSkTypeID type_id = get_skookum_property_type(property_p);
  if (type_id == SkTypeID_Enum)
    {
    generate_enum_from_property(property_p);
    }
  else if (type_id == SkTypeID_UStruct)
    {
    generate_struct_from_property(property_p);
    }
  else if (type_id == SkTypeID_List)
    {
    UProperty * element_property_p = Cast<UArrayProperty>(property_p)->Inner;
    eSkTypeID inner_type_id = get_skookum_property_type(element_property_p);
    if (inner_type_id == SkTypeID_Enum)
      {
      generate_enum_from_property(element_property_p);
      }
    else if (inner_type_id == SkTypeID_UStruct)
      {
      generate_struct_from_property(element_property_p);
      }
    }
  return true;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::get_skookum_property_type_name(UProperty * property_p)
  {
  eSkTypeID type_id = get_skookum_property_type(property_p);

  if (type_id == SkTypeID_UObject)
    {
    UObjectPropertyBase * object_property_p = Cast<UObjectPropertyBase>(property_p);
    m_used_classes.AddUnique(object_property_p->PropertyClass);
    return skookify_class_name(object_property_p->PropertyClass->GetName());
    }
  else if (type_id == SkTypeID_UStruct)
    {
    UStruct * struct_p = Cast<UStructProperty>(property_p)->Struct;
    generate_struct(struct_p);
    return skookify_class_name(struct_p->GetName());
    }
  else if (type_id == SkTypeID_Enum)
    {
    return get_enum(property_p)->GetName();
    }
  else if (type_id == SkTypeID_List)
    {
    return FString::Printf(TEXT("List{%s}"), *get_skookum_property_type_name(Cast<UArrayProperty>(property_p)->Inner));
    }

  return ms_sk_type_id_names[type_id];
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::get_skookum_property_binding_class_name(UProperty * property_p)
  {
  eSkTypeID type_id = get_skookum_property_type(property_p);

  FString prefix = TEXT("Sk");
  if (type_id == SkTypeID_Name
    || type_id == SkTypeID_Enum
    || type_id == SkTypeID_UStruct
    || type_id == SkTypeID_UClass
    || type_id == SkTypeID_UObject)
    {
    prefix = TEXT("SkUE");
    }

  return prefix + get_skookum_property_type_name(property_p);
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::get_cpp_class_name(UStruct * class_or_struct_p)
  {
  return FString::Printf(TEXT("%s%s"), class_or_struct_p->GetPrefixCPP(), *class_or_struct_p->GetName());
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::get_cpp_property_type_name(UProperty * property_p)
  {
  static FString decl_Enum(TEXT("enum "));
  static FString decl_Struct(TEXT("struct "));
  static FString decl_Class(TEXT("class "));
  static FString decl_TEnumAsByte(TEXT("TEnumAsByte<enum "));
  static FString decl_TSubclassOf(TEXT("TSubclassOf<class "));
  static FString decl_TSubclassOfShort(TEXT("TSubclassOf<"));
  static FString decl_TArray(TEXT("TArray"));

  FString property_type_name = property_p->GetCPPType(NULL, CPPF_ArgumentOrReturnValue);

  // Check for enum
  UByteProperty * byte_property_p = Cast<UByteProperty>(property_p);
  if (byte_property_p && byte_property_p->Enum && byte_property_p->Enum->GetCppForm() == UEnum::ECppForm::Regular)
    {
    property_type_name = TEXT("TEnumAsByte<") + byte_property_p->Enum->GetName() + TEXT(">");
    }

  // Strip any forward declaration keywords
  if (property_type_name.StartsWith(decl_Enum) || property_type_name.StartsWith(decl_Struct) || property_type_name.StartsWith(decl_Class))
    {
    int first_space_index = property_type_name.Find(TEXT(" "));
    property_type_name = property_type_name.Mid(first_space_index + 1);
    }
  else if (property_type_name.StartsWith(decl_TEnumAsByte))
    {
    int first_space_index = property_type_name.Find(TEXT(" "));
    property_type_name = TEXT("TEnumAsByte<") + property_type_name.Mid(first_space_index + 1);
    }
  else if (property_type_name.StartsWith(decl_TSubclassOf)
        || property_type_name.StartsWith(decl_TSubclassOfShort))
    {
    property_type_name = TEXT("UClass *");
    }
  else if (property_type_name.StartsWith(decl_TArray))
    {
    const UArrayProperty * array_property_p = Cast<UArrayProperty>(property_p);
    UProperty * element_property_p = array_property_p->Inner;
    property_type_name = FString::Printf(TEXT("TArray<%s>"), *get_cpp_property_type_name(element_property_p));
    }

  return property_type_name;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::get_cpp_property_cast_name(UProperty * property_p)
  {
  UByteProperty * byte_property_p = Cast<UByteProperty>(property_p);
  if (byte_property_p && byte_property_p->Enum && byte_property_p->Enum->GetCppForm() == UEnum::ECppForm::Regular)
    {
    return byte_property_p->Enum->GetName();
    }

  return get_cpp_property_type_name(property_p);
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::get_skookum_default_initializer(UFunction * function_p, UProperty * param_p)
  {
  FString default_value;

  // For Blueprintcallable functions, assume all arguments have defaults even if not specified
  bool has_default_value = function_p->HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_Exec); // || function_p->HasMetaData(*param_p->GetName());
  if (has_default_value)
    {
    default_value = function_p->GetMetaData(*param_p->GetName());
    }
  else
    {
    FName cpp_default_value_key(*(TEXT("CPP_Default_") + param_p->GetName()));
    has_default_value = function_p->HasMetaData(cpp_default_value_key);
    if (has_default_value)
      {
      default_value = function_p->GetMetaData(cpp_default_value_key);
      }
    }
  if (has_default_value)
    {
    // Trivial default?
    if (default_value.IsEmpty())
      {
      eSkTypeID type_id = get_skookum_property_type(param_p);
      switch (type_id)
        {
        case SkTypeID_Integer:         default_value = TEXT("0"); break;
        case SkTypeID_Real:            default_value = TEXT("0.0"); break;
        case SkTypeID_Boolean:         default_value = TEXT("false"); break;
        case SkTypeID_String:          default_value = TEXT("\"\""); break;
        case SkTypeID_Enum:            default_value = get_enum(param_p)->GetName() + TEXT(".@") + skookify_var_name(get_enum(param_p)->GetEnumName(0), false, true); break;
        case SkTypeID_Name:            default_value = TEXT("Name!none"); break;
        case SkTypeID_Vector2:
        case SkTypeID_Vector3:
        case SkTypeID_Vector4:
        case SkTypeID_Rotation:
        case SkTypeID_RotationAngles:
        case SkTypeID_Transform:
        case SkTypeID_Color:
        case SkTypeID_List:
        case SkTypeID_UStruct:         default_value = get_skookum_property_type_name(param_p) + TEXT("!"); break;
        case SkTypeID_UClass:
        case SkTypeID_UObject:         default_value = (param_p->GetName() == TEXT("WorldContextObject")) ? TEXT("@@world") : skookify_class_name(Cast<UObjectPropertyBase>(param_p)->PropertyClass->GetName()) + TEXT("!null"); break;
        }
      }
    else
      {
      // Remove variable assignments from default_value (e.g. "X=")
      for (int32 pos = 0; pos < default_value.Len() - 1; ++pos)
        {
        if (FChar::IsAlpha(default_value[pos]) && default_value[pos + 1] == '=')
          {
          default_value.RemoveAt(pos, 2);
          }
        }

      // Trim trailing zeros off floating point numbers
      for (int32 pos = 0; pos < default_value.Len(); ++pos)
        {
        if (FChar::IsDigit(default_value[pos]))
          {
          int32 npos = pos;
          while (npos < default_value.Len() && FChar::IsDigit(default_value[npos])) ++npos;
          if (npos < default_value.Len() && default_value[npos] == '.')
            {
            ++npos;
            while (npos < default_value.Len() && FChar::IsDigit(default_value[npos])) ++npos;
            int32 zpos = npos - 1;
            while (default_value[zpos] == '0') --zpos;
            if (default_value[zpos] == '.') ++zpos;
            ++zpos;
            if (npos > zpos) default_value.RemoveAt(zpos, npos - zpos);
            npos = zpos;
            }
          pos = npos;
          }
        }

      // Skookify the default argument
      eSkTypeID type_id = get_skookum_property_type(param_p);
      switch (type_id)
        {
        case SkTypeID_Integer:         break; // Leave as-is
        case SkTypeID_Real:            break; // Leave as-is
        case SkTypeID_Boolean:         default_value = default_value.ToLower(); break;
        case SkTypeID_String:          default_value = TEXT("\"") + default_value + TEXT("\""); break;
        case SkTypeID_Name:            default_value = TEXT("Name!(\"") + default_value + TEXT("\")"); break;
        case SkTypeID_Enum:            default_value = get_enum(param_p)->GetName() + TEXT(".@") + skookify_var_name(default_value, false, true); break;
        case SkTypeID_Vector2:         default_value = TEXT("Vector2!xy") + default_value; break;
        case SkTypeID_Vector3:         default_value = TEXT("Vector3!xyz(") + default_value + TEXT(")"); break;
        case SkTypeID_Vector4:         default_value = TEXT("Vector4!xyzw") + default_value; break;
        case SkTypeID_Rotation:        break; // Not implemented yet - leave as-is for now
        case SkTypeID_RotationAngles:  default_value = TEXT("RotationAngles!yaw_pitch_roll(") + default_value + TEXT(")"); break;
        case SkTypeID_Transform:       break; // Not implemented yet - leave as-is for now
        case SkTypeID_Color:           default_value = TEXT("Color!rgba") + default_value; break;
        case SkTypeID_UStruct:         if (default_value == TEXT("LatentInfo")) default_value = skookify_class_name(Cast<UStructProperty>(param_p)->Struct->GetName()) + TEXT("!"); break;
        case SkTypeID_UClass:          default_value = skookify_class_name(default_value) + TEXT(".static_class"); break;
        case SkTypeID_UObject:         if (default_value == TEXT("WorldContext") || default_value == TEXT("WorldContextObject") || param_p->GetName() == TEXT("WorldContextObject")) default_value = TEXT("@@world"); break;
        }
      }

    check(!default_value.IsEmpty()); // Default value must be non-empty at this point

    default_value = TEXT(" : ") + default_value;
    }

  return default_value;
  }

//=======================================================================================
// MethodBinding implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::MethodBinding::make_method(UFunction * function_p)
  {
  m_script_name = skookify_method_name(function_p->GetName(), function_p->GetReturnProperty());
  m_code_name = m_script_name.Replace(TEXT("?"), TEXT("_Q"));
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::MethodBinding::make_property_getter(UProperty * property_p)
  {
  m_script_name = skookify_method_name(property_p->GetName(), property_p);
  m_code_name = m_script_name.Replace(TEXT("?"), TEXT("_Q"));
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::MethodBinding::make_property_setter(UProperty * property_p)
  {
  m_script_name = skookify_method_name(property_p->GetName()) + TEXT("_set");
  m_code_name = m_script_name;
  }