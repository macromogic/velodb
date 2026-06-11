FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

ARG INSTALL_AZ_DCAP_CLIENT=0
ARG INTEL_SGX_SDK_URL=https://download.01.org/intel-sgx/sgx-linux/2.24/distro/ubuntu20.04-server/sgx_linux_x64_sdk_2.24.100.3.bin

SHELL ["/bin/bash", "-lc"]

RUN apt-get update && apt-get install -y --no-install-recommends \
    apt-transport-https \
    autoconf \
    automake \
    build-essential \
    ca-certificates \
    cmake \
    curl \
    debhelper \
    file \
    gdb \
    git \
    gnupg \
    less \
    libboost-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libcurl4-openssl-dev \
    libmbedtls-dev \
    libmpich-dev \
    libprotobuf-dev \
    libssl-dev \
    libsystemd0 \
    libtool \
    lsb-release \
    make \
    mpich \
    nasm \
    ocaml \
    ocaml-nox \
    ocamlbuild \
    openssl \
    perl \
    pkg-config \
    protobuf-compiler \
    python-is-python3 \
    reprepro \
    sudo \
    unzip \
    wget && \
    rm -rf /var/lib/apt/lists/*

RUN mkdir -p /etc/apt/keyrings && \
    curl -fsSL https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key \
      | gpg --dearmor >/etc/apt/keyrings/intel-sgx-deb.gpg && \
    echo 'deb [arch=amd64 signed-by=/etc/apt/keyrings/intel-sgx-deb.gpg] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' \
      >/etc/apt/sources.list.d/intel-sgx.list && \
    curl -fsSL https://packages.microsoft.com/keys/microsoft.asc \
      | gpg --dearmor >/etc/apt/keyrings/microsoft.gpg && \
    echo 'deb [arch=amd64 signed-by=/etc/apt/keyrings/microsoft.gpg] https://packages.microsoft.com/ubuntu/20.04/prod focal main' \
      >/etc/apt/sources.list.d/msprod.list

RUN apt-get update && apt-get install -y --no-install-recommends \
    libsgx-dcap-default-qpl-dev \
    libsgx-dcap-ql \
    libsgx-dcap-ql-dev \
    libsgx-dcap-quote-verify \
    libsgx-enclave-common \
    libsgx-enclave-common-dev \
    libsgx-epid \
    libsgx-launch \
    libsgx-quote-ex \
    libsgx-urts \
    open-enclave \
    sgx-aesm-service && \
    if [[ "${INSTALL_AZ_DCAP_CLIENT}" == "1" ]]; then \
      apt-get install -y --no-install-recommends az-dcap-client; \
    fi && \
    rm -rf /var/lib/apt/lists/*

RUN curl -fsSL "${INTEL_SGX_SDK_URL}" -o /tmp/sgx_linux_x64_sdk.bin && \
    chmod +x /tmp/sgx_linux_x64_sdk.bin && \
    printf 'no\n/opt/intel\n' | /tmp/sgx_linux_x64_sdk.bin && \
    rm -f /tmp/sgx_linux_x64_sdk.bin

RUN if ! grep -q openenclaverc /root/.bashrc; then \
      echo 'source /opt/openenclave/share/openenclave/openenclaverc' >>/root/.bashrc; \
    fi && \
    if ! grep -q '/opt/intel/sgxsdk/environment' /root/.bashrc; then \
      echo 'source /opt/intel/sgxsdk/environment' >>/root/.bashrc; \
    fi

COPY container-entrypoint.sh /usr/local/bin/container-entrypoint.sh
RUN chmod +x /usr/local/bin/container-entrypoint.sh

WORKDIR /root/obliviator

ENTRYPOINT ["/usr/local/bin/container-entrypoint.sh"]
CMD ["/bin/bash"]
