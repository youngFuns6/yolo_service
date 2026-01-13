# FindExosip2.cmake
# 查找 eXosip2 库，如果未找到则从源码自动编译

include(FindPackageHandleStandardArgs)
include(ExternalProject)

# 选项：是否强制从源码编译
if(NOT DEFINED EXOSIP2_BUILD_FROM_SOURCE)
    set(EXOSIP2_BUILD_FROM_SOURCE OFF CACHE BOOL "Force building eXosip2 from source")
endif()

# 首先尝试通过 pkg-config 查找系统安装的 eXosip2
if(NOT EXOSIP2_BUILD_FROM_SOURCE)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(PC_EXOSIP2 QUIET eXosip2)
        pkg_check_modules(PC_OSIP2 QUIET osip2)
        
        if(PC_EXOSIP2_FOUND AND PC_OSIP2_FOUND)
            find_path(EXOSIP2_INCLUDE_DIR
                NAMES eXosip2/eXosip.h
                PATHS ${PC_EXOSIP2_INCLUDE_DIRS}
                PATH_SUFFIXES eXosip2
            )
            
            find_path(OSIP2_INCLUDE_DIR
                NAMES osip2/osip.h
                PATHS ${PC_OSIP2_INCLUDE_DIRS}
                PATH_SUFFIXES osip2
            )
            
            find_library(EXOSIP2_LIBRARY
                NAMES eXosip2 libeXosip2
                PATHS ${PC_EXOSIP2_LIBRARY_DIRS}
            )
            
            find_library(OSIP2_LIBRARY
                NAMES osip2 libosip2
                PATHS ${PC_OSIP2_LIBRARY_DIRS}
            )
            
            find_library(OSIPPARSER2_LIBRARY
                NAMES osipparser2 libosipparser2
                PATHS ${PC_OSIP2_LIBRARY_DIRS}
            )
            
            if(EXOSIP2_INCLUDE_DIR AND OSIP2_INCLUDE_DIR AND 
               EXOSIP2_LIBRARY AND OSIP2_LIBRARY AND OSIPPARSER2_LIBRARY)
                set(EXOSIP2_FOUND TRUE)
                set(EXOSIP2_INCLUDE_DIRS ${EXOSIP2_INCLUDE_DIR} ${OSIP2_INCLUDE_DIR})
                set(EXOSIP2_LIBRARIES ${EXOSIP2_LIBRARY} ${OSIP2_LIBRARY} ${OSIPPARSER2_LIBRARY})
                message(STATUS "Found eXosip2 via pkg-config")
                message(STATUS "  Include dirs: ${EXOSIP2_INCLUDE_DIRS}")
                message(STATUS "  Libraries: ${EXOSIP2_LIBRARIES}")
            endif()
        endif()
    endif()
endif()

