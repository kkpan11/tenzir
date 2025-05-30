cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)

project(
  azure-blob-storage
  DESCRIPTION "Azure Blob Storage plugin for Tenzir"
  LANGUAGES CXX)

find_package(Arrow QUIET REQUIRED CONFIG)
if (NOT ARROW_AZURE OR NOT ${ARROW_CS} STREQUAL "ON")
  message(
    WARNING
      "Disabling Azure Blob Storage plugin because the ARROW_AZURE flag is not found/enabled. You probably need to enable ARROW_AZURE when installing Apache Arrow."
  )
  return()
endif ()

include(CTest)

find_package(Tenzir REQUIRED)

TenzirRegisterPlugin(
  TARGET azure-blob-storage
  ENTRYPOINT src/plugin.cpp
  INCLUDE_DIRECTORIES include
  BUILTINS GLOB "${CMAKE_CURRENT_SOURCE_DIR}/builtins/*.cpp")

if (TENZIR_ENABLE_STATIC_EXECUTABLE)
  target_link_libraries(
    azure-blob-storage PUBLIC Azure::azure-identity Azure::azure-storage-blobs
                              Azure::azure-storage-files-datalake)
endif ()
