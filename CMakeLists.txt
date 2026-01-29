cmake_minimum_required(VERSION 3.11)
project(OpenLiveStacker)

enable_testing()
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows: do not override the build settings with GoogleTest's
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include_directories(include)
if(NOT WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -g")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -rdynamic")
else()
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDLL_EXPORT /wd4267 /wd4244 /wd4101 /wd4141 -O2 -D_USE_MATH_DEFINES -D_CRT_SECURE_NO_WARNINGS ")
endif()
option(OPENCV_ROOT "Path to OpenCV Library" "")
option(NO_SSE4 "Disable SSE 4.1/4.3 on x86/x86_64 builds" OFF)
option(USE_INDI_AS_LIBRARY "Use special variand of indi - embedded server/client/deriver" OFF)

find_library(ASI_CAMERA_LIB ASICamera2)
find_path(ASI_CAMERA_HDR ASICamera2.h)
find_library(LIBUSB_LIB usb-1.0 NAMES usb-1.0 libusb-1.0) 
find_library(SVB_CAMERA_LIB SVBCameraSDK) 
find_path(SVB_CAMERA_HDR SVBCameraSDK.h)

if(USE_INDI_AS_LIBRARY)
    find_library(INDI_LIB1 indi_unified_client_server HINT /opt/indi_as_library/lib)
    find_library(INDI_LIB2 indi_unified HINT /opt/indi_as_library/lib)
    set(INDI_LIB ${INDI_LIB1} ${INDI_LIB2})
    find_path(INDI_HEADER libindi/indiserver_lib.h 
                HINT ${CMAKE_FIND_ROOT_PATH}/include
                HINT /opt/indi_as_library/include)
    include_directories(${INDI_HEADER}/libindi)
    add_definitions(-DINDI_AS_LIBRARY)
else()
    find_library(INDI_LIB indiclient)
    find_path(INDI_HEADER libindi/baseclient.h)
    if(INDI_HEADER)
        include_directories(${INDI_HEADER})
    endif()
endif()

find_library(INDIGO_LIB indigo)
find_path(INDIGO_HEADER indigo/indigo_bus.h)

find_library(TOUPCAM_CAMERA_LIB toupcam)
find_path(TOUPCAM_CAMERA_HDR toupcam.h)
find_path(MEADECAM_CAMERA_HDR meadecam.h)
find_library(MEADECAM_CAMERA_LIB meadecam)

find_library(LIBTIFF tiff)
find_library(LIBCPPCMS cppcms)
find_library(LIBBOOSTER booster)
find_library(LIBUVC uvc)
find_library(LIBRAW_LIB raw_r)
find_path(LIBRAW_HDR libraw/libraw.h)


find_path(LIBCURL_HDR curl/curl.h)
find_path(LIBZ_HDR zlib.h)
find_path(LIBCPPCMS_HRD cppcms/application.h)
find_path(LIBCAMERA2_HDR camera/NdkCameraDevice.h)
find_path(LIBMEDIANDK_HDR media/NdkImageReader.h)
find_library(LIBCURL curl NAMES curl curl_imp libcurl_imp)
find_library(LIBZ z NAMES z zlib)
find_library(LIBCAMERA2_LIB camera2ndk)
find_library(LIBMEDIANDK_LIB mediandk)

find_path(LIBGPHOTO2_HDR gphoto2/gphoto2.h)
find_library(LIBGPHOTO2_LIB gphoto2)
find_library(LIBGPHOTO2_PORT_LIB gphoto2_port)



set(WITH_SSE OFF)
set(OLS_EXTRA )
set(OLS_LIB_EXTRA )


include_directories(${LIBCPPCMS_HRD})

if(LIBCURL_HDR AND LIBCURL)
    set(WITH_CURL ON)
    add_definitions(-DWITH_CURL)
    message("-- ASTAP DB Download enabled")
else()
    message("-- ASTAP DB Download DISABLED")
    set(WITH_CURL OFF)
endif()



