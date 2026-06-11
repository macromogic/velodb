# Obliviator Docker Reproduction Scheme

This directory provides a host-preserving reproduction setup for the `obliviator` artifact:

- Host OS stays on Ubuntu 24.04 and keeps its existing TDX/SGX stack.
- The artifact runs inside an Ubuntu 20.04 container so that the legacy `open-enclave` and Intel SGX SDK assumptions remain valid.
- SGX hardware access is passed through from the host using `/dev/sgx_enclave` and `/dev/sgx_provision`.

The design follows current official guidance:

- Intel SGX on modern kernels exposes `/dev/sgx_enclave` and `/dev/sgx_provision`.
- Intel SGX SDK/PSW currently support modern hosts, but the public `open-enclave` package flow still aligns with Ubuntu 20.04.

## What This Setup Does

- Builds an Ubuntu 20.04 image with:
  - Open Enclave SDK from the Microsoft `focal` repository
  - Intel SGX runtime packages from the Intel `focal` repository
  - Intel SGX SDK installed under `/opt/intel/sgxsdk`
  - Build dependencies for both the Open Enclave code and the `join_kks_*` Intel SGX code
- Mounts the artifact into `/root/obliviator` so existing scripts using `~/obliviator/...` keep working.
- Creates compatibility symlinks at container startup:
  - `/root/Parallel-join -> /root/obliviator`
  - `/root/obliviator/join_kks -> /root/obliviator/join_kks_INT_INT` by default

## Host Preconditions

On the host, verify:

```bash
ls -l /dev/sgx_enclave /dev/sgx_provision
```

If you want AESM-backed quoting services available inside the container, also check whether one of these sockets exists:

```bash
ls -l /var/run/aesmd/aesm.socket
ls -l /run/aesmd/aesm.socket
```

The host should keep responsibility for the kernel SGX driver and any host-side attestation daemons. The container only provides the user-space toolchain.

## Build And Run

From the artifact root:

```bash
cd /home/wh25/gputee-odb/obliviator
chmod +x docker/run-container.sh
./docker/run-container.sh
```

This script:

- builds `obliviator-ae:ubuntu20.04`
- mounts the current artifact tree into the container
- forwards SGX devices if they exist
- forwards AESM sockets if they exist

To use the `INT_STR` KKS variant instead of the default `INT_INT` variant:

```bash
OBLIVIATOR_JOIN_KKS_VARIANT=join_kks_INT_STR ./docker/run-container.sh
```

To enable Azure-specific `az-dcap-client` during image build:

```bash
INSTALL_AZ_DCAP_CLIENT=1 ./docker/run-container.sh
```

This is intentionally off by default because it is Azure-specific and not required for every reproduction path.

## Optional Docker Compose

You can also use Compose:

```bash
cd /home/wh25/gputee-odb/obliviator/docker
docker compose run --rm obliviator
```

Notes:

- `compose.yaml` assumes `/dev/sgx_enclave`, `/dev/sgx_provision`, and `/var/run/aesmd` exist.
- If your host lacks any of them, remove the corresponding `devices` or `volumes` entries before using Compose.
- The shell-script launcher is more forgiving because it only forwards resources that actually exist.

## Inside The Container

Once inside:

```bash
pkg-config --exists oehost-gcc oeenclave-gcc && echo "Open Enclave OK"
test -d /opt/intel/sgxsdk && echo "Intel SGX SDK OK"
```

Recommended first checks:

```bash
cd ~/obliviator
./scripts/ae-basic-test.sh
```

## Expected Boundaries

This scheme is designed for artifact compilation and functional reproduction with minimal host disruption.

It does **not** guarantee paper-exact hardware-mode performance, because that still depends on:

- the host SGX firmware and BIOS state
- EPC size
- host-side attestation services
- exact machine shape used by the paper

It also does not fix every artifact issue automatically. In particular:

- the artifact scripts still carry some historical assumptions
- `join_kks` is provided via a symlink to one of the two variants
- the Intel SGX comparison path may still need manual validation depending on whether you want simulation-only checks or full hardware execution

## Why This Is Safer Than Modifying The Host

- No host downgrade from Ubuntu 24.04 to 20.04
- No direct conflict with the host's TDX-oriented package stack
- No need to install `focal` Open Enclave packages into the host package database
- Easy to remove, rebuild, or iterate without touching host system packages
