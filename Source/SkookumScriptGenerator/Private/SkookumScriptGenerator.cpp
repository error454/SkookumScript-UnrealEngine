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

#include "ISkookumScriptGenerator.h"
#include "CoreUObject.h"
#include "Regex.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"

#include "SkookumScriptGeneratorBase.inl"

//#define USE_DEBUG_LOG_FILE

DEFINE_LOG_CATEGORY_STATIC(LogSkookumScriptGenerator, Log, All);

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

  // Processing phase of this plugin
  enum ePhase
    {
    Phase_gathering,  // Gathering information from UHT
    Phase_generating, // Generating type information
    Phase_saving,     // Saving out to disk
    };

  // Indicates where a class is being declared
  enum eClassScope
    {
    ClassScope_engine,    // Class is declared in engine
    ClassScope_project,   // Class is declared in project
    };

  // Information about a module we are exporting
  struct ModuleInfo
    {
    FString           m_name;
    eClassScope       m_scope;
    FString           m_generated_include_folder;
    TArray<UClass *>  m_classes_to_export;

    ModuleInfo(const FString & name) : m_name(name), m_scope(ClassScope_engine) { m_classes_to_export.Reserve(250); }
    bool operator == (const FString & module_name) const { return m_name == module_name; } // For using the TArray::FindByKey function
    bool operator == (const TCHAR * module_name_p) const { return m_name == module_name_p; } // For using the TArray::FindByKey function
    bool operator == (eClassScope scope) const { return m_scope == scope; } // For using the TArray::FindByKey function
    };

  // Information for engine/project code generation
  struct GenerationTarget
    {
    FString               m_root_directory_path;      // Root of the runtime plugin or project we're generating the code for
    FString               m_project_name;             // Name of this project
    TArray<ModuleInfo>    m_script_supported_modules; // List of module names specified in SkookumScript.ini
    TSet<FName>           m_skip_classes;             // All classes set to skip in SkookumScript.ini
    TArray<FString>       m_additional_includes;      // Workaround - extra header files to include

    mutable ModuleInfo *  m_current_module_p;  // Module that is currently processed by UHT

    void                initialize(const FString & root_directory_path, const FString & project_name, const GenerationTarget * inherit_from_p = nullptr);
    bool                begin_process_module(const FString & module_name, EBuildModuleType::Type module_type, const FString & module_generated_include_folder) const;
    bool                can_export_class(UClass * class_p) const;
    bool                can_export_struct(UStruct * struct_p) const;
    void                export_class(UClass * class_p);
    const ModuleInfo *  get_module(UField * type_p); // Determine module info of a type
    };

  // A type that we plan to generate
  struct TypeToGenerate
    {
    UField *  m_type_p;
    int32     m_include_priority;
    uint16    m_referenced_flags; // See enum eReferenced
    int16     m_generated_type_index; // Once generated, this is the array index of the corresponding generated type

    // Note: The "<" operator is deliberately used in reverse so that these structs will be sorted by include_priority in descending order
    bool operator < (const TypeToGenerate & other) const { return m_include_priority > other.m_include_priority; }
    };

  // All the data generated for a routine
  struct GeneratedRoutine
    {
    FString   m_name;
    bool      m_is_class_member;
    FString   m_body;
    };

  typedef TArray<GeneratedRoutine> tSkRoutines;

  enum eEventCoro
    {
    EventCoro_do,       // _on_x_do()
    EventCoro_do_until, // _on_x_do_until(;)
    EventCoro_wait,     // _wait_x(;)

    EventCoro__count
    };

  // All generated files relating to a class, struct or enum
  struct GeneratedType
    {
    // The type this is generated for
    UField * m_type_p;

    // Skookified name of this class/struct/enum
    FString m_sk_name;

    // Where is this class being used? (engine/game)
    eClassScope m_class_scope;

    // Is this a stub type only (a parent class from another overlay that we are just referencing)
    bool m_is_hierarchy_stub;

    // C++ bindings for this class
    FString m_cpp_header_file_body;         // C++ code to place in header file
    FString m_cpp_binding_file_body;        // C++ code to place in binding file
    FString m_cpp_register_static_ue_type;  // C++ to initialize UE class pointer
    FString m_cpp_register_static_sk_type;  // C++ to initialize Sk class pointer

    // Sk scripts for this class
    FString m_sk_meta_file_body;          // Sk class meta information
    FString m_sk_instance_data_file_body; // Sk instance data member declarations  
    FString m_sk_class_data_file_body;    // Sk class data member declarations  

    // Sk scripts for this class's methods
    tSkRoutines m_sk_routines;

    // For using the TArray::FindByKey function
    bool operator == (UField * type_p) const { return m_type_p == type_p; }
    };

  // Record include paths in the order they were encountered
  struct IncludeFilePath
    {
    FString             m_include_file_path;
    const ModuleInfo *  m_module_p;
    };

  // To keep track of method bindings generated for a particular class
  struct MethodBinding
    {
    void make_method(UFunction * function_p); // create names for a method

    bool operator == (const MethodBinding & other) const { return m_script_name == other.m_script_name; }

    UFunction * m_function_p;
    FString     m_script_name;  // Sk name of this method
    FString     m_code_name;    // C++ digestible version of the above (e.g. `?` replaced with `_Q`)
    };

  // To keep track of delegate bindings generated for a particular class
  struct EventBinding
    {
    void make_event(UMulticastDelegateProperty * property_p); // create names for an event

    UMulticastDelegateProperty *  m_property_p;
    FString                       m_script_name_base;  // Sk name base (without the `on_`)
    };

  enum eScope { Scope_instance, Scope_class }; // 0 = instance, 1 = static bindings

  // Array with instance and class methods for a given class
  struct RoutineBindings
    {
    TArray<MethodBinding>   m_method_bindings[2]; // Indexed via eScope
    TArray<EventBinding>    m_event_bindings;
    };

  //---------------------------------------------------------------------------------------
  // Data

  FString               m_binding_code_path_engine;       // Output folder for generated binding code files in engine
  FString               m_unreal_engine_root_path_local;  // Root of "Unreal Engine" folder on local machine
  FString               m_unreal_engine_root_path_build;  // Root of "Unreal Engine" folder for builds - may be different to m_unreal_engine_root_local if we're building remotely

  GenerationTarget      m_targets[2]; // Indexed by eClassScope (engine or project)

  ePhase                m_phase; // Current processing phase

  TArray<TypeToGenerate>  m_types_to_generate;        // Types that still need to be generated
  TMap<UField *, int32>   m_types_to_generate_lookup; // To quickly determine if we already requested a type to be looked up
  TArray<GeneratedType>   m_types_generated;          // Generated data for the types

  static const TCHAR *  ms_event_coro_fmts_pp[EventCoro__count]; // Format string for transforming the base name to its coroutine name
  static const TCHAR *  ms_event_coro_impl_fmts_pp[EventCoro__count]; // Format string for transforming the base name to its coroutine name

  static const FName    ms_meta_data_key_custom_structure_param;
  static const FName    ms_meta_data_key_array_parm;
  static const FName    ms_meta_data_key_module_relative_path;
  static const FName    ms_meta_data_key_custom_thunk;
  static const FName    ms_meta_data_key_cannot_implement_interface_in_blueprint;

#ifdef USE_DEBUG_LOG_FILE
  FILE *                m_debug_log_file; // Quick file handle to print debug stuff to, generates log file in output folder
#endif

  //---------------------------------------------------------------------------------------
  // Methods

  void                  generate_all_bindings(eClassScope class_scope);

  int32                 generate_class(UStruct * struct_or_class_p, int32 include_priority, uint32 referenced_flags, eClassScope class_scope); // Generate script and binding files for a class and its methods and properties
  FString               generate_class_header_file_body(UStruct * struct_or_class_p, const RoutineBindings & bindings, eClassScope class_scope); // Generate header file body for a class
  FString               generate_class_binding_file_body(UStruct * struct_or_class_p, const RoutineBindings & bindings); // Generate binding code source file for a class or struct

  int32                 generate_enum(UEnum * enum_p, int32 include_priority, uint32 referenced_flags, eClassScope class_scope); // Generate files for an enum
  FString               generate_enum_header_file_body(UEnum * enum_p, eClassScope class_scope); // Generate header file body for an enum
  FString               generate_enum_binding_file_body(UEnum * enum_p); // Generate binding code source file for an enum

  FString               generate_method_script_file_body(UFunction * function_p, const FString & script_function_name); // Generate script file for a method
  FString               generate_method_binding_code(const FString & class_name_cpp, UClass * class_p, const MethodBinding & binding); // Generate binding code for a method
  FString               generate_method_binding_code_body_via_call(const FString & class_name_cpp, UClass * class_p, const MethodBinding & binding); // Generate binding code for a method
  FString               generate_method_binding_code_body_via_event(const FString & class_name_cpp, UClass * class_p, const MethodBinding & binding); // Generate binding code for a method

  FString               generate_event_script_file_body(eEventCoro which, UMulticastDelegateProperty * delegate_property_p, const FString & script_base_name, FString * out_coro_name_p); // Generate script file for an event coroutine
  FString               generate_event_binding_code(const FString & class_name_cpp, UClass * class_p, const EventBinding & binding); // Generate binding code for a method  

  FString               generate_routine_script_parameters(UFunction * function_p, int32 indent_spaces, FString * out_return_type_name_p, int32 * out_num_inputs_p); // Generate script code for a routine's parameter list (without parentheses)

  FString               generate_method_binding_declaration(const FString & function_name, bool is_static); // Generate declaration of method binding function
  FString               generate_this_pointer_initialization(const FString & class_name_cpp, UStruct * struct_or_class_p, bool is_static); // Generate code that obtains the 'this' pointer from scope_p
  FString               generate_method_parameter_assignment(UProperty * param_p, int32 param_index, FString assignee_name);
  FString               generate_method_out_parameter_expression(UFunction * function_p, UProperty * param_p, int32 param_index, const FString & param_name);
  FString               generate_property_default_ctor_argument(UProperty * param_p);

  FString               generate_return_value_passing(UProperty * return_value_p, const FString & return_value_name); // Generate code that passes back the return value
  FString               generate_var_to_instance_expression(UProperty * var_p, const FString & var_name); // Generate code that creates an SkInstance from a property

  void                  save_generated_cpp_files(eClassScope class_scope);
  bool                  save_generated_script_files(eClassScope class_scope);

  bool                  can_export_enum(UEnum * enum_p);
  bool                  can_export_method(UFunction * function_p, int32 include_priority, uint32 referenced_flags, bool allow_delegate = false);

  FString               get_skookum_property_binding_class_name(UProperty * property_p);
  static FString        get_cpp_class_name(UStruct * struct_or_class_p);
  static FString        get_cpp_property_type_name(UProperty * property_p, bool is_array_element = false, bool with_const = false, bool with_ref = false);
  static FString        get_cpp_property_cast_name(UProperty * property_p); // Returns the type to be used to cast an assignment before assigning
  FString               get_skookum_default_initializer(UFunction * function_p, UProperty * param_p);

  void                  on_property_referenced(UProperty * prop_p, int32 include_priority, uint32 referenced_flags); // Generate script and binding files for a class from property
  void                  request_generate_type(UField * type_p, int32 include_priority, uint32 referenced_flags);

  // FSkookumScriptGeneratorBase interface implementation

  virtual bool          can_export_property(UProperty * property_p, int32 include_priority, uint32 referenced_flags) override final;
  virtual void          on_type_referenced(UField * type_p, int32 include_priority, uint32 referenced_flags) override final;
  virtual void          report_error(const FString & message) override final;

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

const TCHAR * FSkookumScriptGenerator::ms_event_coro_fmts_pp[EventCoro__count] = { TEXT("_on_%s_do"), TEXT("_on_%s_do_until"), TEXT("_wait_%s") };
const TCHAR * FSkookumScriptGenerator::ms_event_coro_impl_fmts_pp[EventCoro__count] = { TEXT("coro_on_event_do(scope_p, %s, false)"), TEXT("coro_on_event_do(scope_p, %s, true)"), TEXT("coro_wait_event(scope_p, %s)") };

const FName FSkookumScriptGenerator::ms_meta_data_key_custom_structure_param(TEXT("CustomStructureParam"));
const FName FSkookumScriptGenerator::ms_meta_data_key_array_parm(TEXT("ArrayParm"));
const FName FSkookumScriptGenerator::ms_meta_data_key_module_relative_path(TEXT("ModuleRelativePath"));
const FName FSkookumScriptGenerator::ms_meta_data_key_custom_thunk(TEXT("CustomThunk"));
const FName FSkookumScriptGenerator::ms_meta_data_key_cannot_implement_interface_in_blueprint(TEXT("CannotImplementInterfaceInBlueprint"));