# 如果未找到系统安装的库，则从源码编译
if(NOT EXOSIP2_FOUND)
    message(STATUS "eXosip2 not found in system, will build from source")
    
    # 设置安装目录
    set(EXOSIP2_INSTALL_DIR "${CMAKE_BINARY_DIR}/third_party/exosip2-install")
    set(EXOSIP2_INCLUDE_DIRS 
        "${EXOSIP2_INSTALL_DIR}/include"
        "${EXOSIP2_INSTALL_DIR}/include/eXosip2"
        "${EXOSIP2_INSTALL_DIR}/include/osip2"
    )
    
    # 检查是否已安装（避免重复编译）
    if(EXISTS "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}eXosip2${CMAKE_STATIC_LIBRARY_SUFFIX}" AND
       EXISTS "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osip2${CMAKE_STATIC_LIBRARY_SUFFIX}")
        message(STATUS "eXosip2 already built, using existing installation")
        set(EXOSIP2_FOUND TRUE)
        set(EXOSIP2_LIBRARIES
            "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}eXosip2${CMAKE_STATIC_LIBRARY_SUFFIX}"
            "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osip2${CMAKE_STATIC_LIBRARY_SUFFIX}"
            "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osipparser2${CMAKE_STATIC_LIBRARY_SUFFIX}"
        )
    else()
        # 设置源码和构建目录
        set(OSIP2_SOURCE_DIR "${CMAKE_BINARY_DIR}/third_party/osip2")
        set(OSIP2_BUILD_DIR "${CMAKE_BINARY_DIR}/third_party/osip2-build")
        set(EXOSIP2_SOURCE_DIR "${CMAKE_BINARY_DIR}/third_party/exosip2")
        set(EXOSIP2_BUILD_DIR "${CMAKE_BINARY_DIR}/third_party/exosip2-build")
        
        # 检测构建工具
        find_program(AUTOCONF_EXECUTABLE autoconf)
        find_program(AUTOMAKE_EXECUTABLE automake)
        find_program(LIBTOOL_EXECUTABLE libtool)
        find_program(MAKE_EXECUTABLE make)
        find_program(GIT_EXECUTABLE git)
        
        if(NOT AUTOCONF_EXECUTABLE OR NOT AUTOMAKE_EXECUTABLE OR NOT LIBTOOL_EXECUTABLE)
            message(WARNING "autotools not found. Please install: autoconf automake libtool")
            message(WARNING "  macOS: brew install autoconf automake libtool")
            message(WARNING "  Linux: sudo apt-get install autoconf automake libtool")
            set(EXOSIP2_FOUND FALSE)
        else()
            # 检查是否需要运行 autogen.sh
            find_program(SH_EXECUTABLE sh)
            find_program(BASH_EXECUTABLE bash)
            
            # 设置版本号
            set(OSIP2_VERSION "5.1.2")
            set(EXOSIP2_VERSION "5.1.2")
            
            # 设置下载 URL（使用官方下载地址，支持多个备用地址）
            # osip2 下载地址
            set(OSIP2_URLS
                "http://ftp.twaren.net/Unix/NonGNU/osip/libosip2-${OSIP2_VERSION}.tar.gz"
                "http://download.savannah.gnu.org/releases/osip/libosip2-${OSIP2_VERSION}.tar.gz"
                "http://ftp.gnu.org/gnu/osip/libosip2-${OSIP2_VERSION}.tar.gz"
            )
            # eXosip2 下载地址
            set(EXOSIP2_URLS
                "http://download.savannah.gnu.org/releases/exosip/libexosip2-${EXOSIP2_VERSION}.tar.gz"
                "http://download.savannah.nongnu.org/releases/exosip/libexosip2-${EXOSIP2_VERSION}.tar.gz"
            )
            
            # 编译 osip2
            # 创建配置脚本
            set(OSIP2_CONFIGURE_SCRIPT "${CMAKE_BINARY_DIR}/osip2-configure.sh")
            file(WRITE ${OSIP2_CONFIGURE_SCRIPT}
                "#!/bin/sh\n"
                "cd ${OSIP2_SOURCE_DIR}\n"
                "if [ -f autogen.sh ]; then\n"
                "  ./autogen.sh\n"
                "fi\n"
                "./configure --prefix=${EXOSIP2_INSTALL_DIR} --enable-static --disable-shared CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}\n"
            )
            file(CHMOD ${OSIP2_CONFIGURE_SCRIPT} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
            
            ExternalProject_Add(osip2
                URL ${OSIP2_URLS}
                DOWNLOAD_NAME libosip2-${OSIP2_VERSION}.tar.gz
                SOURCE_DIR ${OSIP2_SOURCE_DIR}
                CONFIGURE_COMMAND ${SH_EXECUTABLE} ${OSIP2_CONFIGURE_SCRIPT}
                BUILD_COMMAND ${MAKE_EXECUTABLE} -j${CMAKE_BUILD_PARALLEL_LEVEL}
                BUILD_IN_SOURCE 1
                INSTALL_COMMAND ${MAKE_EXECUTABLE} install
                UPDATE_COMMAND ""
                LOG_DOWNLOAD 1
                LOG_CONFIGURE 1
                LOG_BUILD 1
                LOG_INSTALL 1
            )
            
            # 编译 eXosip2（依赖 osip2）
            # 创建配置脚本
            set(EXOSIP2_CONFIGURE_SCRIPT "${CMAKE_BINARY_DIR}/exosip2-configure.sh")
            file(WRITE ${EXOSIP2_CONFIGURE_SCRIPT}
                "#!/bin/sh\n"
                "cd ${EXOSIP2_SOURCE_DIR}\n"
                "if [ -f autogen.sh ]; then\n"
                "  ./autogen.sh\n"
                "fi\n"
                "export PKG_CONFIG_PATH=${EXOSIP2_INSTALL_DIR}/lib/pkgconfig:$$PKG_CONFIG_PATH\n"
                "./configure --prefix=${EXOSIP2_INSTALL_DIR} --enable-static --disable-shared CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}\n"
            )
            file(CHMOD ${EXOSIP2_CONFIGURE_SCRIPT} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
            
            ExternalProject_Add(exosip2
                URL ${EXOSIP2_URLS}
                DOWNLOAD_NAME libexosip2-${EXOSIP2_VERSION}.tar.gz
                SOURCE_DIR ${EXOSIP2_SOURCE_DIR}
                DEPENDS osip2
                CONFIGURE_COMMAND ${SH_EXECUTABLE} ${EXOSIP2_CONFIGURE_SCRIPT}
                BUILD_COMMAND ${MAKE_EXECUTABLE} -j${CMAKE_BUILD_PARALLEL_LEVEL}
                BUILD_IN_SOURCE 1
                INSTALL_COMMAND ${MAKE_EXECUTABLE} install
                UPDATE_COMMAND ""
                LOG_DOWNLOAD 1
                LOG_CONFIGURE 1
                LOG_BUILD 1
                LOG_INSTALL 1
            )
            
            # 创建一个目标，确保所有库都构建完成
            add_custom_target(exosip2_libs_ready
                DEPENDS exosip2 osip2
                COMMENT "eXosip2 libraries ready"
            )
            
            set(EXOSIP2_FOUND TRUE)
            set(EXOSIP2_LIBRARIES
                "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}eXosip2${CMAKE_STATIC_LIBRARY_SUFFIX}"
                "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osip2${CMAKE_STATIC_LIBRARY_SUFFIX}"
                "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osipparser2${CMAKE_STATIC_LIBRARY_SUFFIX}"
            )
            
            message(STATUS "eXosip2 will be built from source")
            message(STATUS "  Install dir: ${EXOSIP2_INSTALL_DIR}")
            message(STATUS "  Include dirs: ${EXOSIP2_INCLUDE_DIRS}")
            message(STATUS "  Libraries: ${EXOSIP2_LIBRARIES}")
        endif()
    endif()
endif()

# 设置导入目标（如果从源码编译）
if(EXOSIP2_FOUND AND TARGET exosip2 AND TARGET osip2)
    # 创建导入目标以便其他目标可以依赖
    if(NOT TARGET exosip2::exosip2)
        add_library(exosip2::exosip2 STATIC IMPORTED)
        set_target_properties(exosip2::exosip2 PROPERTIES
            IMPORTED_LOCATION "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}eXosip2${CMAKE_STATIC_LIBRARY_SUFFIX}"
            INTERFACE_INCLUDE_DIRECTORIES "${EXOSIP2_INSTALL_DIR}/include"
        )
    endif()
    
    if(NOT TARGET osip2::osip2)
        add_library(osip2::osip2 STATIC IMPORTED)
        set_target_properties(osip2::osip2 PROPERTIES
            IMPORTED_LOCATION "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osip2${CMAKE_STATIC_LIBRARY_SUFFIX}"
            INTERFACE_INCLUDE_DIRECTORIES "${EXOSIP2_INSTALL_DIR}/include"
        )
    endif()
    
    if(NOT TARGET osipparser2::osipparser2)
        add_library(osipparser2::osipparser2 STATIC IMPORTED)
        set_target_properties(osipparser2::osipparser2 PROPERTIES
            IMPORTED_LOCATION "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osipparser2${CMAKE_STATIC_LIBRARY_SUFFIX}"
            INTERFACE_INCLUDE_DIRECTORIES "${EXOSIP2_INSTALL_DIR}/include"
        )
    endif()
endif()

find_package_handle_standard_args(Exosip2
    FOUND_VAR EXOSIP2_FOUND
    REQUIRED_VARS EXOSIP2_INCLUDE_DIRS EXOSIP2_LIBRARIES
)

