# DISTRHO Plugin Framework (DPF)
# Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
# Copyright (C) 2021-2022 Filipe Coelho <falktx@falktx.com>
# SPDX-License-Identifier: ISC

cmake_minimum_required(VERSION 3.7)

set(NAME imgui-demo-plugin)
project(${NAME})

add_subdirectory(dpf)

dpf_add_plugin(${NAME}
  TARGETS clap lv2 vst2 vst3 jack
  FILES_DSP
      src/PluginDSP.cpp
  FILES_UI
      src/PluginUI.cpp
      dpf-widgets/opengl/DearImGui.cpp)

target_include_directories(${NAME} PUBLIC src)
target_include_directories(${NAME} PUBLIC dpf-widgets/generic)
target_include_directories(${NAME} PUBLIC dpf-widgets/opengl)
