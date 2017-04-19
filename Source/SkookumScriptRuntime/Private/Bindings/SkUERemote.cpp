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
//
// SkookumScript Remote Client
//=======================================================================================


//=======================================================================================
// Includes
//=======================================================================================

#include "SkUERemote.hpp"

#ifdef SKOOKUM_REMOTE_UNREAL

#include "SkUERuntime.hpp"
#include "Bindings/SkUEBlueprintInterface.hpp"
#include "../SkookumScriptRuntimeGenerator.h"
#include "Bindings/SkUEUtils.hpp"

#include "AssertionMacros.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Logging/LogMacros.h"
#include "Kismet/GameplayStatics.h"

#include <AgogCore/AMethodArg.hpp>

//=======================================================================================
// Local Global Structures
//=======================================================================================

namespace
{
  const int32_t SkUERemote_ide_port = 12357;

} // End unnamed namespace


//=======================================================================================
// SkUERemote Methods
//=======================================================================================

//---------------------------------------------------------------------------------------
// Constructor
// 
// #Author(s): Conan Reis
SkUERemote::SkUERemote(FSkookumScriptRuntimeGenerator * runtime_generator_p) :
  m_socket_p(nullptr),
  m_data_idx(ADef_uint32),
  m_editor_interface_p(nullptr),
  m_runtime_generator_p(runtime_generator_p),
  m_last_connected_to_ide(false)
  {
  }

//---------------------------------------------------------------------------------------
// Destructor
// 
// #Author(s): Conan Reis
SkUERemote::~SkUERemote()
  {
  }

//---------------------------------------------------------------------------------------
// Processes any remotely received data and call on_cmd_recv() whenever enough data is
// accumulated.
// 
// #Author(s): Conan Reis
void SkUERemote::process_incoming()
  {
  //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // Connected and data available?
  uint32 bytes_available;

  while (is_connected() && m_socket_p->HasPendingData(bytes_available))
    {
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Get datum size & prep datum
    int32    bytes_read;
    uint32_t datum_size;

    if (m_data_idx == ADef_uint32)
      {
      // Not working on a partially filled datum so get size of new datum
      if (bytes_available < sizeof(uint32_t))
        {
        // wait until there is enough data for size
        return;
        }

      // Read datum size from socket
      m_socket_p->Recv(reinterpret_cast<uint8 *>(&datum_size), sizeof(uint32_t), bytes_read);

      bytes_available -= sizeof(uint32_t);
      datum_size -= ADatum_header_size;
      m_data_in.ensure_size(datum_size, false);
      m_data_in.set_data_length(datum_size);
      m_data_idx = 0u;
      }
    else
      {
      datum_size = m_data_in.get_data_length();
      }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Begin or resume filling datum
    uint32_t cmd;
    uint32_t bytes_to_read;
    uint8 *  buffer_p = m_data_in.get_data_writable();

    while (bytes_available
      || (is_connected() && m_socket_p->HasPendingData(bytes_available)))
      {
      bytes_to_read = a_min(bytes_available, datum_size - m_data_idx);

      // Read datum data from socket
      m_socket_p->Recv(buffer_p + m_data_idx, bytes_to_read, bytes_read);

      m_data_idx += bytes_read;
      bytes_available = 0; // Reset to refresh on next loop

      if (m_data_idx == datum_size)
        {
        // Datum fully received and ready to use
        m_data_idx = ADef_uint32;

        // Parse command from IDE
        A_BYTE_STREAM_IN32(&cmd, &buffer_p);
        on_cmd_recv(eCommand(cmd), buffer_p, datum_size - 4u);

        // Exit filling datum while loop & look for new datum
        break;
        }
      // loop back and continue to try filling datum
      }
    } // While connected and has data
  }

//---------------------------------------------------------------------------------------
// Get (ANSI) string representation of specified socket IP Address and port
// 
// #Author(s): Conan Reis
AString SkUERemote::get_socket_str(const FInternetAddr & addr)
  {
  AString str;
  uint8_t ipv4addr[4];

  addr.GetIp(*reinterpret_cast<uint32 *>(ipv4addr));
  str.ensure_size(32u);
  str.format("%u.%u.%u.%u:%u", ipv4addr[3], ipv4addr[2], ipv4addr[1], ipv4addr[0], addr.GetPort());

  return str;
  }

