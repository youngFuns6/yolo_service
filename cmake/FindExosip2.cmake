# FindExosip2.cmake
# 查找 eXosip2 库，如果未找到则从源码自动编译

include(FindPackageHandleStandardArgs)
include(ExternalProject)

# 选项：是否强制从源码编译
if(NOT DEFINED EXOSIP2_BUILD_FROM_SOURCE)
    set(EXOSIP2_BUILD_FROM_SOURCE OFF CACHE BOOL "Force building eXosip2 from source")
endif()

# 首先尝试手动指定路径（用于交叉编译或自定义安装位置）
if(NOT EXOSIP2_FOUND AND NOT EXOSIP2_BUILD_FROM_SOURCE AND DEFINED EXOSIP2_ROOT)
    find_path(EXOSIP2_INCLUDE_DIR
        NAMES eXosip2/eXosip.h
        PATHS ${EXOSIP2_ROOT}
        PATH_SUFFIXES include
        NO_DEFAULT_PATH
    )
    
    find_path(OSIP2_INCLUDE_DIR
        NAMES osip2/osip.h
        PATHS ${EXOSIP2_ROOT}
        PATH_SUFFIXES include
        NO_DEFAULT_PATH
    )
    
    find_library(EXOSIP2_LIBRARY
        NAMES eXosip2 libeXosip2
        PATHS ${EXOSIP2_ROOT}
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH
    )
    
    find_library(OSIP2_LIBRARY
        NAMES osip2 libosip2
        PATHS ${EXOSIP2_ROOT}
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH
    )
    
    find_library(OSIPPARSER2_LIBRARY
        NAMES osipparser2 libosipparser2
        PATHS ${EXOSIP2_ROOT}
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH
    )
    
    if(EXOSIP2_INCLUDE_DIR AND OSIP2_INCLUDE_DIR AND 
       EXOSIP2_LIBRARY AND OSIP2_LIBRARY AND OSIPPARSER2_LIBRARY)
        set(EXOSIP2_FOUND TRUE)
        get_filename_component(EXOSIP2_INCLUDE_DIR_PARENT ${EXOSIP2_INCLUDE_DIR} DIRECTORY)
        get_filename_component(OSIP2_INCLUDE_DIR_PARENT ${OSIP2_INCLUDE_DIR} DIRECTORY)
        set(EXOSIP2_INCLUDE_DIRS ${EXOSIP2_INCLUDE_DIR_PARENT} ${OSIP2_INCLUDE_DIR_PARENT})
        set(EXOSIP2_LIBRARIES ${EXOSIP2_LIBRARY} ${OSIP2_LIBRARY} ${OSIPPARSER2_LIBRARY})
        message(STATUS "Found eXosip2 at specified path: ${EXOSIP2_ROOT}")
        message(STATUS "  Include dirs: ${EXOSIP2_INCLUDE_DIRS}")
        message(STATUS "  Libraries: ${EXOSIP2_LIBRARIES}")
    endif()
endif()