if(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(OLS_EXTRA src/android_main.cpp)
    set(ANDROID_BUILD ON)
    add_definitions(-DANDROID_SUPPORT)
    add_definitions(-DOLS_ARCH="${ANDROID_ABI}")
    find_library(LIBUSB usb1.0)
    if(${ANDROID_ABI} STREQUAL "x86_64" OR ${ANDROID_ABI} STREQUAL "x86")
        set(WITH_SSE ON)
        if(${ANDROID_ABI} STREQUAL "x86")
            set(NO_SSE4 ON)
        endif()
        add_definitions(-DUSE_CV_SIMD)
    elseif(${ANDROID_ABI} STREQUAL "arm64-v8a") # No simd for armeabi-v7a
        add_definitions(-DUSE_CV_SIMD)
        add_definitions(-DUSE_ARM_SIMD)
    endif()
else()
    message("CPU: ${CMAKE_SYSTEM_PROCESSOR}")
    add_definitions(-DOLS_ARCH="${CMAKE_SYSTEM_PROCESSOR}")
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86") 
        add_definitions(-DUSE_CV_SIMD)
        set(WITH_SSE ON)
    endif()        
    set(ANDROID_BUILD OFF)
endif()


find_path(LIBCFITSIO_HDR fitsio.h)
find_library(LIBCFITSIO cfitsio)


if(WITH_SSE)
    if(NOT WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
        if(NOT NO_SSE4)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1 -msse4.2")
        endif()
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
        if(NOT NO_SSE4)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE4.2")
        endif()
    endif()
endif()        



if(OPENCV_ROOT)
    message("Searching lib in ${OPENCV_ROOT}")
    find_library(OPENCV_CORE        opencv_core      PATHS ${OPENCV_ROOT}/lib/ NO_DEFAULT_PATH)
    find_library(OPENCV_IMGPROC     opencv_imgproc   PATHS ${OPENCV_ROOT}/lib/ NO_DEFAULT_PATH)
    find_library(OPENCV_IMGCODECS   opencv_imgcodecs PATHS ${OPENCV_ROOT}/lib/ NO_DEFAULT_PATH)
    find_library(OPENCV_DNN         opencv_dnn       PATHS ${OPENCV_ROOT}/lib/ NO_DEFAULT_PATH)
    find_path(OPENCV_INC opencv2/core.hpp PATHS ${OPENCV_ROOT}/include/opencv4;${OPENCV_ROOT}/include NO_DEFAULT_PATH )
else()
    find_package(OpenCV 4.5.5 QUIET COMPONENTS core imgproc imgcodecs dnn HINTS /usr/local)
    if(NOT OpenCV_FOUND)
        find_package(PkgConfig QUIET)
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(OPENCV4 opencv4 QUIET)
            if(OPENCV4_FOUND AND OPENCV4_VERSION VERSION_GREATER_EQUAL "4.5.5")
                message("- Found OpenCV ${OPENCV4_VERSION} via pkg-config")
                set(OPENCV_INC ${OPENCV4_INCLUDE_DIRS})
                set(OPENCV_CORE ${OPENCV4_LIBRARIES})
                set(OpenCV_VERSION ${OPENCV4_VERSION})
                set(OpenCV_FOUND ON)
            endif()
        endif()
    endif()

    if(OpenCV_FOUND)
        set(OPENCV_INC ${OpenCV_INCLUDE_DIRS})
        set(OPENCV_CORE ${OpenCV_LIBS})
        message("- Found OpenCV ${OpenCV_VERSION}")
        set(OPENCV_IMGPROC "")
        set(OPENCV_IMGCODECS "")
        set(OPENCV_DNN "")
    else()
        find_library(OPENCV_CORE        opencv_core      )
        find_library(OPENCV_IMGPROC     opencv_imgproc   )
        find_library(OPENCV_IMGCODECS   opencv_imgcodecs )
        find_library(OPENCV_DNN         opencv_dnn       )
        find_path(OPENCV_INC_A opencv2/core.hpp)
        if(NOT OPENCV_INC_A)
            find_path(OPENCV_INC_B opencv4/opencv2/core.hpp)
            if(OPENCV_INC_B)
                set(OPENCV_INC "${OPENCV_INC_B}/opencv4")
            else()
                message("Can't find OpenCV include")
            endif()
        else()
            set(OPENCV_INC "${OPENCV_INC_A}")
        endif()
        if(OPENCV_CORE)
            # Try to get version if possible, but find_library doesn't provide it easily.
            message("- Found OpenCV via find_library")
        endif()
    endif()
endif()

if(OpenCV_VERSION AND OpenCV_VERSION VERSION_LESS "4.5.5")
    message(WARNING "OpenCV version ${OpenCV_VERSION} is detected. 3D DepthAnything feature requires at least 4.5.5 for ONNX models with external weights (.onnx_data).")
endif()

message("- OpenCV Include Path ${OPENCV_INC}")
include_directories(${OPENCV_INC})

option(USE_ONNXRUNTIME "Use Microsoft ONNX Runtime for DepthAnything" ON)

if(USE_ONNXRUNTIME)
    # Look for headers in common locations
    find_path(ONNXRUNTIME_HDR onnxruntime_cxx_api.h
            HINTS /usr/local/include /usr/local/include/onnxruntime /opt/onnxruntime/include)

    # Look for the shared library
    find_library(ONNXRUNTIME_LIB onnxruntime
            HINTS /usr/local/lib /opt/onnxruntime/lib)

    if(ONNXRUNTIME_HDR AND ONNXRUNTIME_LIB)
        message("- Found ONNX Runtime: ${ONNXRUNTIME_LIB}")
        include_directories(${ONNXRUNTIME_HDR})
        add_definitions(-DUSE_ONNXRUNTIME)
    else()
        message(WARNING "- ONNX Runtime NOT found. DepthAnything V3 features may fail or require OpenCV DNN fallback.")
        set(USE_ONNXRUNTIME OFF)
    endif()
endif()

if(WITH_CURL)
    include_directories(${LIBCURL_HDR})
endif()
include_directories(${LIBZ_HDR})


if(ASI_CAMERA_HDR AND ASI_CAMERA_LIB)
    include_directories(${ASI_CAMERA_HDR})
    add_library(ols_driver_asi SHARED src/asi_camera.cpp)
    target_link_libraries(ols_driver_asi ${ASI_CAMERA_LIB})
    install(TARGETS ols_driver_asi
            LIBRARY DESTINATION lib
    )
    message("- Building ASI Driver")
else()
    message("- No ASI SDK header/lib found")
endif()

# Remove AND LIBUSB_LIB
# link ${LIBUSB_LIB}

if(SVB_CAMERA_HDR AND SVB_CAMERA_LIB AND (WIN32 OR LIBUSB_LIB))
    include_directories(${SVB_CAMERA_HDR})
    add_library(ols_driver_svb SHARED src/svb_camera.cpp)
    target_link_libraries(ols_driver_svb ${SVB_CAMERA_LIB})
    if(NOT WIN32)
        target_link_libraries(ols_driver_svb ${LIBUSB_LIB})
    endif()
    install(TARGETS ols_driver_svb
            LIBRARY DESTINATION lib
    )
    message("- Building SVBony Driver")
else()
    message("- No SVBony SDK header/lib found")
endif()


if(TOUPCAM_CAMERA_HDR AND TOUPCAM_CAMERA_LIB)
    include_directories(${TOUPCAM_CAMERA_HDR})
    add_library(ols_driver_toup SHARED src/toup_camera.cpp)
    target_link_libraries(ols_driver_toup ${TOUPCAM_CAMERA_LIB})
    if(WIN32)
        target_link_libraries(ols_driver_toup ${LIBUSB_LIB})
    endif()
    install(TARGETS ols_driver_toup
            LIBRARY DESTINATION lib
    )
    message("- Building Toupcam Driver")
else()
    message("- No Toupcam SDK header/lib found")
endif()

if(MEADECAM_CAMERA_HDR AND MEADECAM_CAMERA_LIB)
    include_directories(${MEADECAM_CAMERA_HDR})
    add_library(ols_driver_meade SHARED src/toup_camera.cpp)
    target_compile_definitions(ols_driver_meade PRIVATE MEADECAM)
    
    target_link_libraries(ols_driver_meade ${MEADECAM_CAMERA_LIB})
    install(TARGETS ols_driver_meade
            LIBRARY DESTINATION lib
    )
    message("- Building Meade Driver")
else()
    message("- No Meade SDK header/lib found")
endif()

if(LIBCFITSIO AND LIBCFITSIO_HDR)
    set(OLS_EXTRA ${OLS_EXTRA} src/fitsmat.cpp)
    set(OLS_LIB_EXTRA ${LIBCFITSIO})
    include_directories(${LIBCFITSIO_HDR})
    add_definitions(-DWITH_CFITSIO)
    message("- Building OLS with fits support")
else()
    message("- Building OLS without fits support")
endif()


if(INDI_LIB AND INDI_HEADER)
    message(" - Building OLS with INDI Mount Support")
    set(OLS_EXTRA ${OLS_EXTRA} src/indi_mount.cpp)
    set(OLS_LIB_EXTRA ${OLS_LIB_EXTRA} ${INDI_LIB})
    add_definitions(-DWITH_INDI)
else()
    message(" - Building OLS without INDI Mount Support")
endif()




add_library(ols SHARED 
    src/camera.cpp 
    src/ols.cpp
    src/mount_ctl.cpp
    src/mount.cpp
    src/video_generator.cpp
    src/tiffmat.cpp
    src/debug_save_processor.cpp
    src/post_processor.cpp
    src/pre_processor.cpp
    src/stacker_processor.cpp
    src/common_utils.cpp
    src/util.cpp
    src/plate_solver.cpp
    src/server_sent_events.cpp
    src/downloader.cpp
    src/depth_anything_v3.cpp
    src/allocator.cpp
    src/alpaca_client.cpp
    src/alpaca_mount.cpp
    src/focuser_ctl.cpp
    src/focuser.cpp
    src/alpaca_focuser.cpp
    ${OLS_EXTRA}
    )

target_link_libraries(ols ${LIBTIFF} ${LIBCPPCMS} ${LIBZ} ${LIBBOOSTER} ${OPENCV_CORE} ${OPENCV_IMGPROC} ${OPENCV_IMGCODECS} ${OPENCV_DNN} ${OLS_LIB_EXTRA})

if(USE_ONNXRUNTIME AND ONNXRUNTIME_LIB)
    target_link_libraries(ols ${ONNXRUNTIME_LIB})
endif()

if(NOT WIN32)
    target_link_libraries(ols dl)
else()
    target_link_libraries(ols iphlpapi)
endif()

if(ANDROID_BUILD)
    target_link_libraries(ols log)
endif()

if(WITH_CURL)
    target_link_libraries(ols ${LIBCURL} ${LIBZ})
endif()

if(TRUE) # Always build alpaca
    add_library(ols_driver_alpaca SHARED src/alpaca_camera.cpp)
    target_link_libraries(ols_driver_alpaca ols ${OPENCV_CORE})    
    message("- Building Alpaca Camera Driver")
    install(TARGETS ols_driver_alpaca
            LIBRARY DESTINATION lib
    )
endif()

if(LIBUVC)
    add_library(ols_driver_uvc SHARED src/uvc_camera.cpp)
    target_link_libraries(ols_driver_uvc ols ${LIBUVC})    
    if(ANDROID_BUILD)   
        target_link_libraries(ols_driver_uvc ${LIBUSB})
    endif()
    message("- Building UVC Driver")
    install(TARGETS ols_driver_uvc
            LIBRARY DESTINATION lib
    )
else()
    message("- No libuvc, skipping UVC Driver")
endif()

add_library(ols_driver_sim SHARED src/sim_camera.cpp)
target_link_libraries(ols_driver_sim ols ${OPENCV_IMGCODECS})




if(LINUX AND NOT ANDROID_BUILD)  
    add_library(ols_driver_wdir SHARED src/wdir_camera.cpp)
    target_link_libraries(ols_driver_wdir ols ${OPENCV_CORE} ${OPENCV_IMGPROC} ${OPENCV_IMGCODECS} ${LIBBOOSTER})
    if(LIBRAW_LIB AND LIBRAW_HDR)
        include_directories(${LIBRAW_HDR})
        target_compile_definitions(ols_driver_wdir PRIVATE WITH_LIBRAW)
        target_link_libraries(ols_driver_wdir ${LIBRAW_LIB})
        message("- Building WDir driver with libraw")
        install(TARGETS ols_driver_wdir
                LIBRARY DESTINATION lib
        )
    else()
        message("- Building WDir driver without libraw")
    endif()
endif()


if(INDIGO_HEADER AND INDIGO_LIB AND LIBCFITSIO AND LIBCFITSIO_HDR AND NOT ANDROID_BUILD)
    include_directories(${INDIGO_HEADER})
    add_library(ols_driver_indigo SHARED src/indigo_camera.cpp)
    target_link_libraries(ols_driver_indigo ${INDIGO_LIB})
    message(" -- Building indigo camera driver")
    install(TARGETS ols_driver_indigo
            LIBRARY DESTINATION lib
    )
else()
    message(" -- Indigo camera driver disabled")
endif()

if(INDI_HEADER AND INDI_LIB AND LIBCFITSIO AND LIBCFITSIO_HDR)
    include_directories(${INDI_HEADER})
    add_library(ols_driver_indi SHARED src/indi_camera.cpp)
    target_link_libraries(ols_driver_indi ${INDI_LIB} ols ${OPENCV_CORE})
    message(" -- Building indi camera driver")
    install(TARGETS ols_driver_indi
            LIBRARY DESTINATION lib
    )
else()
    message(" -- Indi camera driver disabled")
endif()




if(LIBGPHOTO2_HDR AND LIBGPHOTO2_LIB AND LIBGPHOTO2_PORT_LIB)
    include_directories(${LIBGPHOTO2_HDR})
    add_library(ols_driver_gphoto2 SHARED src/gphoto2_camera.cpp)
    target_link_libraries(ols_driver_gphoto2 ols ${LIBGPHOTO2_LIB} ${LIBGPHOTO2_PORT_LIB} ${OPENCV_CORE} ${OPENCV_IMGCODECS} ${OPENCV_IMGPROC})

    if(LIBRAW_LIB AND LIBRAW_HDR)
        include_directories(${LIBRAW_HDR})
        set_target_properties(ols_driver_gphoto2 PROPERTIES COMPILE_DEFINITIONS WITH_LIBRAW)
        target_link_libraries(ols_driver_gphoto2 ${LIBRAW_LIB})
        message("- Building GPhoto2 Driver with libraw")
    else()
        message("- Building GPhoto2 Driver without libraw")
    endif()


    install(TARGETS ols_driver_gphoto2
            LIBRARY DESTINATION lib
    )
else()
    message("- No GPhoto2 header/lib found")
endif()

if(ANDROID_BUILD)
    if(LIBCAMERA2_HDR AND LIBCAMERA2_LIB AND LIBMEDIANDK_HDR AND LIBMEDIANDK_LIB)
        include_directories(LIBCAMERA2_HDR)
        include_directories(LIBMEDIANDK_HDR)
        add_library(ols_driver_android SHARED src/android_camera.cpp)
        target_link_libraries(ols_driver_android ols ${OPENCV_CORE} ${LIBCAMERA2_LIB} ${LIBMEDIANDK_LIB} android)
        message("- Building Android camera driver ")
        install(TARGETS ols_driver_android
                LIBRARY DESTINATION lib
        )
    else()
        message("- No Android Ndk Camera2 header/lib found, disabling android camera support")
    endif()
endif()

install(TARGETS ols ols_driver_sim 
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
)



if(NOT ANDROID_BUILD)
    include_directories(src)
    add_executable(test_indigo_format_parsing test/test_indigo_format_parsing.cpp)
    add_executable(test_camera test/test_camera.cpp)
    add_executable(ols_cmd src/ols_cmd.cpp)
    add_executable(offline_ols test/offline_sim.cpp)
    add_executable(ols_plate_solver_test test/plate_solver_test.cpp)
    add_executable(test_depth_anything test/depth_anything_test.cpp)
    add_executable(test_generate_3d_batch test/generate_3d_batch.cpp)
    target_link_libraries(test_camera ols)
    target_link_libraries(ols_cmd ols)
    target_link_libraries(offline_ols ols)
    target_link_libraries(ols_plate_solver_test ols ${OPENCV_CORE} ${OPENCV_IMGPROC} ${OPENCV_IMGCODECS})
    target_link_libraries(test_depth_anything ols ${OPENCV_CORE} ${OPENCV_IMGPROC} ${OPENCV_IMGCODECS} ${OPENCV_DNN})
    target_link_libraries(test_generate_3d_batch ols ${OPENCV_CORE} ${OPENCV_IMGPROC} ${OPENCV_IMGCODECS} ${OPENCV_DNN})
    target_link_libraries(test_indigo_format_parsing ols)
    add_executable(ols_unit_tests test/unit/test_util.cpp test/unit/test_astronomy.cpp)
    target_link_libraries(ols_unit_tests GTest::gtest_main ols)
    
    include(GoogleTest)
    gtest_discover_tests(ols_unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    
    add_test(NAME indigo_format_parsing COMMAND test_indigo_format_parsing WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    install(TARGETS ols_driver_sim ols_cmd offline_ols
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
    )
    install(DIRECTORY www-data DESTINATION share/ols)
    install(DIRECTORY sim DESTINATION share/ols)
    install(PROGRAMS scripts/ols_gui DESTINATION bin)
endif()

find_program(PYTHON NAMES python3 python)
if(NOT PYTHON)
    message(FATAL_ERROR "Can't build without Python interpreter")
endif()

add_custom_command(TARGET ols POST_BUILD
    COMMAND ${PYTHON} 
                ${CMAKE_CURRENT_SOURCE_DIR}/scripts/make_db.py 
                ${CMAKE_CURRENT_SOURCE_DIR}/www-data/media/js/jsdb.js 
                ${CMAKE_CURRENT_SOURCE_DIR}/www-data/media/js/vsop87_lib.js
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building DB"
)