//---------------------------------------------------------------------------------------
// Get (ANSI) string representation of current socket IP Address and port
// 
// #Author(s): Conan Reis
AString SkUERemote::get_socket_str()
  {
  if (!is_connected())
    {
    return "SkookumIDE.RemoteConnection(Disconnected)";
    }

  ISocketSubsystem * socket_system_p = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
  TSharedRef<FInternetAddr> local_addr = socket_system_p->CreateInternetAddr();

  m_socket_p->GetAddress(*local_addr);

  return get_socket_str(*local_addr);
  }

//---------------------------------------------------------------------------------------
// Gets local host IP address using ini file as a guide
// 
// #Author(s): Conan Reis
TSharedPtr<FInternetAddr> SkUERemote::get_ip_address_local()
  {
  ISocketSubsystem * socket_system_p = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

  bool bind_all_b;
  TSharedPtr<FInternetAddr> local_addr = socket_system_p->GetLocalHostAddr(*GLog, bind_all_b);

  return local_addr;
  }

//---------------------------------------------------------------------------------------
TSharedPtr<FInternetAddr> SkUERemote::get_ip_address_ide()
  {
  FString ip_file_path;

  TSharedPtr<FInternetAddr> ip_addr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();

  // Check if there's a file named "ide-ip.txt" present in the compiled binary folder
  // If so, use the ip stored in it to connect to the IDE
  if (static_cast<SkUERuntime*>(SkUERuntime::ms_singleton_p)->content_file_exists(
    TEXT("ide-ip.txt"), &ip_file_path))
    {
    FString ip_text;

    ip_file_path /= TEXT("ide-ip.txt");

    if (FFileHelper::LoadFileToString(ip_text, *ip_file_path))
      {
      bool is_valid;

      ip_addr->SetIp(*ip_text, is_valid);
      }
    }
  else
    {
    // "localhost" - Use local machine loop back adapter by default
    // Note that this is for 127.0.0.1 - the numbers are in network byte order.
    const uint8_t local_host_ipv4[4] = {1u, 0u, 0u, 127u};

    ip_addr->SetIp(*reinterpret_cast<const uint32 *>(local_host_ipv4));

    // $Note - CReis localhost is used instead of adapter address by default
    // Get IP address from network adapter
    //ip_addr = get_ip_address_local();
    
    // If non-desktop expect SkookumIDE IP address to be specified
    #if ((PLATFORM_WINDOWS == 0) && (PLATFORM_MAC == 0) && (PLATFORM_LINUX == 0))
      A_DPRINT("SkookumIDE Warning!\n"
        "  Did not find ide-ip.txt in the SkookumScript compiled binary folder.\n"
        "  It specifies the SkookumIDE IP address and without it the runtime will not be able\n"
        "  to connect to the SkookumIDE.\n\n"
        "  See 'Specifying the SkookumRuntime IP address' here:\n"
        "    http://skookumscript.com/docs/v3.0/ide/ip-addresses/#specifying-the-skookumruntime-ip-address"
        );
    #endif
    }

  ip_addr->SetPort(SkUERemote_ide_port);

  return ip_addr;
  }

//---------------------------------------------------------------------------------------
// Determines if runtime connected to remote SkookumIDE
// 
// #Author(s): Conan Reis
bool SkUERemote::is_connected() const
  {
  return this && m_socket_p
    && (m_socket_p->GetConnectionState() == SCS_Connected);
  }

