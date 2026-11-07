# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
LOCAL_PATH := $(call my-dir)

POWDERROOT := ../../../..

include $(CLEAR_VARS)

LOCAL_MODULE    := powder-jni

LOCAL_C_INCLUDES := $(POWDERROOT)/port/sdl

LOCAL_CPPFLAGS := -DLINUX -DANDROID

LOCAL_SRC_FILES := powder-jni.cpp \
	 $(POWDERROOT)/main.cpp \
	 $(POWDERROOT)/thread.cpp \
	 $(POWDERROOT)/thread_linux.cpp \
	 $(POWDERROOT)/port/sdl/hamfake.cpp \
	 $(POWDERROOT)/action.cpp \
	 $(POWDERROOT)/assert.cpp \
	 $(POWDERROOT)/ai.cpp \
	 $(POWDERROOT)/artifact.cpp \
	 $(POWDERROOT)/bmp.cpp \
	 $(POWDERROOT)/build.cpp \
	 $(POWDERROOT)/buf.cpp \
	 $(POWDERROOT)/control.cpp \
	 $(POWDERROOT)/creature.cpp \
	 $(POWDERROOT)/dpdf_table.cpp \
	 $(POWDERROOT)/encyc_support.cpp \
	 $(POWDERROOT)/gfxengine.cpp \
	 $(POWDERROOT)/grammar.cpp \
	 $(POWDERROOT)/hiscore.cpp \
	 $(POWDERROOT)/input.cpp \
	 $(POWDERROOT)/intrinsic.cpp \
	 $(POWDERROOT)/item.cpp \
	 $(POWDERROOT)/map.cpp \
	 $(POWDERROOT)/mobref.cpp \
	 $(POWDERROOT)/msg.cpp \
	 $(POWDERROOT)/name.cpp \
	 $(POWDERROOT)/piety.cpp \
	 $(POWDERROOT)/rand.cpp \
	 $(POWDERROOT)/signpost.cpp \
	 $(POWDERROOT)/smokestack.cpp \
	 $(POWDERROOT)/speed.cpp \
	 $(POWDERROOT)/sramstream.cpp \
	 $(POWDERROOT)/stylus.cpp \
	 $(POWDERROOT)/victory.cpp \
	 $(POWDERROOT)/encyclopedia.cpp \
	 $(POWDERROOT)/glbdef.cpp \
	 $(POWDERROOT)/credits.cpp \
	 $(POWDERROOT)/license.cpp \
	 $(POWDERROOT)/gfx/all_bitmaps.cpp \
	 $(POWDERROOT)/rooms/allrooms.cpp

include $(BUILD_SHARED_LIBRARY)