# 然后尝试通过 pkg-config 查找系统安装的 eXosip2
if(NOT EXOSIP2_FOUND AND NOT EXOSIP2_BUILD_FROM_SOURCE)
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
        # 确保包含目录正确设置
        if(NOT EXOSIP2_INCLUDE_DIRS)
            set(EXOSIP2_INCLUDE_DIRS 
                "${EXOSIP2_INSTALL_DIR}/include"
            )
        endif()
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
        find_program(LIBTOOLIZE_EXECUTABLE libtoolize)
        find_program(MAKE_EXECUTABLE make)
        find_program(GIT_EXECUTABLE git)
        
        # libtool 或 libtoolize 都可以
        if(NOT LIBTOOL_EXECUTABLE AND LIBTOOLIZE_EXECUTABLE)
            set(LIBTOOL_EXECUTABLE ${LIBTOOLIZE_EXECUTABLE})
        endif()
        
        if(NOT AUTOCONF_EXECUTABLE OR NOT AUTOMAKE_EXECUTABLE OR (NOT LIBTOOL_EXECUTABLE AND NOT LIBTOOLIZE_EXECUTABLE))
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
            # 检测是否为交叉编译
            # 方法1: 检查 CMAKE_CROSSCOMPILING
            # 方法2: 检查编译器名称是否包含交叉编译前缀
            set(CROSS_COMPILE_FLAGS "")
            if(CMAKE_CROSSCOMPILING)
                if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
                    set(CROSS_COMPILE_FLAGS "--host=aarch64-linux-gnu")
                else()
                    set(CROSS_COMPILE_FLAGS "--host=${CMAKE_SYSTEM_PROCESSOR}-linux-gnu")
                endif()
            else()
                # 通过检查编译器名称来检测交叉编译
                get_filename_component(COMPILER_NAME ${CMAKE_C_COMPILER} NAME)
                if(COMPILER_NAME MATCHES "^([^-]+)-.*-gcc$")
                    set(COMPILER_PREFIX ${CMAKE_MATCH_1})
                    if(COMPILER_PREFIX STREQUAL "aarch64")
                        set(CROSS_COMPILE_FLAGS "--host=aarch64-linux-gnu")
                    elseif(COMPILER_PREFIX MATCHES "^arm")
                        set(CROSS_COMPILE_FLAGS "--host=${COMPILER_PREFIX}-linux-gnu")
                    elseif(COMPILER_PREFIX MATCHES "^x86_64" OR COMPILER_PREFIX MATCHES "^i[3-6]86")
                        # 本地编译，不需要 --host
                        set(CROSS_COMPILE_FLAGS "")
                    else()
                        # 其他架构，尝试使用编译器前缀
                        set(CROSS_COMPILE_FLAGS "--host=${COMPILER_PREFIX}-linux-gnu")
                    endif()
                endif()
            endif()
            
            # 获取工具链工具路径
            if(CMAKE_AR)
                set(AR_TOOL "${CMAKE_AR}")
            else()
                set(AR_TOOL "ar")
            endif()
            if(CMAKE_RANLIB)
                set(RANLIB_TOOL "${CMAKE_RANLIB}")
            else()
                set(RANLIB_TOOL "ranlib")
            endif()
            if(CMAKE_STRIP)
                set(STRIP_TOOL "${CMAKE_STRIP}")
            else()
                set(STRIP_TOOL "strip")
            endif()
            
            file(WRITE ${OSIP2_CONFIGURE_SCRIPT}
                "#!/bin/sh\n"
                "set -e\n"
                "set -x\n"
                "echo \"=== Configuring osip2 ===\"\n"
                "echo \"Source directory: ${OSIP2_SOURCE_DIR}\"\n"
                "echo \"Install directory: ${EXOSIP2_INSTALL_DIR}\"\n"
                "echo \"Compiler: ${CMAKE_C_COMPILER}\"\n"
                "cd ${OSIP2_SOURCE_DIR}\n"
                "if [ -f autogen.sh ] && [ ! -f configure ]; then\n"
                "  echo \"Running autogen.sh...\"\n"
                "  ./autogen.sh || { echo \"autogen.sh failed, but continuing if configure exists\"; }\n"
                "fi\n"
                "if [ ! -f configure ]; then\n"
                "  echo \"ERROR: configure script not found after autogen.sh\"\n"
                "  exit 1\n"
                "fi\n"
                "export CC=\"${CMAKE_C_COMPILER}\"\n"
                "export CXX=\"${CMAKE_CXX_COMPILER}\"\n"
                "export AR=\"${AR_TOOL}\"\n"
                "export RANLIB=\"${RANLIB_TOOL}\"\n"
                "export STRIP=\"${STRIP_TOOL}\"\n"
                "# 在交叉编译时，可能需要设置一些额外的标志\n"
                "if [ -n \"${CROSS_COMPILE_FLAGS}\" ]; then\n"
                "  export CFLAGS=\"-fPIC\"\n"
                "  export CXXFLAGS=\"-fPIC\"\n"
                "fi\n"
                "echo \"CC=\$CC\"\n"
                "echo \"CXX=\$CXX\"\n"
                "echo \"AR=\$AR\"\n"
                "echo \"CFLAGS=\$CFLAGS\"\n"
                "echo \"CXXFLAGS=\$CXXFLAGS\"\n"
                "echo \"Running configure...\"\n"
                "./configure --prefix=${EXOSIP2_INSTALL_DIR} --enable-static --disable-shared ${CROSS_COMPILE_FLAGS} CC=\"${CMAKE_C_COMPILER}\" CXX=\"${CMAKE_CXX_COMPILER}\" AR=\"${AR_TOOL}\" RANLIB=\"${RANLIB_TOOL}\" STRIP=\"${STRIP_TOOL}\" || { echo \"configure failed\"; cat config.log 2>/dev/null || true; exit 1; }\n"
                "echo \"=== osip2 configuration completed ===\"\n"
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
                BYPRODUCTS
                    "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osip2${CMAKE_STATIC_LIBRARY_SUFFIX}"
                    "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osipparser2${CMAKE_STATIC_LIBRARY_SUFFIX}"
                LOG_DOWNLOAD 1
                LOG_CONFIGURE 1
                LOG_BUILD 1
                LOG_INSTALL 1
            )
            
            # 编译 eXosip2（依赖 osip2）
            # 创建配置脚本
            set(EXOSIP2_CONFIGURE_SCRIPT "${CMAKE_BINARY_DIR}/exosip2-configure.sh")
            # 检测是否为交叉编译（使用与 osip2 相同的逻辑）
            set(EXOSIP2_CROSS_COMPILE_FLAGS "")
            if(CMAKE_CROSSCOMPILING)
                if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
                    set(EXOSIP2_CROSS_COMPILE_FLAGS "--host=aarch64-linux-gnu")
                else()
                    set(EXOSIP2_CROSS_COMPILE_FLAGS "--host=${CMAKE_SYSTEM_PROCESSOR}-linux-gnu")
                endif()
            else()
                # 通过检查编译器名称来检测交叉编译
                get_filename_component(COMPILER_NAME ${CMAKE_C_COMPILER} NAME)
                if(COMPILER_NAME MATCHES "^([^-]+)-.*-gcc$")
                    set(COMPILER_PREFIX ${CMAKE_MATCH_1})
                    if(COMPILER_PREFIX STREQUAL "aarch64")
                        set(EXOSIP2_CROSS_COMPILE_FLAGS "--host=aarch64-linux-gnu")
                    elseif(COMPILER_PREFIX MATCHES "^arm")
                        set(EXOSIP2_CROSS_COMPILE_FLAGS "--host=${COMPILER_PREFIX}-linux-gnu")
                    elseif(COMPILER_PREFIX MATCHES "^x86_64" OR COMPILER_PREFIX MATCHES "^i[3-6]86")
                        # 本地编译，不需要 --host
                        set(EXOSIP2_CROSS_COMPILE_FLAGS "")
                    else()
                        # 其他架构，尝试使用编译器前缀
                        set(EXOSIP2_CROSS_COMPILE_FLAGS "--host=${COMPILER_PREFIX}-linux-gnu")
                    endif()
                endif()
            endif()
            
            # 尝试获取 vcpkg 安装目录（用于 OpenSSL）
            set(VCPKG_INSTALL_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed")
            if(DEFINED VCPKG_TARGET_TRIPLET)
                set(VCPKG_TRIPLET_DIR "${VCPKG_INSTALL_DIR}/${VCPKG_TARGET_TRIPLET}")
            else()
                # 尝试推断 triplet
                if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
                    set(VCPKG_TRIPLET_DIR "${VCPKG_INSTALL_DIR}/arm64-linux")
                else()
                    set(VCPKG_TRIPLET_DIR "${VCPKG_INSTALL_DIR}/x64-linux")
                endif()
            endif()
            
            # 设置 OpenSSL 路径（如果存在）
            set(OPENSSL_LIB_DIR "")
            set(OPENSSL_INCLUDE_DIR "")
            set(OPENSSL_PKG_CONFIG_PATH "")
            if(EXISTS "${VCPKG_TRIPLET_DIR}/lib")
                set(OPENSSL_LIB_DIR "${VCPKG_TRIPLET_DIR}/lib")
            endif()
            if(EXISTS "${VCPKG_TRIPLET_DIR}/include")
                set(OPENSSL_INCLUDE_DIR "${VCPKG_TRIPLET_DIR}/include")
            endif()
            if(EXISTS "${VCPKG_TRIPLET_DIR}/lib/pkgconfig")
                set(OPENSSL_PKG_CONFIG_PATH "${VCPKG_TRIPLET_DIR}/lib/pkgconfig")
            endif()
            
            # 获取工具链工具路径（用于 eXosip2）
            if(CMAKE_AR)
                set(AR_TOOL "${CMAKE_AR}")
            else()
                set(AR_TOOL "ar")
            endif()
            if(CMAKE_RANLIB)
                set(RANLIB_TOOL "${CMAKE_RANLIB}")
            else()
                set(RANLIB_TOOL "ranlib")
            endif()
            if(CMAKE_STRIP)
                set(STRIP_TOOL "${CMAKE_STRIP}")
            else()
                set(STRIP_TOOL "strip")
            endif()
            
            file(WRITE ${EXOSIP2_CONFIGURE_SCRIPT}
                "#!/bin/sh\n"
                "set -e\n"
                "set -x\n"
                "echo \"=== Configuring eXosip2 ===\"\n"
                "echo \"Source directory: ${EXOSIP2_SOURCE_DIR}\"\n"
                "echo \"Install directory: ${EXOSIP2_INSTALL_DIR}\"\n"
                "echo \"Compiler: ${CMAKE_C_COMPILER}\"\n"
                "cd ${EXOSIP2_SOURCE_DIR}\n"
                "if [ -f autogen.sh ] && [ ! -f configure ]; then\n"
                "  echo \"Running autogen.sh...\"\n"
                "  ./autogen.sh || { echo \"autogen.sh failed, but continuing if configure exists\"; }\n"
                "fi\n"
                "if [ ! -f configure ]; then\n"
                "  echo \"ERROR: configure script not found after autogen.sh\"\n"
                "  exit 1\n"
                "fi\n"
                "export CC=\"${CMAKE_C_COMPILER}\"\n"
                "export CXX=\"${CMAKE_CXX_COMPILER}\"\n"
                "export AR=\"${AR_TOOL}\"\n"
                "export RANLIB=\"${RANLIB_TOOL}\"\n"
                "export STRIP=\"${STRIP_TOOL}\"\n"
                "export PKG_CONFIG_PATH=${EXOSIP2_INSTALL_DIR}/lib/pkgconfig:\${PKG_CONFIG_PATH}\n"
                "echo \"CC=\$CC\"\n"
                "echo \"CXX=\$CXX\"\n"
                "echo \"AR=\$AR\"\n"
                "echo \"PKG_CONFIG_PATH=\$PKG_CONFIG_PATH\"\n"
            )
            # 添加 OpenSSL 路径（如果存在）
            if(OPENSSL_PKG_CONFIG_PATH)
                file(APPEND ${EXOSIP2_CONFIGURE_SCRIPT}
                    "export PKG_CONFIG_PATH=${OPENSSL_PKG_CONFIG_PATH}:\${PKG_CONFIG_PATH}\n"
                )
            endif()
            if(OPENSSL_LIB_DIR)
                file(APPEND ${EXOSIP2_CONFIGURE_SCRIPT}
                    "export LDFLAGS=\"-L${OPENSSL_LIB_DIR} \${LDFLAGS}\"\n"
                )
            endif()
            # 在 macOS 上，需要链接 Security、CoreFoundation 和 CoreServices 框架
            # Security 和 CoreFoundation 用于 TLS 证书加载
            # CoreServices 包含 Gestalt 函数（虽然已弃用，但 exosip2 仍在使用）
            if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
                file(APPEND ${EXOSIP2_CONFIGURE_SCRIPT}
                    "export LDFLAGS=\"\${LDFLAGS} -framework Security -framework CoreFoundation -framework CoreServices\"\n"
                )
            endif()
            if(OPENSSL_INCLUDE_DIR)
                file(APPEND ${EXOSIP2_CONFIGURE_SCRIPT}
                    "export CPPFLAGS=\"-I${OPENSSL_INCLUDE_DIR} \${CPPFLAGS}\"\n"
                )
            endif()
            # 在 macOS 上，不要直接指定静态库路径，而是通过 -l 选项让链接器查找
            # 这样可以避免在构建静态库时嵌入其他静态库的问题
            if(OPENSSL_LIB_DIR AND NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
                file(APPEND ${EXOSIP2_CONFIGURE_SCRIPT}
                    "export LIBS=\"${OPENSSL_LIB_DIR}/libssl.a ${OPENSSL_LIB_DIR}/libcrypto.a \${LIBS}\"\n"
                )
            elseif(OPENSSL_LIB_DIR AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
                # macOS: 使用 -l 选项而不是完整路径，让链接器在 LDFLAGS 指定的目录中查找
                file(APPEND ${EXOSIP2_CONFIGURE_SCRIPT}
                    "export LIBS=\"-lssl -lcrypto \${LIBS}\"\n"
                )
            endif()
            
            # 构建 configure 命令
            # 注意：eXosip2 的 configure 脚本不支持 --with-openssl，我们通过 PKG_CONFIG_PATH 和 LDFLAGS 来指定 OpenSSL
            set(CONFIGURE_OPTS "--prefix=${EXOSIP2_INSTALL_DIR} --enable-static --disable-shared ${EXOSIP2_CROSS_COMPILE_FLAGS}")
            file(APPEND ${EXOSIP2_CONFIGURE_SCRIPT}
                "echo \"Running configure...\"\n"
                "./configure ${CONFIGURE_OPTS} CC=\"${CMAKE_C_COMPILER}\" CXX=\"${CMAKE_CXX_COMPILER}\" AR=\"${AR_TOOL}\" RANLIB=\"${RANLIB_TOOL}\" STRIP=\"${STRIP_TOOL}\" || { echo \"configure failed\"; cat config.log 2>/dev/null || true; exit 1; }\n"
                "echo \"=== eXosip2 configuration completed ===\"\n"
            )
            file(CHMOD ${EXOSIP2_CONFIGURE_SCRIPT} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
            
            # 创建构建脚本
            set(EXOSIP2_BUILD_SCRIPT "${CMAKE_BINARY_DIR}/exosip2-build.sh")
            file(WRITE ${EXOSIP2_BUILD_SCRIPT}
                "#!/bin/sh\n"
                "cd ${EXOSIP2_SOURCE_DIR}\n"
                "${MAKE_EXECUTABLE} -j${CMAKE_BUILD_PARALLEL_LEVEL}\n"
            )
            file(CHMOD ${EXOSIP2_BUILD_SCRIPT} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
            
            # 创建安装脚本
            set(EXOSIP2_INSTALL_SCRIPT "${CMAKE_BINARY_DIR}/exosip2-install.sh")
            file(WRITE ${EXOSIP2_INSTALL_SCRIPT}
                "#!/bin/sh\n"
                "cd ${EXOSIP2_SOURCE_DIR}\n"
                "${MAKE_EXECUTABLE} install\n"
            )
            file(CHMOD ${EXOSIP2_INSTALL_SCRIPT} FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
            
            ExternalProject_Add(exosip2
                URL ${EXOSIP2_URLS}
                DOWNLOAD_NAME libexosip2-${EXOSIP2_VERSION}.tar.gz
                SOURCE_DIR ${EXOSIP2_SOURCE_DIR}
                DEPENDS osip2
                CONFIGURE_COMMAND ${SH_EXECUTABLE} ${EXOSIP2_CONFIGURE_SCRIPT}
                BUILD_COMMAND ${SH_EXECUTABLE} ${EXOSIP2_BUILD_SCRIPT}
                BUILD_IN_SOURCE 1
                INSTALL_COMMAND ${SH_EXECUTABLE} ${EXOSIP2_INSTALL_SCRIPT}
                UPDATE_COMMAND ""
                BYPRODUCTS
                    "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}eXosip2${CMAKE_STATIC_LIBRARY_SUFFIX}"
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
    # 创建接口库来包装 ExternalProject 生成的库
    if(NOT TARGET exosip2::exosip2)
        # 定义库文件路径
        set(EXOSIP2_LIB_FILE "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}eXosip2${CMAKE_STATIC_LIBRARY_SUFFIX}")
        set(OSIP2_LIB_FILE "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osip2${CMAKE_STATIC_LIBRARY_SUFFIX}")
        set(OSIPPARSER2_LIB_FILE "${EXOSIP2_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}osipparser2${CMAKE_STATIC_LIBRARY_SUFFIX}")
        
        # 创建 IMPORTED 静态库目标
        add_library(exosip2_imported STATIC IMPORTED)
        set_target_properties(exosip2_imported PROPERTIES
            IMPORTED_LOCATION ${EXOSIP2_LIB_FILE}
            INTERFACE_INCLUDE_DIRECTORIES "${EXOSIP2_INSTALL_DIR}/include"
        )
        add_dependencies(exosip2_imported exosip2_libs_ready)
        
        add_library(osip2_imported STATIC IMPORTED)
        set_target_properties(osip2_imported PROPERTIES
            IMPORTED_LOCATION ${OSIP2_LIB_FILE}
            INTERFACE_INCLUDE_DIRECTORIES "${EXOSIP2_INSTALL_DIR}/include"
        )
        add_dependencies(osip2_imported exosip2_libs_ready)
        
        add_library(osipparser2_imported STATIC IMPORTED)
        set_target_properties(osipparser2_imported PROPERTIES
            IMPORTED_LOCATION ${OSIPPARSER2_LIB_FILE}
            INTERFACE_INCLUDE_DIRECTORIES "${EXOSIP2_INSTALL_DIR}/include"
        )
        add_dependencies(osipparser2_imported exosip2_libs_ready)
        
        # 创建一个接口库来组合所有库
        add_library(exosip2_interface INTERFACE)
        target_link_libraries(exosip2_interface INTERFACE
            exosip2_imported
            osip2_imported
            osipparser2_imported
        )
        target_include_directories(exosip2_interface INTERFACE "${EXOSIP2_INSTALL_DIR}/include")
        
        # 创建别名目标
        add_library(exosip2::exosip2 ALIAS exosip2_interface)
        add_library(osip2::osip2 ALIAS exosip2_interface)
        add_library(osipparser2::osipparser2 ALIAS exosip2_interface)
    endif()
endif()

find_package_handle_standard_args(Exosip2
    FOUND_VAR EXOSIP2_FOUND
    REQUIRED_VARS EXOSIP2_INCLUDE_DIRS EXOSIP2_LIBRARIES
)