//---------------------------------------------------------------------------------------
// Set remote connection mode.
// 
// #Author(s): Conan Reis
void SkUERemote::set_mode(eSkLocale mode)
  {
  if (m_mode != mode)
    {
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Stop old mode
    if (m_socket_p)
      {
      SkDebug::print(a_str_format("SkookumScript: Disconnecting... %s\n", get_socket_str().as_cstr()), SkLocale_local);

      ISocketSubsystem * socket_system_p = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

      if (!m_socket_p->Close())
        {
        SkDebug::print(a_str_format("  error closing socket: %i\n", (int32)socket_system_p->GetLastErrorCode()), SkLocale_local);
        }

      // Free the memory the OS allocated for this socket
      socket_system_p->DestroySocket(m_socket_p);
      m_socket_p = NULL;
      }


    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Start new mode

    SkRemoteBase::set_mode(mode);

    // $Revisit - CReis Update debug UI of SkookumIDE connection state

    switch (mode)
      {
      case SkLocale_embedded:
        set_connect_state(ConnectState_disconnected);
        SkDebug::print("\nSkookumScript: SkookumIDE not connected (off-line)\n\n", SkLocale_local);
        break;

      case SkLocale_runtime:
        {
        set_connect_state(ConnectState_connecting);

        m_socket_p = FTcpSocketBuilder(TEXT("SkookumIDE.RemoteConnection"))
          .AsReusable()
          .AsBlocking();

        bool success = false;

        if (m_socket_p)
          {
          TSharedPtr<FInternetAddr> ip_addr = get_ip_address_ide();
          SkDebug::print(a_str_format("SkookumScript: Attempting to connect to remote IDE at %s\n", get_socket_str(*ip_addr).as_cstr()), SkLocale_local);
          success = m_socket_p->Connect(*ip_addr);
          }

        if (!success)
          {
          SkDebug::print("\nSkookumScript: Failed attempt to connect with remote IDE!\n\n", SkLocale_local);
          set_mode(SkLocale_embedded);
          return;
          }

        SkDebug::print(a_str_format("SkookumScript: Connected %s\n", get_socket_str().as_cstr()), SkLocale_local);

        set_connect_state(ConnectState_authenticating);
        break;
        }
      }
    }
  }

//---------------------------------------------------------------------------------------
// Sends a command in datum form to the remote IDE
// 
// #Params
//   datum: command in datum form from a cmd_*() method
//   
// #Modifiers: virtual
// #Author(s): Conan Reis
SkRemoteBase::eSendResponse SkUERemote::on_cmd_send(const ADatum & datum)
  {
  if (is_connected())
    {
    int32 bytes_sent = 0;

    // $Note - CReis Assumes that Send() is able to transfer entire datum in 1 pass.
    m_socket_p->Send(datum.get_buffer(), datum.get_length(), bytes_sent);

    // Did sending go wrong?
    if (bytes_sent <= 0)
      {
      // Reconnect
      set_mode(SkLocale_embedded);
      ensure_connected(5.0);

      // Try again
      if (m_socket_p)
        {
        m_socket_p->Send(datum.get_buffer(), datum.get_length(), bytes_sent);
        }

      return SendResponse_Reconnecting;
      }
    }
  else
    {
    SkDebug::print(
      "SkookumScript: Remote IDE is not connected - command ignored!\n"
      "[Connect runtime to SkookumIDE and try again.]\n",
      SkLocale_local,
      SkDPrintType_warning);

    return SendResponse_Not_Connected;
    }

    return SendResponse_OK;
  }

//---------------------------------------------------------------------------------------
// Make this editable and tell IDE about it
void SkUERemote::on_cmd_make_editable()
  {
  SkProjectInfo project_info;

  FString error_msg(TEXT("Can't make project editable!"));
  #if WITH_EDITORONLY_DATA
    if (m_runtime_generator_p)
      {
      error_msg = m_runtime_generator_p->make_project_editable();
      }
  #endif
  if (error_msg.IsEmpty())
    {
    get_project_info(&project_info);
    SkRuntimeBase::ms_singleton_p->on_binary_hierarchy_path_changed();
    }

  // Send result back
  cmd_make_editable_reply(FStringToAString(error_msg), project_info);
  }

//---------------------------------------------------------------------------------------
void SkUERemote::on_cmd_freshen_compiled_reply(eCompiledState state)
  {
  // Call base class
  SkRemoteRuntimeBase::on_cmd_freshen_compiled_reply(state);
  }

