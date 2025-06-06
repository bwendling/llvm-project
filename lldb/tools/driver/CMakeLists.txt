set(LLVM_TARGET_DEFINITIONS Options.td)
tablegen(LLVM Options.inc -gen-opt-parser-defs)
add_public_tablegen_target(LLDBOptionsTableGen)

if(APPLE)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/lldb-Info.plist.in
    ${CMAKE_CURRENT_BINARY_DIR}/lldb-Info.plist
    )
  # Inline info plist in binary (use target_link_options for this as soon as CMake 3.13 is available)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_BINARY_DIR}/lldb-Info.plist")
endif()

if (UNIX AND ${CMAKE_SYSTEM_NAME} MATCHES "AIX")                               
  remove_definitions("-D_XOPEN_SOURCE=700")                                    
  add_definitions("-D_ALL_SOURCE")                                             
endif()

add_lldb_tool(lldb
  Driver.cpp
  Platform.cpp

  LINK_COMPONENTS
    Option
    Support
  LINK_LIBS
    liblldb
    lldbHost
    lldbUtility
  )

add_dependencies(lldb
  LLDBOptionsTableGen
  ${tablegen_deps}
)

if(LLDB_BUILD_FRAMEWORK)
  # In the build-tree, we know the exact path to the framework directory.
  # The installed framework can be in different locations.
  lldb_setup_rpaths(lldb
    BUILD_RPATH
      "${LLDB_FRAMEWORK_ABSOLUTE_BUILD_DIR}"
    INSTALL_RPATH
      "@loader_path/../../../SharedFrameworks"
      "@loader_path/../../System/Library/PrivateFrameworks"
      "@loader_path/../../Library/PrivateFrameworks"
  )
endif()