//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::Initialize(const FString & root_local_path, const FString & root_build_path, const FString & output_directory, const FString & include_base)
  {
  m_binding_code_path_engine = output_directory;
  m_unreal_engine_root_path_local = root_local_path;
  m_unreal_engine_root_path_build = root_build_path;

  // Set up information about engine and project code generation
  m_targets[ClassScope_engine].initialize(include_base / TEXT("../.."), TEXT("UE4"));
  m_targets[ClassScope_project].initialize(FPaths::GetPath(FPaths::GetProjectFilePath()), FPaths::GetBaseFilename(FPaths::GetProjectFilePath()), &m_targets[ClassScope_engine]);

  // Use some conservative estimates to avoid unnecessary reallocations
  m_types_to_generate.Reserve(3000);
  m_types_to_generate_lookup.Reserve(3000);
  m_types_generated.Reserve(3000);

  m_phase = Phase_gathering;

  // Create debug log file
  #ifdef USE_DEBUG_LOG_FILE
    m_debug_log_file = _wfopen(*(output_directory / TEXT("SkookumScriptGenerator.log.txt")), TEXT("w"));
  #endif
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::ShouldExportClassesForModule(const FString & module_name, EBuildModuleType::Type module_type, const FString & module_generated_include_folder) const
  {
  bool should_export_engine  = m_targets[ClassScope_engine].begin_process_module(module_name, module_type, module_generated_include_folder);
  bool should_export_project = m_targets[ClassScope_project].begin_process_module(module_name, module_type, module_generated_include_folder);
  if (should_export_project && !should_export_engine && module_type != EBuildModuleType::GameRuntime)
    {
    // TODO fix this - should be easy, just didn't get to it
    // When C++ files are generated, check not only class scope but also in which SkookumScript.ini file it's declared
    GWarn->Log(ELogVerbosity::Warning, FString::Printf(TEXT("Module '%s' specified in game project SkookumScript.ini file is not a game module. Currently, only game modules are supported. No bindings will be generated for it."), *module_name));
    should_export_project = false;
    }
  return should_export_engine || should_export_project;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::ExportClass(UClass * class_p, const FString & source_header_file_name, const FString & generated_header_file_name, bool has_changed)
  {
  m_targets[ClassScope_engine].export_class(class_p);
  m_targets[ClassScope_project].export_class(class_p);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::FinishExport()
  {
  // Generate bindings
  generate_all_bindings(ClassScope_engine);
  generate_all_bindings(ClassScope_project);

  // Make files "live"
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

void FSkookumScriptGenerator::generate_all_bindings(eClassScope class_scope)
  {
  // Generating phase first
  m_phase = Phase_generating;

  // Get target info
  const GenerationTarget & target = m_targets[class_scope];
  // Bail if nothing to do
  if (target.m_script_supported_modules.Num() == 0)
    {
    return;
    }

  // 1) Determine which modules to export
  TArray<const ModuleInfo *> modules_to_generate;
  modules_to_generate.Reserve(target.m_script_supported_modules.Num());
  for (const ModuleInfo & module : target.m_script_supported_modules)
    {
    modules_to_generate.Add(&module);
    }

  // 2) Seed the types to generate with the classes to export
  m_types_to_generate.Empty();
  m_types_to_generate_lookup.Empty();
  TSet<UObject *> used_packages;
  used_packages.Reserve(target.m_script_supported_modules.Num());
  const int32 module_order_bias_step = 10; // Decrease module priority by this value per module
  int32 module_order_bias = modules_to_generate.Num() * module_order_bias_step; // Start at high priority and go down
  for (const ModuleInfo * module_p : modules_to_generate)
    {
    for (UClass * class_p : module_p->m_classes_to_export)
      {
      request_generate_type(class_p, module_order_bias, module_p->m_scope == ClassScope_project ? Referenced_by_game_module : 0);
      used_packages.Add(class_p->GetOuterUPackage());
      }
    module_order_bias -= module_order_bias_step;
    }

  // 3) Also request generation of all structs contained in the same packages as all classes
  TArray<UObject *> obj_array;
  obj_array.Reserve(1500);
  GetObjectsOfClass(UScriptStruct::StaticClass(), obj_array, false, RF_ClassDefaultObject);
  for (auto obj_p : obj_array)
    {
    if (used_packages.Contains(obj_p->GetOutermost()))
      {
      UScriptStruct * struct_p = static_cast<UScriptStruct *>(obj_p);
      if (m_targets[class_scope].can_export_struct(struct_p))
        {
        request_generate_type(struct_p, 0, 0);
        }
      }
    }

#if 0
  // 4) This does currently not work for enums that are within WITH_EDITOR or WITH_EDITORONLY_DATA blocks
  // as we cannot detect from the UEnum if that's the case, causing compile errors in cooked builds

  // Export all enums
  obj_array.Reset();
  GetObjectsOfClass(UEnum::StaticClass(), obj_array, false, RF_ClassDefaultObject);
  for (auto obj_p : obj_array)
    {
    if (used_packages.Contains(obj_p->GetOutermost()))
      {
      UEnum * enum_p = static_cast<UEnum *>(obj_p);
      if (can_export_enum(enum_p))
        {
        request_generate_type(enum_p, 0);
        }
      }
    }
#endif

  // 5) Loop until all types have been generated
  // Note that the array will grow as we generate but the index will eventually catch up
  m_types_generated.Empty();
  for (int32 i = 0; i < m_types_to_generate.Num(); ++i)
    {
    // Get next element in list
    const TypeToGenerate & type_to_generate = m_types_to_generate[i];

    // Generate structure for it (if not there already)
    UStruct * struct_or_class_p;
    int32 generated_index;
    if ((struct_or_class_p = Cast<UStruct>(type_to_generate.m_type_p)) != nullptr)
      {
      generated_index = generate_class(struct_or_class_p, type_to_generate.m_include_priority, type_to_generate.m_referenced_flags, class_scope);
      }
    else
      {
      generated_index = generate_enum(CastChecked<UEnum>(type_to_generate.m_type_p), type_to_generate.m_include_priority, type_to_generate.m_referenced_flags, class_scope);
      }

    // Remember index for later
    m_types_to_generate[i].m_generated_type_index = (int16)generated_index;
    }

  // Now we are saving things out
  m_phase = Phase_saving;

  // Now that all is generated, write it to disk
  // First the cpp files to consume the m_types_to_generate array
  save_generated_cpp_files(class_scope);
  // Then generate script files which will reuse m_types_to_generate
  save_generated_script_files(class_scope);
  }

//---------------------------------------------------------------------------------------

int32 FSkookumScriptGenerator::generate_class(UStruct * struct_or_class_p, int32 include_priority, uint32 referenced_flags, eClassScope class_scope)
  {
  UE_LOG(LogSkookumScriptGenerator, Log, TEXT("Generating struct/class %s"), *get_skookum_class_name(struct_or_class_p));

  // Allocate structure
  int32 generated_index = m_types_generated.Emplace();
  GeneratedType & generated_class = m_types_generated[generated_index];

  FString skookum_class_name = get_skookum_class_name(struct_or_class_p);
  const ModuleInfo * module_p = m_targets[class_scope].get_module(struct_or_class_p);

  // Remember info about class
  generated_class.m_type_p = struct_or_class_p;
  generated_class.m_sk_name = skookum_class_name;
  generated_class.m_class_scope = module_p ? module_p->m_scope : ClassScope_engine;

  // Determine if it's just a stub (i.e. Sk built-in struct like Vector3, Transform, SkookumScriptBehaviorComponent etc.)
  eSkTypeID type_id = get_skookum_struct_type(struct_or_class_p);
  bool has_built_in_name = struct_or_class_p->GetName() == TEXT("SkookumScriptBehaviorComponent");
  generated_class.m_is_hierarchy_stub = (type_id != SkTypeID_UStruct && type_id != SkTypeID_UClass) || has_built_in_name; // || !module_p;

  // Generate meta file
  generated_class.m_sk_meta_file_body = generate_class_meta_file_body(struct_or_class_p);

  // Generate full class?
  if (!generated_class.m_is_hierarchy_stub)
    {
    // Generate instance data members
    generated_class.m_sk_instance_data_file_body = generate_class_instance_data_file_body(struct_or_class_p, include_priority, referenced_flags);

    // For structs, generate ctor/ctor_copy/op_assign/dtor
    if (!Cast<UClass>(struct_or_class_p))
      {
      generated_class.m_sk_routines.Add({ TEXT("!"), false, FString::Printf(TEXT("() %s\r\n"), *skookum_class_name) });
      generated_class.m_sk_routines.Add({ TEXT("!copy"), false, FString::Printf(TEXT("(%s other) %s\r\n"), *skookum_class_name, *skookum_class_name) });
      generated_class.m_sk_routines.Add({ TEXT("assign"), false, FString::Printf(TEXT("(%s other) %s\r\n"), *skookum_class_name, *skookum_class_name) });
      generated_class.m_sk_routines.Add({ TEXT("!!"), false, TEXT("()\r\n") });
      }

    // Build array of all methods (only for classes)
    // And generate script code
    RoutineBindings bindings;
    UClass * class_p = Cast<UClass>(struct_or_class_p);
    if (class_p)
      {
      MethodBinding method;
      for (TFieldIterator<UFunction> func_it(struct_or_class_p, EFieldIteratorFlags::ExcludeSuper); func_it; ++func_it)
        {
        UFunction * function_p = *func_it;
        if (can_export_method(function_p, include_priority, referenced_flags | (class_scope == generated_class.m_class_scope ? Referenced_as_binding_class : 0)))
          {
          method.make_method(function_p);
          if (bindings.m_method_bindings[Scope_instance].Find(method) < 0 && bindings.m_method_bindings[Scope_class].Find(method) < 0) // If method with this name already bound, assume it does the same thing and skip
            {
            // Remember binding for later
            bool is_static = function_p->HasAnyFunctionFlags(FUNC_Static);
            bindings.m_method_bindings[is_static ? Scope_class : Scope_instance].Push(method);

            // Generate script code
            generated_class.m_sk_routines.Add({ method.m_script_name, is_static, generate_method_script_file_body(function_p, method.m_script_name) });
            }
          }
        }

      EventBinding event;
      for (TFieldIterator<UProperty> property_it(struct_or_class_p, EFieldIteratorFlags::ExcludeSuper); property_it; ++property_it)
        {
        UMulticastDelegateProperty * delegate_property_p = Cast<UMulticastDelegateProperty>(*property_it);
        if (delegate_property_p 
         && delegate_property_p->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPublic)
         && !delegate_property_p->HasAnyPropertyFlags(CPF_EditorOnly)
         && can_export_method(delegate_property_p->SignatureFunction, include_priority, referenced_flags | (class_scope == generated_class.m_class_scope ? Referenced_as_binding_class : 0), true))
          {
          // Remember binding for later
          event.make_event(delegate_property_p);
          bindings.m_event_bindings.Push(event);

          // Generate script code
          for (int i = 0; i < EventCoro__count; ++i)
            {
            FString coro_name;
            FString coro_body = generate_event_script_file_body(eEventCoro(i), delegate_property_p, event.m_script_name_base, &coro_name);
            generated_class.m_sk_routines.Add({ coro_name, false, coro_body });
            }
          }
        }
      }

    // Generate binding code files
    const TCHAR * class_or_struct_text_p          = (type_id == SkTypeID_UClass ? TEXT("class") : TEXT("struct"));
    generated_class.m_cpp_header_file_body        = generate_class_header_file_body(struct_or_class_p, bindings, generated_class.m_class_scope);
    generated_class.m_cpp_binding_file_body       = generate_class_binding_file_body(struct_or_class_p, bindings);    
    generated_class.m_cpp_register_static_ue_type = FString::Printf(TEXT("SkUEClassBindingHelper::register_static_%s(SkUE%s::ms_u%s_p = FindObjectChecked<%s>(ANY_PACKAGE, TEXT(\"%s\")));"), class_or_struct_text_p, *skookum_class_name, class_or_struct_text_p, class_p ? TEXT("UClass") : TEXT("UStruct"), *struct_or_class_p->GetName());
    generated_class.m_cpp_register_static_sk_type = FString::Printf(TEXT("SkUEClassBindingHelper::add_static_%s_mapping(SkUE%s::initialize_class(0x%08x), SkUE%s::ms_u%s_p);"), class_or_struct_text_p, *skookum_class_name, get_skookum_symbol_id(*skookum_class_name), *skookum_class_name, class_or_struct_text_p);
    }

  return generated_index;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_class_header_file_body(UStruct * struct_or_class_p, const RoutineBindings & bindings, eClassScope class_scope)
  {
  FString skookum_class_name = get_skookum_class_name(struct_or_class_p);
  FString cpp_class_name = get_cpp_class_name(struct_or_class_p);
  const TCHAR * api_decl_p = class_scope == ClassScope_engine ? TEXT("SKOOKUMSCRIPTRUNTIME_API ") : TEXT("");

  UClass * class_p = Cast<UClass>(struct_or_class_p);

  FString header_code = FString::Printf(TEXT("class SkUE%s : public SkUEClassBinding%s<SkUE%s, %s>\r\n  {\r\n"),
    *skookum_class_name,
    class_p ? (class_p->HasAnyCastFlag(CASTCLASS_AActor) ? TEXT("Actor") : TEXT("Entity"))
    : (is_pod(struct_or_class_p) ? TEXT("StructPod") : TEXT("Struct")),
    *skookum_class_name,
    *cpp_class_name);

  header_code += TEXT("  public:\r\n");
  header_code += FString::Printf(TEXT("    static %sSkClass * get_class();\r\n"), api_decl_p);
  if (class_p && bindings.m_method_bindings[Scope_instance].Num() + bindings.m_method_bindings[Scope_class].Num() + bindings.m_event_bindings.Num() > 0)
    {
    header_code += TEXT("    static void      register_bindings();\r\n");
    }
  else
    {
    header_code += FString::Printf(TEXT("    using %s::register_bindings;\r\n"), class_p ? TEXT("tBindingEntity") : TEXT("tBindingStruct"));
    }
  header_code += TEXT("  };\r\n\r\n");

  return header_code;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_class_binding_file_body(UStruct * struct_or_class_p, const RoutineBindings & bindings)
  {
  const FString skookum_class_name = get_skookum_class_name(struct_or_class_p);
  const FString cpp_class_name = get_cpp_class_name(struct_or_class_p);

  FString generated_code;

  // Get class function
  generated_code += FString::Printf(TEXT("\r\nSkClass * SkUE%s::get_class() { return ms_class_p; }\r\n"), *skookum_class_name);

  UClass * class_p = Cast<UClass>(struct_or_class_p);
  if (class_p && bindings.m_method_bindings[Scope_instance].Num() + bindings.m_method_bindings[Scope_class].Num() + bindings.m_event_bindings.Num() > 0)
    {
    generated_code += FString::Printf(TEXT("\r\nnamespace SkUE%s_Impl\r\n  {\r\n\r\n"), *skookum_class_name);

    for (uint32 scope = 0; scope < 2; ++scope)
      {
      for (auto & method : bindings.m_method_bindings[scope])
        {
        generated_code += generate_method_binding_code(cpp_class_name, class_p, method);
        }
      }

    for (auto & event : bindings.m_event_bindings)
      {
      generated_code += generate_event_binding_code(cpp_class_name, class_p, event);
      }

    // Binding array
    for (uint32 scope = 0; scope < 2; ++scope)
      {
      if (bindings.m_method_bindings[scope].Num() > 0)
        {
        generated_code += FString::Printf(TEXT("  static const SkClass::MethodInitializerFuncId methods_%c[] =\r\n    {\r\n"), scope ? TCHAR('c') : TCHAR('i'));
        for (auto & method : bindings.m_method_bindings[scope])
          {
          generated_code += FString::Printf(TEXT("      { 0x%08x, mthd%s_%s },\r\n"), get_skookum_symbol_id(*method.m_script_name), scope ? TEXT("c") : TEXT(""), *method.m_code_name);
          }
        generated_code += TEXT("    };\r\n\r\n");
        }
      }
    if (bindings.m_event_bindings.Num() > 0)
      {
      generated_code += FString::Printf(TEXT("  static const SkClass::CoroutineInitializerFuncId coroutines_i[] =\r\n    {\r\n"));
      for (auto & event : bindings.m_event_bindings)
        {
        for (int32 i = 0; i < EventCoro__count; ++i)
          {
          FString coro_name = FString::Printf(ms_event_coro_fmts_pp[i], *event.m_script_name_base);
          generated_code += FString::Printf(TEXT("      { 0x%08x, coro%s },\r\n"), get_skookum_symbol_id(*coro_name), *coro_name);
          }
        }
      generated_code += TEXT("    };\r\n\r\n");
      }

    // Close namespace
    generated_code += FString::Printf(TEXT("  } // SkUE%s_Impl\r\n\r\n"), *skookum_class_name);

    // Register bindings function
    generated_code += FString::Printf(TEXT("void SkUE%s::register_bindings()\r\n  {\r\n"), *skookum_class_name);

    generated_code += FString::Printf(TEXT("  %s::register_bindings();\r\n"), class_p ? TEXT("tBindingEntity") : TEXT("tBindingStruct"));

    if (class_p)
      {
      for (uint32 scope = 0; scope < 2; ++scope)
        {
        if (bindings.m_method_bindings[scope].Num() > 0)
          {
          generated_code += FString::Printf(TEXT("  ms_class_p->register_method_func_bulk(SkUE%s_Impl::methods_%c, %d, %s);\r\n"), *skookum_class_name, scope ? TCHAR('c') : TCHAR('i'), bindings.m_method_bindings[scope].Num(), scope ? TEXT("SkBindFlag_class_no_rebind") : TEXT("SkBindFlag_instance_no_rebind"));
          }
        }
      if (bindings.m_event_bindings.Num() > 0)
        {
        generated_code += FString::Printf(TEXT("  ms_class_p->register_coroutine_func_bulk(SkUE%s_Impl::coroutines_i, %d, %s);\r\n"), *skookum_class_name, 3 * bindings.m_event_bindings.Num(), TEXT("eSkBindFlag(SkBindFlag_instance_no_rebind | SkBindFlag__arg_user_data)"));
        }
      }

    generated_code += TEXT("  }\r\n\r\n");
    }

  return generated_code;
  }

//---------------------------------------------------------------------------------------

int32 FSkookumScriptGenerator::generate_enum(UEnum * enum_p, int32 include_priority, uint32 referenced_flags, eClassScope class_scope)
  {
  // Allocate structure
  int32 generated_index = m_types_generated.Emplace();
  GeneratedType & generated_enum = m_types_generated[generated_index];

  FString enum_type_name = get_skookum_class_name(enum_p);
  const ModuleInfo * module_p = m_targets[class_scope].get_module(enum_p);

  // Remember info about class
  generated_enum.m_type_p = enum_p;
  generated_enum.m_sk_name = enum_type_name;
  generated_enum.m_class_scope = module_p ? module_p->m_scope : ClassScope_engine;
  generated_enum.m_is_hierarchy_stub = false; // || !module_p;

  // Generate meta file
  generated_enum.m_sk_meta_file_body = generate_class_meta_file_body(enum_p);

  // Generate full class?
  if (!generated_enum.m_is_hierarchy_stub)
    {
    // Class data members and class constructor
    FString data_body;
    FString constructor_body = FString::Printf(TEXT("// %s\r\n// EnumPath: %s\r\n\r\n()\r\n\r\n  [\r\n"), *enum_type_name, *enum_p->GetPathName());
    int32 max_name_length = 0;
    // Pass 0: Compute max_name_length
    // Pass 1: Generate the data
    for (uint32_t pass = 0; pass < 2; ++pass)
      {
      for (int32 enum_index = 0; enum_index < enum_p->NumEnums() - 1; ++enum_index)
        {
        FString enum_val_name = enum_p->GetNameStringByIndex(enum_index);
        FString enum_val_full_name = enum_p->GenerateFullEnumName(*enum_val_name);
        FString skookified_val_name = skookify_var_name(enum_val_name, false, VarScope_class);

        FName token = FName(*enum_val_full_name, FNAME_Find);
        if (token != NAME_None)
          {
          int32 enum_value = UEnum::LookupEnumName(token);
          if (enum_value != INDEX_NONE)
            {
            if (pass == 0)
              {
              max_name_length = FMath::Max(max_name_length, skookified_val_name.Len());
              }
            else
              {
              data_body += FString::Printf(TEXT("%s !%s\r\n"), *enum_type_name, *skookified_val_name);
              constructor_body += FString::Printf(TEXT("  %s %s!int(%d)\r\n"), *(skookified_val_name + TEXT(":")).RightPad(max_name_length + 1), *enum_type_name, enum_value);
              }
            }
          }
        }
      }
    constructor_body += TEXT("  ]\r\n");
    generated_enum.m_sk_class_data_file_body = data_body;
    generated_enum.m_sk_routines.Add({ TEXT("!"), true, constructor_body });

    // Generate binding code files
    generated_enum.m_cpp_header_file_body         = generate_enum_header_file_body(enum_p, generated_enum.m_class_scope);
    generated_enum.m_cpp_binding_file_body        = generate_enum_binding_file_body(enum_p);
    generated_enum.m_cpp_register_static_ue_type  = FString::Printf(TEXT("SkUEClassBindingHelper::register_static_enum(SkUE%s::ms_uenum_p = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT(\"%s\")));"), *enum_type_name, *enum_p->GetName());
    generated_enum.m_cpp_register_static_sk_type  = FString::Printf(TEXT("SkUEClassBindingHelper::add_static_enum_mapping(SkUE%s::ms_class_p = SkBrain::get_class(ASymbol::create_existing(0x%08x)), SkUE%s::ms_uenum_p);"), *enum_type_name, get_skookum_symbol_id(*enum_type_name), *enum_type_name);
    }

  return generated_index;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_enum_header_file_body(UEnum * enum_p, eClassScope class_scope)
  {
  const TCHAR * api_decl_p = class_scope == ClassScope_engine ? TEXT("SKOOKUMSCRIPTRUNTIME_API ") : TEXT("");
  FString enum_type_name = get_skookum_class_name(enum_p);

  FString generated_code;
  generated_code += FString::Printf(TEXT("class SkUE%s : public SkEnum\n  {\r\n  public:\r\n    static SkClass *     ms_class_p;\r\n    static UEnum *       ms_uenum_p;\r\n"), *enum_type_name);
  generated_code += FString::Printf(TEXT("    static %sSkClass *     get_class();\r\n"), api_decl_p);
  generated_code += FString::Printf(TEXT("    static SkInstance *  new_instance(%s value) { return SkEnum::new_instance((SkEnumType)value, get_class()); }\r\n"), *enum_p->CppType);
  generated_code += TEXT("  };\r\n\r\n");
  return generated_code;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_enum_binding_file_body(UEnum * enum_p)
  {
  FString enum_type_name = get_skookum_class_name(enum_p);

  FString generated_code;
  generated_code += FString::Printf(TEXT("SkClass * SkUE%s::ms_class_p;\r\nUEnum *   SkUE%s::ms_uenum_p;\r\n"), *enum_type_name, *enum_type_name);
  generated_code += FString::Printf(TEXT("SkClass * SkUE%s::get_class() { return ms_class_p; }\r\n\r\n"), *enum_type_name);
  return generated_code;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_script_file_body(UFunction * function_p, const FString & script_function_name)
  {
  // Generate method content
  FString method_body = get_comment_block(function_p);

  // Generate aka annotations
  method_body += FString::Printf(TEXT("&aka(\"%s\")\r\n"), *function_p->GetName());
  if (function_p->HasMetaData(ms_meta_data_key_display_name))
    {
    FString display_name = function_p->GetMetaData(ms_meta_data_key_display_name);
    if (display_name != function_p->GetName())
      {
      method_body += FString::Printf(TEXT("&aka(\"%s\")\r\n"), *display_name);
      }
    }
  method_body += TEXT("\r\n");

  // Generate parameter list
  FString return_type_name;
  int32 num_inputs;
  method_body += TEXT("(") + generate_routine_script_parameters(function_p, 1, &return_type_name, &num_inputs);

  // Place return type on new line if present and more than one parameter
  if (num_inputs > 1 && !return_type_name.IsEmpty())
    {
    method_body += TEXT("\n");
    }
  method_body += TEXT(") ") + return_type_name + TEXT("\n");

  return method_body;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_binding_code(const FString & class_name_cpp, UClass * class_p, const MethodBinding & binding)
  {
  // Generate code for the function body
  FString function_body;
  if (binding.m_function_p->HasAnyFunctionFlags(FUNC_Public) 
   && !binding.m_function_p->HasMetaData(ms_meta_data_key_custom_structure_param)   // Never call custom thunks directly
   && !binding.m_function_p->HasMetaData(ms_meta_data_key_array_parm)               // Never call custom thunks directly
   && !class_p->HasMetaData(ms_meta_data_key_cannot_implement_interface_in_blueprint) // Never call UINTERFACE methods directly
   && !binding.m_function_p->HasAllFunctionFlags(FUNC_BlueprintEvent|FUNC_Native))  // Never call blueprint native functions directly
    {
    // Public function, might be called via direct call
    if (binding.m_function_p->HasAnyFunctionFlags(FUNC_RequiredAPI) || class_p->HasAnyClassFlags(CLASS_RequiredAPI))
      {
      // This function is always safe to call directly
      function_body = generate_method_binding_code_body_via_call(class_name_cpp, class_p, binding);
      }
    else
      {
      // This function is called directly in monolithic builds, via event otherwise
      function_body += TEXT("  #if IS_MONOLITHIC\r\n");
      function_body += generate_method_binding_code_body_via_call(class_name_cpp, class_p, binding);
      function_body += TEXT("  #else\r\n");
      function_body += generate_method_binding_code_body_via_event(class_name_cpp, class_p, binding);
      function_body += TEXT("  #endif\r\n");
      }
    }
  else
    {
    // This function is protected and always called via event
    function_body = generate_method_binding_code_body_via_event(class_name_cpp, class_p, binding);
    }

  // Assemble function definition
  FString generated_code = FString::Printf(TEXT("  %s\r\n    {\r\n"), *generate_method_binding_declaration(*binding.m_code_name, binding.m_function_p->HasAnyFunctionFlags(FUNC_Static)));
  generated_code += function_body;
  generated_code += TEXT("    }\r\n\r\n");

  return generated_code;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_binding_code_body_via_call(const FString & class_name_cpp, UClass * class_p, const MethodBinding & binding)
  {
  bool is_static = binding.m_function_p->HasAnyFunctionFlags(FUNC_Static);

  FString function_body;

  // Need this pointer only for instance methods
  if (!is_static)
    {
    function_body += FString::Printf(TEXT("    %s\r\n"), *generate_this_pointer_initialization(class_name_cpp, class_p, is_static));
    }

  FString out_params;
  UProperty * return_value_p = nullptr;
  const bool has_params_or_return_value = (binding.m_function_p->Children != NULL);
  if (has_params_or_return_value)
    {
    for (TFieldIterator<UProperty> param_it(binding.m_function_p); param_it; ++param_it)
      {
      UProperty * param_p = *param_it;
      function_body += FString::Printf(TEXT("    %s %s;\r\n"), *get_cpp_property_type_name(param_p), *param_p->GetName());
      }
    int32 param_index = 0;
    for (TFieldIterator<UProperty> param_it(binding.m_function_p); param_it; ++param_it, ++param_index)
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
        out_params += FString::Printf(TEXT("    %s;\r\n"), *generate_method_out_parameter_expression(binding.m_function_p, param_p, param_index, param_p->GetName()));
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
  function_invocation += binding.m_function_p->GetName();

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
  for (TFieldIterator<UProperty> param_it(binding.m_function_p); param_it; ++param_it)
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

FString FSkookumScriptGenerator::generate_method_binding_code_body_via_event(const FString & class_name_cpp, UClass * class_p, const MethodBinding & binding)
  {
  bool is_static = binding.m_function_p->HasAnyFunctionFlags(FUNC_Static);

  FString function_body;
  function_body += FString::Printf(TEXT("    %s\r\n"), *generate_this_pointer_initialization(class_name_cpp, class_p, is_static));

  FString params;
  FString out_params;
  UProperty * return_value_p = nullptr;

  const bool has_params_or_return_value = (binding.m_function_p->Children != NULL);
  if (has_params_or_return_value)
    {
    params += TEXT("    struct FDispatchParams\r\n      {\r\n");

    for (TFieldIterator<UProperty> param_it(binding.m_function_p); param_it; ++param_it)
      {
      UProperty * param_p = *param_it;
      params += FString::Printf(TEXT("      %s %s;\r\n"), *get_cpp_property_type_name(param_p), *param_p->GetName());
      }
    params += TEXT("      } params;\r\n");
    int32 param_index = 0;
    for (TFieldIterator<UProperty> param_it(binding.m_function_p); param_it; ++param_it, ++param_index)
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
        out_params += FString::Printf(TEXT("    %s;\r\n"), *generate_method_out_parameter_expression(binding.m_function_p, param_p, param_index, param_in_struct));
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
  params += indent + FString::Printf(TEXT("    static UFunction * function_p = this_p->FindFunctionChecked(TEXT(\"%s\"));\r\n"), *binding.m_function_p->GetName());

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

FString FSkookumScriptGenerator::generate_event_script_file_body(eEventCoro which, UMulticastDelegateProperty * delegate_property_p, const FString & script_base_name, FString * out_coro_name_p)
  {
  // Determine name
  *out_coro_name_p = FString::Printf(ms_event_coro_fmts_pp[which], *script_base_name);

  // Determine comment
  static TCHAR const * const s_comment_formats_pp[EventCoro__count] =
    {
    TEXT(
      "//---------------------------------------------------------------------------------------\n"
      "// Whenever a `%s` event occurs on this `%s`, run `code` on it.\n"
      "// This coroutine never finishes by itself and can only be terminated externally.\n"
      "//---------------------------------------------------------------------------------------\n"
      "//\n"),
    TEXT(
      "//---------------------------------------------------------------------------------------\n"
      "// Whenever a `%s` event occurs on this `%s`, run `code` on it.\n"
      "// If `code` returns `false`, continue waiting for next event,\n"
      "// otherwise, exit and return the event parameters\n"
      "//---------------------------------------------------------------------------------------\n"
      "//\n"),
    TEXT(
      "//---------------------------------------------------------------------------------------\n"
      "// Wait for a `%s` event to occur on this `%s`.\n"
      "//\n"
      "// IMPORTANT: Do not use this coroutine if several of the events can potentially\n"
      "// occur within the same frame, as only the first one will be seen!\n"
      "// In that case use `_on_%s_do` or `_on_%s_do_until` instead.\n"
      "//---------------------------------------------------------------------------------------\n"
      "//\n")
    };

  // Generate coroutine content
  FString coro_body = FString::Printf(s_comment_formats_pp[which], *delegate_property_p->GetName(), *delegate_property_p->GetOwnerClass()->GetName(), *script_base_name, *script_base_name);
  coro_body += get_comment_block(delegate_property_p);

  // Generate parameter list
  int32 num_inputs;
  // 1) The closure
  coro_body += TEXT("(");
  if (which != EventCoro_wait)
    {
    coro_body += TEXT("(") + generate_routine_script_parameters(delegate_property_p->SignatureFunction, 2, nullptr, &num_inputs);
    if (num_inputs > 1) coro_body += TEXT("\n ");
    coro_body += TEXT(") ");
    if (which == EventCoro_do_until) coro_body += TEXT("Boolean ");
    coro_body += TEXT("code");
    }
  if (delegate_property_p->SignatureFunction->NumParms) coro_body += TEXT(";");
  coro_body += TEXT("\n");
  // 2) The outputs
  coro_body += TEXT(" ") + generate_routine_script_parameters(delegate_property_p->SignatureFunction, 1, nullptr, &num_inputs);
  coro_body += TEXT(")\n");

  return coro_body;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_event_binding_code(const FString & class_name_cpp, UClass * class_p, const EventBinding & binding)
  {
  FString generated_code;

  // 1) Create subclass of USkookumScriptListener that implements the event callback and event install/uninstall functions
  
  generated_code += FString::Printf(TEXT(
    "  class USkookumScriptListener_%s : public USkookumScriptListener\r\n"
    "    {\r\n"
    "    public:\r\n"), *binding.m_property_p->GetName());
  
  // The callback function
  generated_code += FString::Printf(TEXT("      void %s("), *binding.m_property_p->GetName());
  const TCHAR * separator_p = TEXT("");
  for (TFieldIterator<UProperty> param_it(binding.m_property_p->SignatureFunction); param_it; ++param_it)
    {
    UProperty * param_p = *param_it;
    generated_code += FString::Printf(TEXT("%s%s %s"), separator_p, *get_cpp_property_type_name(param_p, false, true, true), *param_p->GetName());
    separator_p = TEXT(", ");
    }
  generated_code += TEXT(
    ")\r\n"
    "        {\r\n"
    "        EventInfo * event_p = alloc_event();\r\n");
  int32 param_index = 0;
  if (binding.m_property_p->SignatureFunction->Children)
    {
    for (TFieldIterator<UProperty> param_it(binding.m_property_p->SignatureFunction); param_it; ++param_it)
      {
      UProperty * param_p = *param_it;
      bool use_const_cast = false;
      if (param_p->HasAnyPropertyFlags(CPF_ConstParm))
        {
        eSkTypeID type_id = get_skookum_property_type(param_p, true);
        use_const_cast = (type_id == FSkookumScriptGeneratorBase::SkTypeID_UObject || type_id == FSkookumScriptGeneratorBase::SkTypeID_UObjectWeakPtr);
        }
      FString argument = use_const_cast ? FString::Printf(TEXT("const_cast<%s>(%s)"), *get_cpp_property_type_name(param_p, false, false, true), *param_p->GetName()) : param_p->GetName();
      generated_code += FString::Printf(TEXT("        event_p->m_argument_p[SkArg_%d] = %s;\r\n"), ++param_index, *generate_var_to_instance_expression(param_p, argument));
      }
    generated_code += FString::Printf(TEXT("        static_assert(sizeof(event_p->m_argument_p) >= %d * sizeof(event_p->m_argument_p[0]), \"Event arguments must fit in the array!\");\r\n"), param_index);
    }
  generated_code += FString::Printf(TEXT("        push_event_and_resume(event_p, %d);\r\n"), param_index);
  generated_code += TEXT(
    "        }\r\n");

  // The "Thunk" (glue code between Blueprint scripting engine and the callback function)
  generated_code += FString::Printf(TEXT("      DECLARE_FUNCTION(exec%s)\r\n"), *binding.m_property_p->GetName());
  generated_code += TEXT("        {\r\n");
  FString param_list; // For invoking the actual callback function
  separator_p = TEXT("");
  for (TFieldIterator<UProperty> param_it(binding.m_property_p->SignatureFunction); param_it; ++param_it)
    {
    UProperty * param_p = *param_it;
    FString eval_base_text = TEXT("P_GET_");
    FString type_text;
    if (param_p->ArrayDim > 1)
      {
      eval_base_text += TEXT("ARRAY");
      type_text = param_p->GetCPPType();
      }
    else
      {
      eval_base_text += param_p->GetCPPMacroType(type_text);
      }
    if (!type_text.IsEmpty()) type_text += TEXT(", ");
    FString param_name = TEXT("Z_Param_") + param_p->GetName();
    FString eval_parameter_text = FString::Printf(TEXT("(%s%s)"), *type_text, *param_name);
    generated_code += FString::Printf(TEXT("        %s%s;\r\n"), *eval_base_text, *eval_parameter_text);
    UEnum * enum_p = get_enum(param_p);
    if (enum_p)
      {
      if (enum_p->GetCppForm() == UEnum::ECppForm::EnumClass)
        {
        param_name = FString::Printf(TEXT("(%s&)(%s)"), *enum_p->CppType, *param_name);
        }
      else
        {
        param_name = FString::Printf(TEXT("(TEnumAsByte<%s>&)(%s)"), *enum_p->CppType, *param_name);
        }
      }
    param_list += separator_p + param_name;
    separator_p = TEXT(", ");
    }
  generated_code += TEXT("        P_FINISH;\r\n");
  generated_code += FString::Printf(TEXT("        this->%s(%s);\r\n"), *binding.m_property_p->GetName(), *param_list);
  generated_code += TEXT("        }\r\n");

  // The install callback
  generated_code += FString::Printf(TEXT(
    "      static void install(UObject * obj_p, USkookumScriptListener * listener_p)\r\n"
    "        {\r\n"
    "        add_dynamic_function(ms_name, obj_p->GetClass(), (Native)&USkookumScriptListener_%s::exec%s);\r\n"
    "        static_cast<%s *>(obj_p)->%s.AddDynamic(static_cast<USkookumScriptListener_%s *>(listener_p), &USkookumScriptListener_%s::%s);\r\n"
    "        }\r\n"),
    *binding.m_property_p->GetName(),
    *binding.m_property_p->GetName(),
    *get_cpp_class_name(binding.m_property_p->GetOwnerClass()),
    *binding.m_property_p->GetName(),
    *binding.m_property_p->GetName(),
    *binding.m_property_p->GetName(),
    *binding.m_property_p->GetName());
  
  // The remove callback
  generated_code += FString::Printf(TEXT(
    "      static void uninstall(UObject * obj_p, USkookumScriptListener * listener_p)\r\n"
    "        {\r\n"
    "        static_cast<%s *>(obj_p)->%s.RemoveDynamic(static_cast<USkookumScriptListener_%s *>(listener_p), &USkookumScriptListener_%s::%s);\r\n"
    "        }\r\n"),
    *get_cpp_class_name(binding.m_property_p->GetOwnerClass()), 
    *binding.m_property_p->GetName(), 
    *binding.m_property_p->GetName(), 
    *binding.m_property_p->GetName(), 
    *binding.m_property_p->GetName());

  // The name
  generated_code += TEXT("    static FName ms_name;\r\n");
  generated_code += TEXT("    };\r\n");
  generated_code += FString::Printf(TEXT("  FName USkookumScriptListener_%s::ms_name(TEXT(\"%s\"));\r\n\r\n"), *binding.m_property_p->GetName(), *binding.m_property_p->GetName());

  // 2) Create coroutine implementations
  FString install_args = FString::Printf(TEXT("&USkookumScriptListener_%s::install, &USkookumScriptListener_%s::uninstall"), *binding.m_property_p->GetName(), *binding.m_property_p->GetName());
  for (int32 i = 0; i < EventCoro__count; ++i)
    {
    generated_code += FString::Printf(TEXT(
      "  static bool coro%s(SkInvokedCoroutine * scope_p)\r\n"
      "    {\r\n"
      "    return USkookumScriptListener::%s;\r\n"
      "    }\r\n"), 
      *FString::Printf(ms_event_coro_fmts_pp[i], *binding.m_script_name_base),
      *FString::Printf(ms_event_coro_impl_fmts_pp[i], *install_args)
      );
    }

  generated_code += TEXT("\r\n");

  return generated_code;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_routine_script_parameters(UFunction * function_p, int32 indent_spaces, FString * out_return_type_name_p, int32 * out_num_inputs_p)
  {
  if (out_return_type_name_p) out_return_type_name_p->Empty();
  if (out_num_inputs_p) *out_num_inputs_p = 0;

  FString parameter_body;
  FString separator_indent = TEXT("\n") + FString::ChrN(indent_spaces, ' ');

  bool has_params_or_return_value = (function_p->Children != NULL);
  if (has_params_or_return_value)
    {
    // Figure out column width of variable types & names
    int32 max_type_length = 0;
    int32 max_name_length = 0;
    int32 inputs_count = 0;
    for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it)
      {
      UProperty * param_p = *param_it;
      if (!(param_p->GetPropertyFlags() & CPF_ReturnParm))
        {
        FString type_name = get_skookum_property_type_name(param_p);
        FString var_name = skookify_var_name(param_p->GetName(), param_p->IsA(UBoolProperty::StaticClass()), VarScope_local);
        max_type_length = FMath::Max(max_type_length, type_name.Len());
        max_name_length = FMath::Max(max_name_length, var_name.Len());
        ++inputs_count;
        }
      }
    if (out_num_inputs_p) *out_num_inputs_p = inputs_count;

    // Format nicely
    FString separator;
    for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it)
      {
      UProperty * param_p = *param_it;
      if (param_p->GetPropertyFlags() & CPF_ReturnParm)
        {
        if (out_return_type_name_p) *out_return_type_name_p = get_skookum_property_type_name(param_p);
        }
      else
        {
        FString type_name = get_skookum_property_type_name(param_p);
        FString var_name = skookify_var_name(param_p->GetName(), param_p->IsA(UBoolProperty::StaticClass()), VarScope_local);
        FString default_initializer = get_skookum_default_initializer(function_p, param_p);
        parameter_body += separator + type_name.RightPad(max_type_length) + TEXT(" ");
        if (default_initializer.IsEmpty())
          {
          parameter_body += var_name;
          }
        else
          {
          parameter_body += var_name.RightPad(max_name_length) + get_skookum_default_initializer(function_p, param_p);
          }
        }
      separator = separator_indent;
      }
    }

  return parameter_body;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_binding_declaration(const FString & function_name, bool is_static)
  {
  return FString::Printf(TEXT("static void mthd%s_%s(SkInvokedMethod * scope_p, SkInstance ** result_pp)"), is_static ? TEXT("c") : TEXT(""), *function_name);
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_this_pointer_initialization(const FString & class_name_cpp, UStruct * struct_or_class_p, bool is_static)
  {
  FString class_name_skookum = get_skookum_class_name(struct_or_class_p);
  if (is_static)
    {
    return FString::Printf(TEXT("%s * this_p = GetMutableDefault<%s>(SkUE%s::ms_uclass_p);"), *class_name_cpp, *class_name_cpp, *class_name_skookum);
    }
  else
    {
    bool is_class = !!Cast<UClass>(struct_or_class_p);
    return FString::Printf(TEXT("%s * this_p = %sscope_p->this_as<SkUE%s>();"), *class_name_cpp, is_class ? TEXT("") : TEXT("&"), *class_name_skookum);
    }
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_method_out_parameter_expression(UFunction * function_p, UProperty * param_p, int32 param_index, const FString & param_name)
  {
  FString fmt;

  eSkTypeID type_id = get_skookum_property_type(param_p, true);
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
    case SkTypeID_UObject:
    case SkTypeID_UObjectWeakPtr:  fmt = FString::Printf(TEXT("scope_p->get_arg<%s>(SkArg_%%d) = %%s"), *get_skookum_property_binding_class_name(param_p)); break;
    case SkTypeID_String:          fmt = TEXT("scope_p->get_arg<SkString>(SkArg_%d) = AString(*%s, %s.Len())"); break; // $revisit MBreyer - Avoid copy here
    case SkTypeID_List:
      {
      const UArrayProperty * array_property_p = Cast<UArrayProperty>(param_p);
      UProperty * element_property_p = array_property_p->Inner;
      fmt = FString::Printf(TEXT("SkUEClassBindingHelper::initialize_list_from_array<%s,%s>(&scope_p->get_arg<SkList>(SkArg_%%d), %%s)"),
        *get_skookum_property_binding_class_name(element_property_p),
        *get_cpp_property_type_name(element_property_p, true));
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

    eSkTypeID type_id = get_skookum_property_type(param_p, true);
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
      case SkTypeID_UObject:
      case SkTypeID_UObjectWeakPtr: generated_code = FString::Printf(TEXT("%s = scope_p->get_arg<%s>(SkArg_%d);"), *assignee_name, *get_skookum_property_binding_class_name(param_p), param_index + 1); break;
      case SkTypeID_Integer:        generated_code = FString::Printf(TEXT("%s = (%s)scope_p->get_arg<%s>(SkArg_%d);"), *assignee_name, *get_cpp_property_cast_name(param_p), *get_skookum_property_binding_class_name(param_p), param_index + 1); break;
      case SkTypeID_Enum:           generated_code = FString::Printf(TEXT("%s = (%s)scope_p->get_arg<SkEnum>(SkArg_%d);"), *assignee_name, *get_cpp_property_cast_name(param_p), param_index + 1); break;
      case SkTypeID_String:         generated_code = FString::Printf(TEXT("%s = scope_p->get_arg<SkString>(SkArg_%d).as_cstr();"), *assignee_name, param_index + 1); break;
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
          *get_cpp_property_type_name(element_property_p, true),
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
  eSkTypeID type_id = get_skookum_property_type(param_p, true);
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
    case SkTypeID_UObjectWeakPtr:  return FString::Printf(TEXT("%s()"), *get_cpp_property_type_name(param_p));
    default:                       FError::Throwf(TEXT("Unsupported property type: %s"), *param_p->GetClass()->GetName()); return TEXT("");
    }
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_return_value_passing(UProperty * return_value_p, const FString & return_value_name)
  {
  if (return_value_p)
    {
    return FString::Printf(TEXT("if (result_pp) *result_pp = %s;"), *generate_var_to_instance_expression(return_value_p, return_value_name));
    }
  else
    {
    return TEXT(""); // TEXT("if (result_pp) *result_pp = SkBrain::ms_nil_p;");
    }
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::generate_var_to_instance_expression(UProperty * var_p, const FString & var_name)
  {
  FString fmt;

  eSkTypeID type_id = get_skookum_property_type(var_p, true);
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
    case SkTypeID_UObject:
    case SkTypeID_UObjectWeakPtr:  fmt = FString::Printf(TEXT("%s::new_instance(%%s)"), *get_skookum_property_binding_class_name(var_p)); break;
    case SkTypeID_String:          fmt = TEXT("SkString::new_instance(AString(*(%s), %s.Len()))"); break; // $revisit MBreyer - Avoid copy here
    case SkTypeID_List:
    {
    const UArrayProperty * array_property_p = Cast<UArrayProperty>(var_p);
    UProperty * element_property_p = array_property_p->Inner;
    fmt = FString::Printf(TEXT("SkUEClassBindingHelper::list_from_array<%s,%s>(%%s)"),
      *get_skookum_property_binding_class_name(element_property_p),
      *get_cpp_property_type_name(element_property_p, true));
    }
    break;
    default:  FError::Throwf(TEXT("Unsupported return param type: %s"), *var_p->GetClass()->GetName()); break;
    }

  return FString::Printf(*fmt, *var_name, *var_name);
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::save_generated_script_files(eClassScope class_scope)
  {
  // Keep track of parent classes when generating game script files
  m_types_to_generate.Empty();
  m_types_to_generate_lookup.Empty();

  // Bail if nothing to do
  if (m_targets[class_scope].m_root_directory_path.IsEmpty())
    {
    return false;
    }

  // Set up output path for scripts
  if (class_scope == ClassScope_project)
    {
    FString project_file_path = get_or_create_project_file(m_targets[ClassScope_project].m_root_directory_path, *m_targets[ClassScope_project].m_project_name);
    if (project_file_path.IsEmpty())
      {
      return false;
      }

    // If project path exists, overrides the default script location
    m_overlay_path = FPaths::GetPath(project_file_path) / TEXT("Project-Generated-C++");
    compute_scripts_path_depth(project_file_path, TEXT("Project-Generated-C++"));
    }
  else
    {
    check(class_scope == ClassScope_engine);
    m_overlay_path = m_targets[ClassScope_engine].m_root_directory_path / TEXT("Scripts/Engine-Generated");
    compute_scripts_path_depth(m_targets[ClassScope_engine].m_root_directory_path / TEXT("Scripts/Skookum-project-default.ini"), TEXT("Engine-Generated"));
    }

  // Clear contents of scripts folder for a fresh start
  FString directory_to_delete(m_overlay_path / TEXT("Object"));
  IFileManager::Get().DeleteDirectory(*directory_to_delete, false, true);

  // Create single packed file or folder structure of loose files?
  if (m_overlay_path_depth == PathDepth_archived)
    {
    // Packed file, generate it

    // Output in correct order
    struct GenerateEntry
      {      
      const GeneratedType * m_generated_type_p;
      FString               m_type_name;
      FString               m_parent_name;
      int                   m_sort_index;
      bool                  m_is_referenced;

      bool operator < (const GenerateEntry & other) const { return m_sort_index > other.m_sort_index; }
      };

    // List of classes to generate
    TArray<GenerateEntry> generate_list;
    generate_list.Reserve(m_types_generated.Num());

    // Fast lookup table from name to list entry
    TMap<FString, int> generate_list_lookup; // Lookup types to generate by name
    generate_list_lookup.Reserve(m_types_generated.Num());

    // Build lookup table and create entry for each type
    for (const GeneratedType & generated_type : m_types_generated)
      {
      generate_list_lookup.Add(generated_type.m_sk_name, generate_list.Num());
      generate_list.Add({ &generated_type, generated_type.m_sk_name, FString(), 0, false });
      }
    // Build complete list
    int type_index = 0;
    for (const GeneratedType & generated_type : m_types_generated)
      {
      // Determine parent and its name
      UStruct * parent_p = nullptr;
      FString parent_name = get_skookum_parent_name(generated_type.m_type_p, 0, 0, &parent_p);

      // Update entry
      GenerateEntry & entry = generate_list[type_index++];
      entry.m_parent_name = parent_name;
      bool is_used_in_this_pass = (entry.m_generated_type_p->m_class_scope == class_scope);
      entry.m_is_referenced = entry.m_is_referenced || is_used_in_this_pass;

      // Crawl up hierarchy to make sure all parents are present
      int sort_index = entry.m_sort_index;
      while (parent_p)
        {
        ++sort_index;
        // Does entry for this parent already exist?
        int * parent_index_p = generate_list_lookup.Find(parent_name);
        if (parent_index_p)
          {
          // Yes, update the sort index and keep crawling
          GenerateEntry & parent_entry = generate_list[*parent_index_p];
          parent_entry.m_sort_index = FMath::Max(parent_entry.m_sort_index, sort_index);
          parent_entry.m_is_referenced = parent_entry.m_is_referenced || is_used_in_this_pass;
          parent_name = get_skookum_parent_name(parent_entry.m_generated_type_p->m_type_p, 0, 0, &parent_p);
          }
        else
          {
          // No, create entry with current sort index and keep crawling
          generate_list_lookup.Add(parent_name, generate_list.Num());
          FString type_name = parent_name;
          parent_name = get_skookum_parent_name(parent_p, 0, 0, &parent_p);
          generate_list.Add({ nullptr, type_name, parent_name, sort_index, is_used_in_this_pass });
          }
        }
      }
    generate_list.Add({ nullptr, TEXT("Enum"), TEXT("Object"), INT_MAX, true });
    generate_list.Add({ nullptr, TEXT("UStruct"), TEXT("Object"), INT_MAX, true });
    // Sort list
    generate_list.Sort();

    FString script;

    // Loop through generated scripts and write them to disk
    for (GenerateEntry & entry : generate_list)
      {
      if (entry.m_is_referenced)
        {
        script += FString::Printf(TEXT("$$ %s < %s\n"), *entry.m_type_name, *entry.m_parent_name);

        const GeneratedType * generated_type_p = entry.m_generated_type_p;
        if (generated_type_p && generated_type_p->m_class_scope == class_scope)
          {
          script += generated_type_p->m_sk_meta_file_body + TEXT("\n");

          // Write instance data if any
          if (!generated_type_p->m_sk_instance_data_file_body.IsEmpty())
            {
            script += TEXT("$$ @\n");
            script += generated_type_p->m_sk_instance_data_file_body + TEXT("\n");
            }

          // Write class data if any
          if (!generated_type_p->m_sk_class_data_file_body.IsEmpty())
            {
            script += TEXT("$$ @@\n");
            script += generated_type_p->m_sk_class_data_file_body + TEXT("\n");
            }

          // Write method definitions
          for (tSkRoutines::TConstIterator iter(generated_type_p->m_sk_routines); iter; ++iter)
            {
            script += FString::Printf(TEXT("$$ %s%s\n"), iter->m_is_class_member ? TEXT("@@") : TEXT("@"), *iter->m_name);
            script += iter->m_body + TEXT("\n");
            }
          }
        else if (!generated_type_p)
          {
          // If no GeneratedType present, just add a comment with the class name as the meta file chunk
          script += FString::Printf(TEXT("// %s\n"), *entry.m_type_name);
          }
        }
      }

    // Append terminator
    script += "$$ .\n";

    // And write it out
    save_text_file(m_overlay_path / TEXT("!Overlay.sk"), script);
    }
  else
    {
    // Loose files, generate them

    // Create class "Enum" and "UStruct" as these folders will not get automagically created when m_overlay_path_depth <= 1
    generate_class_meta_file(nullptr, m_overlay_path / TEXT("Object") / TEXT("Enum"), TEXT("Enum"));
    generate_class_meta_file(nullptr, m_overlay_path / TEXT("Object") / TEXT("UStruct"), TEXT("UStruct"));

    // Loop through generated scripts and write them to disk
    for (const GeneratedType & generated_type : m_types_generated)
      {
      if (generated_type.m_class_scope == class_scope)
        {
        FString skookum_class_path = get_skookum_class_path(generated_type.m_type_p, 0, 0);

        // Write meta file (even if empty)
        save_text_file(skookum_class_path / TEXT("!Class.sk-meta"), generated_type.m_sk_meta_file_body);

        // Write instance data if any
        if (!generated_type.m_sk_instance_data_file_body.IsEmpty())
          {
          save_text_file(skookum_class_path / TEXT("!Data.sk"), generated_type.m_sk_instance_data_file_body);
          }

        // Write class data if any
        if (!generated_type.m_sk_class_data_file_body.IsEmpty())
          {
          save_text_file(skookum_class_path / TEXT("!DataC.sk"), generated_type.m_sk_class_data_file_body);
          }

        // Write method definitions
        for (tSkRoutines::TConstIterator iter(generated_type.m_sk_routines); iter; ++iter)
          {
          save_text_file(skookum_class_path / get_skookum_method_file_name(iter->m_name, iter->m_is_class_member), iter->m_body);
          }
        }
      }

    // In game mode, make sure parent classes that come from the Engine-Generated overlay exist as well
    if (class_scope == ClassScope_project)
      {
      // Loop through all parent classes we encountered while generating the class paths in the above loop
      for (const TypeToGenerate & type_to_generate : m_types_to_generate)
        {
        const GeneratedType * generated_type_p = m_types_generated.FindByKey(type_to_generate.m_type_p);
        if (generated_type_p && generated_type_p->m_class_scope == ClassScope_engine)
          {
          // Write just the meta file to ensure class exists
          save_text_file(get_skookum_class_path(type_to_generate.m_type_p, type_to_generate.m_include_priority, type_to_generate.m_referenced_flags) / TEXT("!Class.sk-meta"), generated_type_p->m_sk_meta_file_body);
          }
        }
      }
    }

  return true;
  }

//---------------------------------------------------------------------------------------
// Write two files, a .hpp and an .inl file that define SkUEEngineGeneratedBindings / SkUEProjectGeneratedBindings
void FSkookumScriptGenerator::save_generated_cpp_files(eClassScope class_scope)
  {
  const TCHAR * engine_project_p = class_scope == ClassScope_engine ? TEXT("Engine") : TEXT("Project");

  //---------------------------------------------------------------------------------------
  // 1) Determine output path

  FString binding_code_directory_path;
  if (class_scope == ClassScope_project)
    {
    // Find module in which to place the generated files (first module in the list of ScriptSupportedModules that is a game module)
    for (const ModuleInfo & module : m_targets[ClassScope_project].m_script_supported_modules)
      {
      if (module.m_scope == ClassScope_project)
        {
        // Found it, use its generated_include_folder path
        binding_code_directory_path = module.m_generated_include_folder;
        break;
        }
      }
    }
  else
    {
    binding_code_directory_path = m_binding_code_path_engine;
    }

  // Got a path?
  if (binding_code_directory_path.IsEmpty())
    {
    // No path = nothing to do
    return;
    }

  //---------------------------------------------------------------------------------------
  // 2) SkUEBindingsInterface header

  FString header_code = TEXT(
    "#pragma once\r\n\r\n"
    "#include \"SkUEBindingsInterface.hpp\"\r\n"
    "\r\n");

  // Declaration of the SkUEGeneratedBindings class
  header_code += FString::Printf(TEXT(
    "// Initializes generated bindings for the engine\r\n"
    "class SkUE%sGeneratedBindings : public SkUEBindingsInterface\r\n"
    "  {\r\n"
    "  public:\r\n"
    "\r\n"
    "    virtual void register_static_ue_types() override;\r\n"
    "    virtual void register_static_sk_types() override;\r\n"
    "    virtual void register_bindings() override;\r\n"
    "\r\n"
    "  };\r\n\r\n"), engine_project_p);

  // Save to disk
  save_text_file_if_changed(binding_code_directory_path / FString::Printf(TEXT("SkUE%sGeneratedBindings.generated.hpp"), engine_project_p), header_code);

  //---------------------------------------------------------------------------------------
  // 3) Individual header files for each of the binding classes

  for (const GeneratedType & generated_type : m_types_generated)
    {
    if (class_scope == ClassScope_project || generated_type.m_class_scope == ClassScope_engine)
      {
      FString class_header_code = TEXT(
        "#pragma once\r\n\r\n"
        "#include \"SkUEClassBinding.hpp\"\r\n");

      if (Cast<UEnum>(generated_type.m_type_p) != nullptr)
        {
        class_header_code += TEXT("#include \"SkookumScript/SkEnum.hpp\"\r\n");
        }

      FString include_file_path = generated_type.m_type_p->GetMetaData(ms_meta_data_key_module_relative_path);
      if (!include_file_path.IsEmpty())
        {
        class_header_code += FString::Printf(TEXT("#include \"%s\"\r\n"), *include_file_path);
        }

      class_header_code += generated_type.m_cpp_header_file_body;

      save_text_file_if_changed(FString::Printf(TEXT("%s/SkUE%s.generated.hpp"), *binding_code_directory_path, *generated_type.m_sk_name), class_header_code);
      }
    }

  //---------------------------------------------------------------------------------------
  // 4) Binding code (inl file)

  // Include some required headers
  FString binding_code = FString::Printf(TEXT(
    "\r\n"
    "#include \"SkUE%sGeneratedBindings.generated.hpp\"\r\n"
    "\r\n"
    "#include \"SkookumScript/SkClass.hpp\"\r\n"
    "#include \"SkookumScript/SkBrain.hpp\"\r\n"
    "#include \"SkookumScript/SkInvokedMethod.hpp\"\r\n"
    "#include \"SkookumScript/SkInteger.hpp\"\r\n"
    "#include \"SkookumScript/SkEnum.hpp\"\r\n"
    "#include \"SkookumScript/SkReal.hpp\"\r\n"
    "#include \"SkookumScript/SkBoolean.hpp\"\r\n"
    "#include \"SkookumScript/SkString.hpp\"\r\n"
    "#include \"SkookumScript/SkList.hpp\"\r\n"
    "\r\n"
    "#include \"VectorMath/SkVector2.hpp\"\r\n"
    "#include \"VectorMath/SkVector3.hpp\"\r\n"
    "#include \"VectorMath/SkVector4.hpp\"\r\n"
    "#include \"VectorMath/SkRotation.hpp\"\r\n"
    "#include \"VectorMath/SkRotationAngles.hpp\"\r\n"
    "#include \"VectorMath/SkTransform.hpp\"\r\n"
    "#include \"VectorMath/SkColor.hpp\"\r\n"
    "\r\n"
    "#include \"Engine/SkUEName.hpp\"\r\n"
    "\r\n"
    "#include \"ISkookumScriptRuntime.h\"\r\n"
    "#include \"SkookumScriptListener.h\"\r\n"
    "\r\n"), engine_project_p);

  // Add extra include files if specified
  TSet<FString> included_files;
  included_files.Reserve(m_types_to_generate.Num());
  for (const FString & include_file_path : m_targets[class_scope].m_additional_includes)
    {
    binding_code += FString::Printf(TEXT("#include \"%s\"\r\n"), *include_file_path);
    included_files.Add(include_file_path);
    }

  // Include all UE4 headers referenced
  // Sort descending by include priority
  m_types_to_generate.Sort();
  m_types_to_generate_lookup.Empty();
  for (const TypeToGenerate & type_to_generate : m_types_to_generate)
    {
    // Get include path so we can make not try to include anything multiple times
    FString include_file_path = type_to_generate.m_type_p->GetMetaData(ms_meta_data_key_module_relative_path);
    bool dont_include = include_file_path.IsEmpty() || included_files.Contains(include_file_path);

    // Does it have an associated generated type?
    if (type_to_generate.m_generated_type_index >= 0)
      {
      // Yes, include the corresponding generated binding class declaration header - if scope is proper
      // Always include even if `dont_include` is true, since we need the binding class declaration
      const GeneratedType & generated_type = m_types_generated[type_to_generate.m_generated_type_index];
      if (generated_type.m_class_scope == class_scope || (class_scope == ClassScope_project && (type_to_generate.m_referenced_flags & Referenced_by_game_module)))
        {
        if (generated_type.m_class_scope == class_scope || (type_to_generate.m_referenced_flags & Referenced_as_binding_class))
          {
          binding_code += FString::Printf(TEXT("#include \"SkUE%s.generated.hpp\"\r\n"), *generated_type.m_sk_name);
          }
        else if (!dont_include)
          {
          binding_code += FString::Printf(TEXT("#include \"%s\"\r\n"), *include_file_path);
          }
        }
      }
    else
      {
      // No, include the UE4 header just to be sure
      if (!dont_include)
        {
        //FPaths::MakePathRelativeTo(include_file_path, *(m_unreal_engine_root_path_local / TEXT("Engine/Source/")));
        binding_code += FString::Printf(TEXT("#include \"%s\"\r\n"), *include_file_path);
        }
      }

    if (!dont_include)
      {
      included_files.Add(include_file_path);
      }
    }

  binding_code += TEXT("\r\n");

  // Disable pesky deprecation warnings
  binding_code += TEXT(
    "// Generated code will use some deprecated functions - that's ok don't tell me about it\r\n"
    "PRAGMA_DISABLE_DEPRECATION_WARNINGS\r\n\r\n");

  // Generate initialization code for project module
  if (class_scope == ClassScope_project)
    {
    binding_code += FString::Printf(TEXT(
      "static TCHAR const * const _sk_module_name_p = TEXT(\"SkookumScriptRuntime\");\r\n"
      "static TCHAR const * const _game_module_name_p = TEXT(\"/Script/%s\");\r\n"
      "static TCHAR const * const _dummy_cpp_class_name_p = TEXT(\"USkookumScriptDummyObject\");\r\n"
      "\r\n"
      "static struct InitializationHelper : FFieldCompiledInInfo\r\n"
      "  {\r\n"
      "  // Step 1: This gets invoked during module static initialization to add this object to the class initialization queue\r\n"
      "  InitializationHelper() : FFieldCompiledInInfo(0, 0)\r\n"
      "    {\r\n"
      "    UClassCompiledInDefer(this, _dummy_cpp_class_name_p, 0, 0);\r\n"
      "    }\r\n"
      "\r\n"
      "  // Step 2: This gets invoked when UCLASSES are registered, which is before UENUMS or USTRUCTS are registered\r\n"
      "  virtual UClass * Register() const override\r\n"
      "    {\r\n"
      "    #if IS_MONOLITHIC\r\n"
      "	     // Queue up another callback which will fire right after the SkookumScriptRuntime modules is initialized\r\n"
      "      m_handle = FModuleManager::Get().OnModulesChanged().AddRaw(this, &InitializationHelper::on_modules_changed);\r\n"
      "    #else\r\n"
      "      // The struct initialization queue has been filled by now (since it is filled during module static initialization)\r\n"
      "      // Queue up another callback which will end up at the very end of the struct initialization queue\r\n"
      "      static FCompiledInDeferStruct OnAllModuleTypesRegistered(on_all_module_types_registered, _game_module_name_p, TEXT(\"OnAllModuleTypesRegistered\"), false, nullptr, nullptr);\r\n"
      "    #endif\r\n"
      "    // Return a dummy class\r\n"
      "    UClass * dummy_class_p = FindObject<UClass>(ANY_PACKAGE, &_dummy_cpp_class_name_p[1]);\r\n"
      "    if (dummy_class_p) return dummy_class_p;\r\n"
      "    GetPrivateStaticClassBody(\r\n"
      "      _game_module_name_p,\r\n"
      "      &_dummy_cpp_class_name_p[1],\r\n"
      "      dummy_class_p,\r\n"
      "      &UObject::StaticRegisterNativesUObject,\r\n"
      "      sizeof(UObject),\r\n"
      "      CLASS_Abstract|CLASS_NoExport,\r\n"
      "      CASTCLASS_None,\r\n"
      "      TEXT(\"Engine\"),\r\n"
      "      &InternalConstructor<UObject>,\r\n"
      "      &InternalVTableHelperCtorCaller<UObject>,\r\n"
      "      &UObject::AddReferencedObjects,\r\n"
      "      &UObject::StaticClass,\r\n"
      "      &UPackage::StaticClass);\r\n"
      "    return dummy_class_p;\r\n"
      "    }\r\n"
      "\r\n"
      "  // Step 3: This gets invoked after all other USTRUCTS in this module have been registered\r\n"
      "  // And as USTRUCTS get registered after UENUMS and UCLASSES, this means all UCLASSES, UENUMS and USTRUCTS have been registered at this point\r\n"
      "  static class UScriptStruct * on_all_module_types_registered()\r\n"
      "    {\r\n"
      "    ISkookumScriptRuntime * sk_module_p = FModuleManager::Get().GetModulePtr<ISkookumScriptRuntime>(_sk_module_name_p);\r\n"
      "    if (sk_module_p)\r\n"
      "      {\r\n"
      "      static SkUEProjectGeneratedBindings bindings;\r\n"
      "      sk_module_p->set_project_generated_bindings(&bindings);\r\n"
      "      }\r\n"
      "    // The registration process does not actually care what we return so return nullptr\r\n"
      "    return nullptr;\r\n"
      "    }\r\n"
      "\r\n"
      "#if IS_MONOLITHIC\r\n"
      "  void on_modules_changed(FName module_name, EModuleChangeReason change_reason)\r\n"
      "    {\r\n"
      "    static FName sk_module_name(_sk_module_name_p);\r\n"
      "    if (module_name == sk_module_name && change_reason == EModuleChangeReason::ModuleLoaded)\r\n"
      "      {\r\n"
      "      on_all_module_types_registered();\r\n"
      "      FModuleManager::Get().OnModulesChanged().Remove(m_handle);\r\n"
      "      }\r\n"
      "    }\r\n"
      "#endif\r\n"
      "\r\n"
      "  // Step 4: When this module goes away, unregister us with the Sk module\r\n"
      "  ~InitializationHelper()\r\n"
      "    {\r\n"
      "    ISkookumScriptRuntime * sk_module_p = FModuleManager::Get().GetModulePtr<ISkookumScriptRuntime>(_sk_module_name_p);\r\n"
      "    if (sk_module_p)\r\n"
      "      {\r\n"
      "      sk_module_p->set_project_generated_bindings(nullptr);\r\n"
      "      }\r\n"
      "    }\r\n"
      "\r\n"
      "  virtual const TCHAR * ClassPackage() const { return _game_module_name_p; }\r\n"
      "\r\n"
      "  mutable FDelegateHandle m_handle;\r\n"
      "\r\n"
      "  } _initialization_helper;\r\n\r\n"), 
      *m_targets[ClassScope_project].m_script_supported_modules.FindByKey(ClassScope_project)->m_name);
    }

  // Insert generate binding code of all generated classes
  // And count types while we are at it
  int32 num_generated_classes = 0;
  int32 num_generated_structs = 0;
  int32 num_generated_enums = 0;
  for (const GeneratedType & generated_type : m_types_generated)
    {
    if (generated_type.m_class_scope == class_scope)
      {
      binding_code += generated_type.m_cpp_binding_file_body;

      // Count types
      if (Cast<UClass>(generated_type.m_type_p))
        {
        ++num_generated_classes;
        }
      else if (Cast<UEnum>(generated_type.m_type_p))
        {
        ++num_generated_enums;
        }
      else
        {
        ++num_generated_structs;
        }
      }
    }

  binding_code += TEXT("\r\n");

  // Generate definition of register_static_ue_types()
  binding_code += FString::Printf(TEXT("void SkUE%sGeneratedBindings::register_static_ue_types()\r\n  {\r\n"), engine_project_p);
  if (class_scope == ClassScope_engine)
    {
    binding_code += FString::Printf(TEXT("  SkUEClassBindingHelper::reset_static_class_mappings(%d);\r\n"), num_generated_classes);
    binding_code += FString::Printf(TEXT("  SkUEClassBindingHelper::reset_static_struct_mappings(%d);\r\n"), num_generated_structs);
    binding_code += FString::Printf(TEXT("  SkUEClassBindingHelper::reset_static_enum_mappings(%d);\r\n"), num_generated_enums);
    }
  else
    {
    binding_code += FString::Printf(TEXT("  SkUEClassBindingHelper::add_slack_to_static_class_mappings(%d);\r\n"), num_generated_classes);
    binding_code += FString::Printf(TEXT("  SkUEClassBindingHelper::add_slack_to_static_struct_mappings(%d);\r\n"), num_generated_structs);
    binding_code += FString::Printf(TEXT("  SkUEClassBindingHelper::add_slack_to_static_enum_mappings(%d);\r\n"), num_generated_enums);
    }
  for (const GeneratedType & generated_type : m_types_generated)
    {
    if (generated_type.m_class_scope == class_scope)
      {
      binding_code += TEXT("\r\n  ") + generated_type.m_cpp_register_static_ue_type;
      }
    }
  binding_code += TEXT("\r\n  }\r\n\r\n");

  // Generate definition of register_static_sk_types()
  binding_code += FString::Printf(TEXT(
    "void SkUE%sGeneratedBindings::register_static_sk_types()\r\n"
    "  {\r\n"), engine_project_p);
  if (class_scope == ClassScope_engine)
    {
    binding_code += TEXT("  SkUEClassBindingHelper::forget_sk_classes_in_all_mappings();\r\n");
    }
  for (const GeneratedType & generated_type : m_types_generated)
    {
    if (generated_type.m_class_scope == class_scope)
      {
      binding_code += TEXT("\r\n  ") + generated_type.m_cpp_register_static_sk_type;
      }
    }
  binding_code += TEXT("\r\n  }\r\n\r\n");

  // Generate definition of register_bindings() - call register_bindings() on all generated binding classes except enums
  binding_code += FString::Printf(TEXT("void SkUE%sGeneratedBindings::register_bindings()\r\n  {\r\n"), engine_project_p);
  for (const GeneratedType & generated_type : m_types_generated)
    {
    if ((generated_type.m_class_scope == class_scope) && !generated_type.m_is_hierarchy_stub && !Cast<UEnum>(generated_type.m_type_p))
      {
      binding_code += FString::Printf(TEXT("  SkUE%s::register_bindings();\r\n"), *generated_type.m_sk_name);
      }
    }
  binding_code += TEXT("  }\r\n\r\n");

  // Re-enable clang warnings
  binding_code += TEXT("PRAGMA_ENABLE_DEPRECATION_WARNINGS\r\n\r\n");

  // Save to disk
  save_text_file_if_changed(binding_code_directory_path / FString::Printf(TEXT("SkUE%sGeneratedBindings.generated.inl"), engine_project_p), binding_code);
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::can_export_enum(UEnum * enum_p)
  {
  return true;
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::can_export_method(UFunction * function_p, int32 include_priority, uint32 referenced_flags, bool allow_delegate)
  {
  // We don't support delegates and non-public functions
  if (!allow_delegate && (function_p->FunctionFlags & FUNC_Delegate))
    {
    return false;
    }

  // Skip custom thunks
  if (function_p->GetBoolMetaData(ms_meta_data_key_custom_thunk))
    {
    return false;
    }

  // Reject if any of the parameter types is unsupported yet
  for (TFieldIterator<UProperty> param_it(function_p); param_it; ++param_it)
    {
    UProperty * param_p = *param_it;

    if (param_p->IsA(UDelegateProperty::StaticClass()) ||
        param_p->IsA(UMulticastDelegateProperty::StaticClass()) ||
        param_p->IsA(UInterfaceProperty::StaticClass()))
      {
      return false;
      }

    if (!is_property_type_supported(param_p))
      {
      return false;
      }

    // Property is supported - use the opportunity to make it known to SkookumScript
    on_property_referenced(param_p, include_priority, referenced_flags);
    }

  return true;
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::can_export_property(UProperty * property_p, int32 include_priority, uint32 referenced_flags)
  {
  // Check if property type is supported
  if (!is_property_type_supported(property_p))
    {
    return false;
    }

  // Exclude any structs with types to skip
  if (UStructProperty * struct_prop_p = Cast<UStructProperty>(property_p))
    {
    FName struct_name = struct_prop_p->Struct->GetFName();
    if (m_targets[ClassScope_engine].m_skip_classes.Contains(struct_name)
     || m_targets[ClassScope_project].m_skip_classes.Contains(struct_name))
      {
      return false;
      }
    }

  // Exclude any arrays of unsupported types as well
  if (UArrayProperty * array_property_p = Cast<UArrayProperty>(property_p))
    {
    if (!can_export_property(array_property_p->Inner, include_priority + 1, referenced_flags))
      {
      return false;
      }
    }

  // Property is supported - use the opportunity to make it known to SkookumScript
  on_property_referenced(property_p, include_priority, referenced_flags);

  return true;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::on_type_referenced(UField * type_p, int32 include_priority, uint32 referenced_flags)
  {
  request_generate_type(type_p, include_priority, referenced_flags);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::report_error(const FString & message)
  {
  GWarn->Log(ELogVerbosity::Error, message);
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::get_skookum_property_binding_class_name(UProperty * property_p)
  {
  eSkTypeID type_id = get_skookum_property_type(property_p, true);

  FString prefix = TEXT("Sk");
  if (type_id == SkTypeID_Name
    || type_id == SkTypeID_Enum
    || type_id == SkTypeID_UStruct
    || type_id == SkTypeID_UClass
    || type_id == SkTypeID_UObject
    || type_id == SkTypeID_UObjectWeakPtr)
    {
    prefix = TEXT("SkUE");
    }

  return prefix + get_skookum_property_type_name(property_p);
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::get_cpp_class_name(UStruct * struct_or_class_p)
  {
  return FString::Printf(TEXT("%s%s"), struct_or_class_p->GetPrefixCPP(), *struct_or_class_p->GetName());
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::get_cpp_property_type_name(UProperty * property_p, bool is_array_element, bool with_const, bool with_ref)
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
  UEnum * enum_p = get_enum(property_p);
  if (enum_p && (is_array_element || property_p->GetName() == TEXT("PathEvent"))) // HACK MJB I see no proper way to detect this from the UProperty
    {
    property_type_name = TEXT("TEnumAsByte<") + property_type_name + TEXT(">");
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
  else if (property_type_name.StartsWith(decl_TArray))
    {
    const UArrayProperty * array_property_p = Cast<UArrayProperty>(property_p);
    UProperty * element_property_p = array_property_p->Inner;
    property_type_name = FString::Printf(TEXT("TArray<%s>"), *get_cpp_property_type_name(element_property_p, true));
    }

  if (with_const && property_p->HasAnyPropertyFlags(CPF_ConstParm)) property_type_name = TEXT("const ") + property_type_name;
  if (with_ref && property_p->HasAnyPropertyFlags(CPF_ReferenceParm)) property_type_name += TEXT(" &");

  return property_type_name;
  }

//---------------------------------------------------------------------------------------

FString FSkookumScriptGenerator::get_cpp_property_cast_name(UProperty * property_p)
  {
  UEnum * enum_p = get_enum(property_p);
  if (enum_p && enum_p->GetCppForm() == UEnum::ECppForm::Regular)
    {
    return enum_p->GetName();
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
  if (default_value.IsEmpty())
    {
    FName cpp_default_value_key(*(TEXT("CPP_Default_") + param_p->GetName()));
    if (function_p->HasMetaData(cpp_default_value_key))
      {
      has_default_value = true;
      default_value = function_p->GetMetaData(cpp_default_value_key);
      }
    }
  if (has_default_value)
    {
    // Trivial default?
    if (default_value.IsEmpty())
      {
      eSkTypeID type_id = get_skookum_property_type(param_p, true);
      switch (type_id)
        {
        case SkTypeID_Integer:         default_value = TEXT("0"); break;
        case SkTypeID_Real:            default_value = TEXT("0.0"); break;
        case SkTypeID_Boolean:         default_value = TEXT("false"); break;
        case SkTypeID_String:          default_value = TEXT("\"\""); break;
        case SkTypeID_Enum:            default_value = get_enum(param_p)->GetName() + TEXT(".") + skookify_var_name(get_enum(param_p)->GetNameStringByIndex(0), false, VarScope_class); break;
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
        case SkTypeID_UObject:
        case SkTypeID_UObjectWeakPtr:  default_value = (param_p->GetName() == TEXT("WorldContextObject")) ? TEXT("@@world") : get_skookum_class_name(Cast<UObjectPropertyBase>(param_p)->PropertyClass) + TEXT("!null"); break;
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
      eSkTypeID type_id = get_skookum_property_type(param_p, true);
      switch (type_id)
        {
        case SkTypeID_Integer:         break; // Leave as-is
        case SkTypeID_Real:            break; // Leave as-is
        case SkTypeID_Boolean:         default_value = default_value.ToLower(); break;
        case SkTypeID_String:          default_value = TEXT("\"") + default_value + TEXT("\""); break;
        case SkTypeID_Name:            default_value = (default_value == TEXT("None") ? TEXT("Name!none") : TEXT("Name!(\"") + default_value + TEXT("\")")); break;
        case SkTypeID_Enum:            default_value = get_enum(param_p)->GetName() + TEXT(".") + skookify_var_name(default_value, false, VarScope_class); break;
        case SkTypeID_Vector2:         default_value = TEXT("Vector2!xy") + default_value; break;
        case SkTypeID_Vector3:         default_value = TEXT("Vector3!xyz(") + default_value + TEXT(")"); break;
        case SkTypeID_Vector4:         default_value = TEXT("Vector4!xyzw") + default_value; break;
        case SkTypeID_Rotation:        break; // Not implemented yet - leave as-is for now
        case SkTypeID_RotationAngles:  default_value = TEXT("RotationAngles!yaw_pitch_roll(") + default_value + TEXT(")"); break;
        case SkTypeID_Transform:       break; // Not implemented yet - leave as-is for now
        case SkTypeID_Color:           default_value = TEXT("Color!rgba") + default_value; break;
        case SkTypeID_UStruct:         if (default_value == TEXT("LatentInfo")) default_value = get_skookum_class_name(Cast<UStructProperty>(param_p)->Struct) + TEXT("!"); break;
        case SkTypeID_UClass:          default_value = skookify_class_name(default_value) + TEXT(".static_class"); break;
        case SkTypeID_UObject:
        case SkTypeID_UObjectWeakPtr:  if (default_value == TEXT("WorldContext") || default_value == TEXT("WorldContextObject") || param_p->GetName() == TEXT("WorldContextObject")) default_value = TEXT("@@world"); break;
        }
      }

    check(!default_value.IsEmpty()); // Default value must be non-empty at this point

    default_value = TEXT(" : ") + default_value;
    }

  return default_value;
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::on_property_referenced(UProperty * prop_p, int32 include_priority, uint32 referenced_flags)
  {
  eSkTypeID type_id = get_skookum_property_type(prop_p, true);
  if (type_id == SkTypeID_Enum)
    {
    UEnum * enum_p = get_enum(prop_p);
    if (enum_p)
      {
      request_generate_type(enum_p, include_priority + 1, referenced_flags);
      }
    }
  else if (type_id == SkTypeID_UStruct)
    {
    UStruct * struct_p = Cast<UStructProperty>(prop_p)->Struct;
    if (struct_p)
      {
      request_generate_type(struct_p, include_priority + 1, referenced_flags);
      }
    }
  else if (type_id == SkTypeID_UClass)
    {
    request_generate_type(UClass::StaticClass(), include_priority + 1, referenced_flags);
    }
  else if (type_id == SkTypeID_UObject || type_id == SkTypeID_UObjectWeakPtr)
    {
    UClass * class_p = Cast<UObjectPropertyBase>(prop_p)->PropertyClass;
    if (class_p)
      {
      request_generate_type(class_p, include_priority + 1, referenced_flags);
      }
    }
  else if (type_id == SkTypeID_List)
    {
    on_property_referenced(CastChecked<UArrayProperty>(prop_p)->Inner, include_priority + 1, referenced_flags);
    }
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::request_generate_type(UField * type_p, int32 include_priority, uint32 referenced_flags)
  {
  // Add it or consolidate include path
  TypeToGenerate * type_to_generate_p;
  int32 * existing_index_p = m_types_to_generate_lookup.Find(type_p);
  if (existing_index_p)
    {
    type_to_generate_p = &m_types_to_generate[*existing_index_p];
    // Maximize priority
    type_to_generate_p->m_include_priority = FMath::Max(type_to_generate_p->m_include_priority, include_priority);
    type_to_generate_p->m_referenced_flags |= (uint16)referenced_flags;
    }
  else
    {
    int32 new_index = m_types_to_generate.Emplace();
    m_types_to_generate_lookup.Add(type_p, new_index);
    type_to_generate_p = &m_types_to_generate[new_index];
    type_to_generate_p->m_type_p = type_p;
    type_to_generate_p->m_include_priority = include_priority;
    type_to_generate_p->m_referenced_flags = (uint16)referenced_flags;
    type_to_generate_p->m_generated_type_index = -1;
    }

  // Also request/update all parent structs
  UStruct * struct_p = Cast<UStruct>(type_p);
  if (struct_p)
    {
    struct_p = struct_p->GetSuperStruct();
    if (struct_p)
      {
      // Parent has to be included before child, so must have higher priority
      request_generate_type(struct_p, include_priority + 1, referenced_flags & ~Referenced_as_binding_class);
      }
    }
  }

//=======================================================================================
// GenerationTarget implementation
//=======================================================================================

void FSkookumScriptGenerator::GenerationTarget::initialize(const FString & root_directory_path, const FString & project_name, const GenerationTarget * inherit_from_p)
  {
  // Check if root path actually exists
  if (IFileManager::Get().DirectoryExists(*root_directory_path))
    {
    // Yes, only then set it
    m_root_directory_path = root_directory_path;
    m_project_name = project_name;

    TArray<FString> script_supported_modules;
    TArray<FString> skip_classes;

    // Do we have a SkookumScript.ini file?
    FString skookumscript_ini_file_path = root_directory_path / TEXT("Config/SkookumScript.ini");
    bool ini_file_exists = IFileManager::Get().FileExists(*skookumscript_ini_file_path);
    if (ini_file_exists)
      {
      // Load settings from SkookumScript.ini
      FConfigCacheIni skookumscript_ini(EConfigCacheType::Temporary);
      skookumscript_ini.GetArray(TEXT("CommonSettings"), TEXT("+ScriptSupportedModules"), script_supported_modules, skookumscript_ini_file_path);
      skookumscript_ini.GetArray(TEXT("CommonSettings"), TEXT("+SkipClasses"), skip_classes, skookumscript_ini_file_path);
      skookumscript_ini.GetArray(TEXT("CommonSettings"), TEXT("+AdditionalIncludes"), m_additional_includes, skookumscript_ini_file_path);
      }
    else if (!inherit_from_p)
      {
      // No SkookumScript.ini found, load legacy specifications from UHT's DefaultEngine.ini
      GConfig->GetArray(TEXT("Plugins"), TEXT("ScriptSupportedModules"), script_supported_modules, GEngineIni);
      GConfig->GetArray(TEXT("SkookumScriptGenerator"), TEXT("SkipClasses"), skip_classes, GEngineIni);
      }
    if (inherit_from_p)
      {
      m_script_supported_modules.Append(inherit_from_p->m_script_supported_modules);
      //m_additional_includes.Append(inherit_from_p->m_additional_includes);
      //m_skip_classes.Append(inherit_from_p->m_skip_classes);
      }
    for (const FString & module_name : script_supported_modules)
      {
      if (inherit_from_p && inherit_from_p->m_script_supported_modules.FindByKey(module_name))
        {
        FString inherited_skookumscript_ini_file_path = inherit_from_p->m_root_directory_path / TEXT("Config/SkookumScript.ini");
        GWarn->Log(ELogVerbosity::Warning, FString::Printf(TEXT("Module '%s' specified in both '%s' and '%s'. Ignoring duplicate."), *module_name, *skookumscript_ini_file_path, *inherited_skookumscript_ini_file_path));
        }
      else if (m_script_supported_modules.FindByKey(module_name))
        {
        GWarn->Log(ELogVerbosity::Warning, FString::Printf(TEXT("Module' %s' specified twice in '%s'. Ignoring duplicate."), *module_name, *skookumscript_ini_file_path));
        }
      else
        {
        m_script_supported_modules.Add(ModuleInfo(module_name));
        }
      }
    // Remember skip classes and convert to FName
    for (int32 i = 0; i < skip_classes.Num(); ++i)
      {
      m_skip_classes.Add(FName(*skip_classes[i]));
      }
    }

  m_current_module_p = nullptr;
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::GenerationTarget::begin_process_module(const FString & module_name, EBuildModuleType::Type module_type, const FString & module_generated_include_folder) const
  {
  m_current_module_p = const_cast<ModuleInfo *>(m_script_supported_modules.FindByKey(module_name));
  if (m_current_module_p)
    {
    m_current_module_p->m_scope = module_type == EBuildModuleType::GameRuntime ? ClassScope_project : ClassScope_engine;
    m_current_module_p->m_generated_include_folder = module_generated_include_folder;
    }

  return !!m_current_module_p;
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::GenerationTarget::can_export_class(UClass * class_p) const
  {
  return (does_class_have_static_class(class_p) || class_p->HasAnyClassFlags(CLASS_HasInstancedReference)) // Don't export classes that don't export DLL symbols
       && !m_skip_classes.Contains(class_p->GetFName()); // Don't export classes that set to skip in UHT config file
  }

//---------------------------------------------------------------------------------------

bool FSkookumScriptGenerator::GenerationTarget::can_export_struct(UStruct * struct_p) const
  {
  if (m_skip_classes.Contains(struct_p->GetFName())
   || FSkookumScriptGeneratorBase::get_skookum_struct_type(struct_p) != SkTypeID_UStruct) // do not export the special types Vector2, Vector3, Color etc.
    {
    return false;
    }

  return FSkookumScriptGeneratorBase::is_struct_type_supported(struct_p);
  }

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::GenerationTarget::export_class(UClass * class_p)
  {
  if (m_current_module_p && can_export_class(class_p))
    {
    m_current_module_p->m_classes_to_export.Add(class_p);
    }
  }

//---------------------------------------------------------------------------------------

const FSkookumScriptGenerator::ModuleInfo * FSkookumScriptGenerator::GenerationTarget::get_module(UField * type_p)
  {
  UPackage * package_p = CastChecked<UPackage>(type_p->GetOutermost());
  FString module_name = FPaths::GetBaseFilename(package_p->GetName());
  return m_script_supported_modules.FindByKey(module_name);
  }

//=======================================================================================
// MethodBinding implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::MethodBinding::make_method(UFunction * function_p)
  {
  m_function_p  = function_p;
  m_script_name = skookify_method_name(function_p->GetName(), function_p->GetReturnProperty());
  m_code_name   = m_script_name.Replace(TEXT("?"), TEXT("_Q"));
  }

//=======================================================================================
// DelegateBinding implementation
//=======================================================================================

//---------------------------------------------------------------------------------------

void FSkookumScriptGenerator::EventBinding::make_event(UMulticastDelegateProperty * property_p)
  {
  m_property_p = property_p;
  m_script_name_base = skookify_method_name(property_p->GetName());
  m_script_name_base.RemoveFromStart(TEXT("on_"));
  }