//---------------------------------------------------------------------------------------
void SkUERemote::on_class_updated(SkClass * class_p)
  {
  // Call superclass behavior
  SkRemoteBase::on_class_updated(class_p);

  #if WITH_EDITOR
    AMethodArg2<ISkookumScriptRuntimeEditorInterface, UFunction*, bool> editor_on_function_updated_f(m_editor_interface_p, &ISkookumScriptRuntimeEditorInterface::on_function_updated);
    AMethodArg<ISkookumScriptRuntimeEditorInterface, UClass*>           editor_on_function_removed_from_class_f(m_editor_interface_p, &ISkookumScriptRuntimeEditorInterface::on_function_removed_from_class);
    tSkUEOnFunctionUpdatedFunc *          on_function_updated_f            = &editor_on_function_updated_f;
    tSkUEOnFunctionRemovedFromClassFunc * on_function_removed_from_class_f = &editor_on_function_removed_from_class_f;
  #else
    tSkUEOnFunctionUpdatedFunc *          on_function_updated_f            = nullptr;
    tSkUEOnFunctionRemovedFromClassFunc * on_function_removed_from_class_f = nullptr;
  #endif
  SkUEBlueprintInterface::get()->sync_bindings_from_binary(class_p, on_function_removed_from_class_f);
  SkUEBlueprintInterface::get()->expose_all_bindings(on_function_updated_f, true);
  }

//---------------------------------------------------------------------------------------
void SkUERemote::on_connect_change(eConnectState old_state)
  {
  // Call base class
  SkRemoteRuntimeBase::on_connect_change(old_state);

  // When in read-only (REPL) mode, regenerate all script files upon each connection to IDE
  #if WITH_EDITORONLY_DATA
    if (m_runtime_generator_p
     && m_runtime_generator_p->get_project_mode() == SkProjectMode_read_only
     && m_connect_state == SkRemoteBase::ConnectState_authenticated)
      {
      m_runtime_generator_p->delete_all_class_script_files();
      m_runtime_generator_p->generate_all_class_script_files();
      }
  #endif
  }

//---------------------------------------------------------------------------------------
double SkUERemote::get_elapsed_seconds()
  {
  return FPlatformTime::Seconds() - GStartTime;
  }

//---------------------------------------------------------------------------------------
void SkUERemote::wait_for_update()
  {
  FPlatformProcess::Sleep(.1f);
  process_incoming();
  }

//---------------------------------------------------------------------------------------
void SkUERemote::set_editor_interface(ISkookumScriptRuntimeEditorInterface * editor_interface_p)
  {
  m_editor_interface_p = editor_interface_p;
  }

