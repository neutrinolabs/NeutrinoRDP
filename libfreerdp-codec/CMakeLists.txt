# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-codec cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(FREERDP_CODEC_SRCS
	bitmap.c
	color.c
	rfx_bitstream.h
	rfx_constants.h
	rfx_decode.c
	rfx_decode.h
	rfx_differential.c
	rfx_differential.h
	rfx_dwt.c
	rfx_dwt.h
	rfx_encode.c
	rfx_encode.h
	rfx_pool.c
	rfx_pool.h
	rfx_quantization.c
	rfx_quantization.h
	rfx_rlgr.c
	rfx_rlgr.h
	rfx_types.h
	rfx.c
	nsc.c
	jpeg.c
)

if(WITH_SSE2)
	set(FREERDP_CODEC_SRCS ${FREERDP_CODEC_SRCS}
	rfx_sse2.c
	rfx_sse2.h
)
	set_property(SOURCE rfx_sse2.c PROPERTY COMPILE_FLAGS "-msse2")
endif()

if(WITH_NEON)
	set(FREERDP_CODEC_SRCS ${FREERDP_CODEC_SRCS}
	rfx_neon.c
	rfx_neon.h
)
	set_property(SOURCE rfx_neon.c PROPERTY COMPILE_FLAGS "-mfpu=neon -mfloat-abi=softfp")
endif()

if(WITH_JPEG)
	set(FREERDP_JPEG_LIBS jpeg)
endif()

if(WITH_TJPEG)
	# find headers for Turbo JPEG
	find_path(TJpegIncDir turbojpeg.h PATHS /usr/include /opt/libjpeg-turbo/include $ENV{TURBOJPEG_PATH}/include)

	if (${TJpegIncDir} MATCHES "TJpegIncDir-NOTFOUND")
		message(FATAL_ERROR "Cant find TurboJPEG header files")
	endif()

	include_directories(${TJpegIncDir})
	message(STATUS "TurboJPEG headers are at ${TJpegIncDir}")

	# find libraries for Turbo JPEG
	#
	# note: if turbojpeg is built from source code and installed to default
	#       location, it gets installed to /opt/libjpeg-turbo and the lib
	#       path is lib64 or lib32 depending on platform
	#
	#       however, if it is installed using a prefix, the libpath is
	#       always lib

	if (NOT ${TJpegIncDir} MATCHES "/usr/include")
		if (${TJpegIncDir} MATCHES "/opt/libjpeg-turbo/include")
			if (EXISTS "/opt/libjpeg-turbo/lib64/libturbojpeg.so")
				# TurboJPEG lib is at /opt/libjpeg-turbo/lib64
				set(CMAKE_SHARED_LINKER_FLAGS "-L /opt/libjpeg-turbo/lib64 -Wl,-rpath=/opt/libjpeg-turbo/lib64")
				message(STATUS "TurboJPEG lib is at /opt/libjpeg-turbo/lib64")
			else()
				# TurboJPEG lib is at /opt/libjpeg-turbo/lib32
				set(CMAKE_SHARED_LINKER_FLAGS "-L /opt/libjpeg-turbo/lib32 -Wl,-rpath=/opt/libjpeg-turbo/lib32")
				message(STATUS "TurboJPEG lib is at /opt/libjpeg-turbo/lib32")
			endif()
		else()
			# TurboJPEG was installed using prefix, get lib location from env var
			set(CMAKE_SHARED_LINKER_FLAGS "-L $ENV{TURBOJPEG_PATH}/lib -Wl,-rpath=$ENV{TURBOJPEG_PATH}/lib")
			message(STATUS "TurboJPEG lib is at $ENV{TURBOJPEG_PATH}/lib")
		endif()
	endif()

	set(FREERDP_TJPEG_LIBS turbojpeg)
endif()

add_library(freerdp-codec ${FREERDP_CODEC_SRCS})

set_target_properties(freerdp-codec PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")
target_link_libraries(freerdp-codec LINK_PRIVATE freerdp-utils ${FREERDP_JPEG_LIBS} ${FREERDP_TJPEG_LIBS})

install(TARGETS freerdp-codec DESTINATION ${CMAKE_INSTALL_LIBDIR})
