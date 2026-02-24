#!/bin/bash

# 检查是否为 root 用户，如果不是则使用 sudo
SUDO=''
if (( $EUID != 0 )); then
    SUDO='sudo'
fi

# 获取系统发行版信息
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$ID
    OS_LIKE=$ID_LIKE
else
    echo "无法检测到操作系统发行版。"
    exit 1
fi

echo "检测到操作系统: $PRETTY_NAME"

# Debian 系 (Ubuntu, Debian 等)
if [[ "$OS" == "ubuntu" || "$OS" == "debian" || "$OS_LIKE" == *"debian"* ]]; then
    echo "正在为 Debian/Ubuntu 系安装依赖..."
    $SUDO apt-get update
    if ! $SUDO apt-get install -y \
        gcc g++ cmake make \
        clang llvm \
        python3 python3-pip python3-venv \
        libprotobuf-dev protobuf-compiler \
        libgrpc++-dev protobuf-compiler-grpc \
        bpftool libbpf-dev \
        linux-headers-$(uname -r) \
        mysql-server default-libmysqlclient-dev default-mysql-client; then
        echo "依赖安装失败，请检查网络或软件源配置。"
        exit 1
    fi

# 红帽系 (CentOS, RHEL, Fedora, Rocky, AlmaLinux 等)
elif [[ "$OS" == "fedora" || "$OS" == "centos" || "$OS" == "rhel" || "$OS" == "rocky" || "$OS" == "almalinux" || "$OS_LIKE" == *"rhel"* || "$OS_LIKE" == *"fedora"* ]]; then
    echo "正在为 RedHat/Fedora/CentOS 系安装依赖..."
    
    # 优先使用 dnf，如果没有则使用 yum
    if command -v dnf >/dev/null 2>&1; then
        PKG_MANAGER="dnf"
    else
        PKG_MANAGER="yum"
    fi
    
    if [[ "$OS" == "fedora" ]]; then
        if ! $SUDO $PKG_MANAGER install -y \
            gcc-c++ cmake make \
            clang llvm \
            python3 python3-pip \
            protobuf-devel protobuf-compiler \
            grpc-devel grpc-plugins \
            bpftool libbpf libbpf-devel \
            kernel-devel-$(uname -r) \
            mysql-server mysql mysql-devel; then
            echo "依赖安装失败，请检查网络或软件源配置。"
            exit 1
        fi
    else
        # Rocky/RHEL/AlmaLinux/CentOS 需要额外仓库，包名也与 Fedora 有差异
        $SUDO $PKG_MANAGER install -y epel-release || true
        $SUDO $PKG_MANAGER install -y dnf-plugins-core || true
        $SUDO $PKG_MANAGER config-manager --set-enabled crb || \
        $SUDO $PKG_MANAGER config-manager --set-enabled powertools || true

        if ! $SUDO $PKG_MANAGER install -y \
            gcc-c++ cmake make \
            clang llvm \
            python3 python3-pip \
            protobuf-devel protobuf-compiler \
            grpc-devel grpc-plugins \
            bpftool libbpf libbpf-devel \
            kernel-devel-$(uname -r); then
            echo "依赖安装失败，请检查网络或软件源配置。"
            exit 1
        fi

        # 统一安装 MySQL：Rocky 10 常用 mysql8.4-* 命名，其他系统可能是 mysql-*
        if ! $SUDO $PKG_MANAGER install -y --allowerasing \
            mysql8.4-server mysql8.4 mysql8.4-devel; then
            if ! $SUDO $PKG_MANAGER install -y --allowerasing \
                mysql-server mysql mysql-devel; then
                echo "MySQL 依赖安装失败，请检查软件源配置（Rocky 10 通常需要 mysql8.4-* 包）。"
                exit 1
            fi
        fi
    fi

else
    echo "不支持的操作系统发行版: $OS"
    echo "请手动安装所需的依赖包。"
    exit 1
fi


echo "环境依赖安装完成！"