//---------------------------------------------------------------------------------------
bool SkUERemote::spawn_remote_ide()
  {
  #ifdef A_PLAT_PC

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Look for SkookumIDE in game/project plug-in folder first.
    FString plugin_root_path(IPluginManager::Get().FindPlugin(TEXT("SkookumScript"))->GetBaseDir());
    FString ide_path(plugin_root_path / TEXT("SkookumIDE/SkookumIDE.exe"));
    // Use path of new Slate IDE if present
    FString slate_ide_path(plugin_root_path / TEXT("SkookumIDE/Engine/Binaries/Win64/SkookumIDE.exe"));
    FString launch_params;
    bool    use_slate_ide = false;

    if (FPaths::FileExists(slate_ide_path))
      {
      ide_path = slate_ide_path;
      use_slate_ide = true;
      }
    #if (SKOOKUM & SK_DEBUG)
      // If IDE exists in both plugin and as a debug build, load the IDE with the newer path
      FString debug_slate_ide_path(FPaths::EngineDir() / TEXT("Binaries/Win64/SkookumIDE-Win64-Debug.exe"));

      if (FPaths::FileExists(debug_slate_ide_path))
        {
        if (FPaths::FileExists(slate_ide_path))
          {
          FDateTime time_stamp, debug_time_stamp;
          IFileManager::Get().GetTimeStampPair(*slate_ide_path, *debug_slate_ide_path, time_stamp, debug_time_stamp);

          if (debug_time_stamp > time_stamp)
            {
            ide_path = debug_slate_ide_path;
            use_slate_ide = true;
            }
          }
        else
          {
          ide_path = debug_slate_ide_path;
          use_slate_ide = true;
          }
        }
    #endif

    if (!FPaths::FileExists(ide_path))
      {
      //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
      // Couldn't find IDE
      UE_LOG(LogSkookum, Warning,
        TEXT("Could not run SkookumScript IDE!\n")
        TEXT("Looked in plugin folder and did not find it:\n")
        TEXT("  %s\n\n")
        TEXT("Please ensure SkookumScript IDE app is present.\n"),
        *ide_path);

      return false;
      }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Found IDE app - now try to run it.

    if (use_slate_ide)
      {
      // Specify starting project
      FString project_path;

      #if WITH_EDITORONLY_DATA
      if (m_runtime_generator_p)
        {
        project_path = m_runtime_generator_p->get_project_file_path();
        }
      else
      #endif
        {
        // Is there an Sk project file in the usual location?
        FString project_ini(FPaths::GameDir() / TEXT("Scripts") / TEXT("Skookum-project.ini"));

        if (FPaths::FileExists(project_ini))
          {
          project_path = FPaths::ConvertRelativePathToFull(project_ini);
          }
        }

      // If we were not connected to the IDE when the plugin shut down (or crashed), start out minimized
      if (!m_last_connected_to_ide)
        {
        launch_params += FString::Printf(TEXT(" -m"), *project_path);
        }

      // If we know what project to launch, let the IDE know
      if (!project_path.IsEmpty())
        {
        launch_params += FString::Printf(TEXT(" -p \"%s\""), *project_path);
        }

      launch_params.Trim();
      }

    // Path seems to need to be made fully qualified in order to work
    FPaths::MakePathRelativeTo(ide_path, TEXT("/"));

    FPlatformProcess::LaunchFileInDefaultExternalApplication(*ide_path, *launch_params);

    return true;

  #else

    return false;

  #endif
  }


//---------------------------------------------------------------------------------------
void SkUERemote::get_project_info(SkProjectInfo * out_project_info_p)
  {
  // Get platform id string
  out_project_info_p->m_platform_id = FStringToAString(UGameplayStatics::GetPlatformName());

  // Get engine id string
  out_project_info_p->m_engine_id.ensure_size(20);
  out_project_info_p->m_engine_id.format("UE%d.%d.%d-%s", ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION, BUILT_FROM_CHANGELIST ? "Installed" : "Compiled");

  // Get game name
  out_project_info_p->m_project_name = FStringToAString(FApp::GetGameName());

  // Get project paths if any
  #if WITH_EDITORONLY_DATA
    if (m_runtime_generator_p)
      {
      out_project_info_p->m_project_path         = FStringToAString(m_runtime_generator_p->get_project_file_path());
      out_project_info_p->m_default_project_path = FStringToAString(m_runtime_generator_p->get_default_project_file_path());
      }
    else
  #endif
      {
      // Is there an Sk project file in the usual location?
      FString project_path(FPaths::GameDir() / TEXT("Scripts") / TEXT("Skookum-project.ini"));
      if (FPaths::FileExists(project_path))
        { 
        out_project_info_p->m_project_path = FStringToAString(FPaths::ConvertRelativePathToFull(project_path));
        }
      // Is there an Sk default project file in the usual location?
      FString default_project_path(IPluginManager::Get().FindPlugin(TEXT("SkookumScript"))->GetBaseDir() / TEXT("Scripts") / TEXT("Skookum-project-default.ini"));
      if (FPaths::FileExists(default_project_path))
        {
        out_project_info_p->m_default_project_path = FStringToAString(FPaths::ConvertRelativePathToFull(default_project_path));
        }
      }
  }


#endif  // SKOOKUM_REMOTE_UNREAL
