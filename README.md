
# tee-fusion (Repo Scaffold)  
**统一可信应用运行时框架 — 支持 TDX / OP-TEE / Keystone**

本仓库提供一个多 TEE 融合运行时的代码骨架，用于验证“**源码级可移植**”的可行性，并为后续“**二进制级可移植**”（如 WebAssembly hostcall）预留扩展点。  

核心思想：  
- 应用程序调用统一 API（`tee_fusion.h`），无需关心底层 TEE 差异。  
- 平台特定的 attestation / 密钥 / OCall 由适配器（adapters/）实现。  
- U-Evidence 在 `core/evidence/` 内构建为 **CBOR（PoC）或 COSE_Sign1（生产）**。  

---

## 目录结构

```
tee-fusion/
├─ include/              # 统一 API 头文件 (T-FAL)
├─ core/                 # 平台无关核心逻辑 (T-SAL)
│   ├─ api/              # API 门面（调用 vtable）
│   ├─ evidence/         # U-Evidence 构建 & claims 映射
│   └─ util/             # 最小 CBOR/COSE 实现 (可换 QCBOR + t_cose)
├─ adapters/             # 平台适配器 (T-SPL)
│   ├─ tdx/              # TDX (CVM, DCAP/QGS)
│   ├─ optee/            # OP-TEE (PSA/FF-A token)
│   └─ keystone/         # Keystone (RISC-V, SM 报告)
├─ examples/ratls/       # RA-TLS 示例
├─ tests/                # 单元 & 集成测试
└─ scripts/              # 辅助脚本 (证书生成等)
```

---

## 环境准备

支持平台：Ubuntu 22.04+ / Debian / 其他类 Linux  

安装依赖：
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config git libssl-dev uuid-dev \
                        python3 python3-pip python3-pyelftools \
                        device-tree-compiler gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
                        gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf
sudo apt-get install -y device-tree-compiler   # 构建 optee_os 需要 dtc
sudo apt-get install -y ninja-build bison flex rsync cpio unzip bc \
                        libglib2.0-dev libpixman-1-dev libfdt-dev zlib1g-dev

# 或者一键安装：
sudo bash scripts/install_deps_ubuntu.sh
```

---

## 构建

```bash
mkdir build && cd build
cmake -DTEE_PLATFORM=tdx ..      # 可选: optee / keystone
make -j$(nproc)
```

生成的二进制：
- `bin/ratls_server`
- `bin/ratls_client`
 - `bin/optee_smoke`（当 `TEE_PLATFORM=optee`）

---

## 自动拉取/编译 OP‑TEE Client（libteec）

当 `-DTEE_PLATFORM=optee` 时，构建系统会：
- 优先查找系统已安装的 `tee_client_api.h` 与 `libteec`；
- 若未找到且启用 `-DOPTEE_CLIENT_AUTO_FETCH=ON`（默认），自动从 Git 拉取并编译 `optee_client`，并将生成的 `include/` 与 `libteec.so` 注入到本工程的编译链路；
- 成功后定义编译宏 `HAVE_TEEC=1`，启用真实 CA↔TA 调用路径；否则退回到 PoC fallback（无 TEEC 也能编译运行）。

示例：
```bash
mkdir build && cd build
cmake -DTEE_PLATFORM=optee -DOPTEE_CLIENT_AUTO_FETCH=ON ..
make -j$(nproc)
```

可选参数：
- `-DOPTEE_CLIENT_GIT_REPO=https://github.com/OP-TEE/optee_client.git`
- `-DOPTEE_CLIENT_GIT_TAG=4.0.0`

说明：自动拉取需要网络访问与 `git`/`make`。若你的环境不具备网络，可手动安装/指定：
- `-DTEEC_INCLUDE_DIR=/path/to/export/include` 以及 `-DTEEC_LIB=/path/to/export/lib/libteec.so`

---

## 自动拉取/编译 OP‑TEE TA（Trusted Application）

为了让示例在 OP‑TEE 平台完整跑通，本仓库内置 TA 源码（`optee/ta/`）并支持以下三种方式构建 TA：

- 使用系统/现有的 TA Dev Kit：
  - 准备好 OP‑TEE OS 导出的 TA Dev Kit（通常在 `optee_os/out/export-ta_arm64`）。
  - 设置变量：`-DOPTEE_TA_DEV_KIT_DIR=/path/to/export-ta_arm64`，可选指定交叉工具链：`-DOPTEE_TA_CROSS_COMPILE=aarch64-linux-gnu-`。
- 自动拉取并编译 OP‑TEE OS 获取 Dev Kit（默认开启）：
  - `-DOPTEE_TA_AUTO_FETCH=ON`（默认 ON），CMake 会通过 `ExternalProject` 拉取 `optee_os` 并调用 `make PLATFORM=<...> CROSS_COMPILE=<...>` 生成 Dev Kit，然后用其构建 `optee/ta`。
  - 可选参数：
    - `-DOPTEE_OS_GIT_REPO=https://github.com/OP-TEE/optee_os.git`
    - `-DOPTEE_OS_GIT_TAG=4.0.0`
    - `-DOPTEE_OS_PLATFORM=vexpress-qemu_armv8a`（与目标板/模拟器匹配）
    - `-DOPTEE_TA_ARCH=arm64`（或 `arm`）
    - `-DOPTEE_TA_CROSS_COMPILE=aarch64-linux-gnu-`
- 直接在 TA 目录手动构建：
  - `make -C optee/ta TA_DEV_KIT_DIR=/path/to/export-ta_arm64 CROSS_COMPILE=aarch64-linux-gnu-`

注意：自动拉取/构建 TA 需要可用的交叉编译工具链（例如 `aarch64-linux-gnu-gcc`）和网络。若环境受限，建议预先准备好 TA Dev Kit 路径并通过 `-DOPTEE_TA_DEV_KIT_DIR` 指定。

---

## 一键准备 OP‑TEE QEMU‑v8 环境（推荐）

用于在新机器上快速拉起完整 OP‑TEE（qemu_v8）开发环境、导出 TA Dev Kit 并构建本仓库的 TA。

1) 一键安装依赖（Ubuntu/Debian）：
```bash
sudo bash scripts/install_deps_ubuntu.sh
```

2) 在构建目录执行一键脚本（需先完成 CMake 配置，见“构建”章节）：
```bash
cd build
make optee_qemu_v8_setup
# 如希望缺包自动安装（ninja/bison/flex/gnutls-dev）：
OPTEE_AUTO_APT=1 make optee_qemu_v8_setup
```

脚本行为：
- 在 `third_party/optee_ws/` 下创建官方工作区（使用 repo + qemu_v8 manifest），稳健同步（单线程、重试、浅克隆）。
- 构建顺序：`toolchains` → `optee-os-devkit` → `buildroot`（默认 `RUST_ENABLE=n`）。
- 自动探测 `optee_os/out/**/export-ta_{arm32,arm64}`，清理旧 `.o/.d`，并使用匹配的交叉工具链重建 `optee/ta/`。
- 输出后续启动 QEMU 与部署/运行说明。

3) 启动 QEMU（另一个终端）：
```bash
make -C third_party/optee_ws/build -f qemu_v8.mk run
```

4) 来宾机验证（可选）：
- 构建来宾机程序：`make build_optee_smoke_guest`（产生 `build/bin/optee_smoke_guest`）
- 将 TA 拷入来宾机 `/lib/optee_armtz/7a9b3b24-3e2f-4d5f-912d-8b7c1355629a.ta`
- 将 `optee_smoke_guest` 拷入来宾机并运行：`./optee_smoke_guest`

提示：主机侧可先运行回退版自检：`make run_optee_smoke`

---

## 示例：OP‑TEE QEMU‑v8 烟雾测试

当以 `-DTEE_PLATFORM=optee` 构建时，将额外生成 `bin/optee_smoke`。

1) 构建（QEMU‑v8，推荐 arm32 TA）
```bash
mkdir build && cd build
cmake -DTEE_PLATFORM=optee \
      -DOPTEE_CLIENT_AUTO_FETCH=ON \
      -DOPTEE_TA_AUTO_FETCH=ON \
      -DOPTEE_OS_PLATFORM=vexpress-qemu_armv8a \
      -DOPTEE_TA_ARCH=arm32 \
      -DOPTEE_TA_CROSS_COMPILE=arm-linux-gnueabihf- \
      -DOPTEE_TA_CROSS_COMPILE32=arm-linux-gnueabihf- \
      -DOPTEE_TA_CROSS_COMPILE64=aarch64-linux-gnu- ..
make -j$(nproc)
```

可选：64 位 TA（若你的环境需要）
```bash
cmake -DTEE_PLATFORM=optee \
      -DOPTEE_CLIENT_AUTO_FETCH=ON \
      -DOPTEE_TA_AUTO_FETCH=ON \
      -DOPTEE_OS_PLATFORM=vexpress-qemu_armv8a \
      -DOPTEE_TA_ARCH=arm64 \
      -DOPTEE_TA_CROSS_COMPILE=aarch64-linux-gnu- \
      -DOPTEE_TA_CROSS_COMPILE32=arm-linux-gnueabihf- \
      -DOPTEE_TA_CROSS_COMPILE64=aarch64-linux-gnu- ..
make -j$(nproc)
```

2) 部署 TA（若希望走真实 CA↔TA 通路）
- 将生成的 TA 复制到来宾机（OP‑TEE OS）目录：
  - arm32: `optee/ta/export-ta_arm32/ta/7a9b3b24-3e2f-4d5f-912d-8b7c1355629a.ta`
  - arm64: `optee/ta/export-ta_arm64/ta/7a9b3b24-3e2f-4d5f-912d-8b7c1355629a.ta`
- 来宾机路径：`/lib/optee_armtz/`
- 确保来宾机已加载 OP‑TEE 驱动并启动 `tee-supplicant`，存在 `/dev/tee*`

说明：即使未部署 TA，本示例也可运行（使用适配器里的回退逻辑），但无法验证真实 TA 通路。

3) 运行测试（主机侧回退路径）
```bash
./bin/optee_smoke
```

输出将包含：
- 随机数；
- 原生 token（若 TA 或环境变量提供）；
- 生成的公钥与签名校验结果（verify=ok）；
- U‑Evidence（CBOR）大小；
- OCall 回显。

---

## 故障排查（QEMU‑v8）

- 缺包：`sudo bash scripts/install_deps_ubuntu.sh`，或设置 `OPTEE_AUTO_APT=1` 后重试 `make optee_qemu_v8_setup`。
- repo 同步失败：配置代理或手动执行 `repo sync -c -j1 --no-clone-bundle --fetch-submodules --fail-fast`。
- 工具链缺失：先执行 `make -C third_party/optee_ws/build -f qemu_v8.mk toolchains`，或安装系统交叉工具链（见依赖脚本）。
- 未找到 TA Dev Kit：执行 `make -C third_party/optee_ws/build -f qemu_v8.mk optee-os-devkit V=1` 并确认 `optee_os/out/**/export-ta_*`。
- TA 头文件指向旧路径：执行 `make -C optee/ta clean` 或删除 `optee/ta/*.o/*.d` 后重建。

4) 在来宾机内运行（真实 TEEC/TA 通路）

- 一键准备 QEMU‑v8 环境并构建 TA：
  - `make optee_qemu_v8_setup`
- 交叉构建来宾机可执行（AArch64）：
  - `make build_optee_smoke_guest`
  - 产物：`build/bin/optee_smoke_guest`
- 启动 QEMU（另一个终端）：
  - `make -C third_party/optee_build run-only`
- 将 TA 与来宾机程序放入来宾机并运行：
  - 将 `optee/ta/export-ta_arm32/ta/<UUID>.ta`（或 arm64）拷贝至来宾机 `/lib/optee_armtz/`
  - 将 `build/bin/optee_smoke_guest` 拷贝至来宾机并执行：`./optee_smoke_guest`

提示：optee/build 默认未启用 9p 共享目录。可参考其文档为 QEMU 添加 `-virtfs` 挂载参数，或通过 SSH/scp 将文件传入来宾机。


## 运行示例 (RA-TLS)

1. 生成测试证书：
```bash
../scripts/gencert.sh
```

2. 启动服务端：
```bash
./bin/ratls_server 0.0.0.0 8443 ../server.crt ../server.key
```

3. 启动客户端：
```bash
./bin/ratls_client 127.0.0.1 8443
```

客户端会打印：
- 公钥长度  
- 签名长度  
- U-Evidence 长度  

---

## 快速运行（make 目标）

在构建完成后，可直接通过 `make` 运行常用示例：

- 运行 OP‑TEE 烟雾测试（需 `TEE_PLATFORM=optee` 构建）：
  - `make run_optee_smoke`
- 启动 RA‑TLS 服务端（自动生成自签名证书）：
  - `make run_ratls_server`
- 启动 RA‑TLS 客户端并连接本机 8443：
  - `make run_ratls_client`

说明：上述目标默认使用 `0.0.0.0:8443`（服务端）与 `127.0.0.1:8443`（客户端），证书位于仓库根目录的 `server.crt/server.key`。你也可以直接运行 `bin/` 下的程序并自定义参数。

---

## 输入原生证明 (可选)

示例支持通过环境变量传入原生 Quote/Token/Report 文件（PoC：直接打包进 U-Evidence 的 `native_quote` 字段）。

```bash
export TDX_QUOTE_FILE=/path/to/tdquote.bin
export OPTEE_TOKEN_FILE=/path/to/psa_token.bin
export KEYSTONE_REPORT_FILE=/path/to/keystone_report.bin
```

---

## 本地 PoC 流程

为了快速验证无需真实 TEE 环境：  

1. 不设置任何 `*_FILE` 环境变量，适配器会返回一个空的“原生证明”。  
2. 服务端仍然会生成 **U-Evidence (CBOR)**，其中包含：
   - profile / hw.model / isolation.class  
   - 随机生成的 nonce  
   - 会话绑定的公钥  
   - 占位的 measurement  
   - native_quote = 空  
3. 客户端会打印收到的长度，证明通路正常。  

这样你可以在无 TEE 硬件的 PC 上完成端到端验证，后续再接入真实 attestation 流程。  

---

## 测试

构建后运行：
```bash
ctest
```

目前包括最小 CBOR 确定性测试。

---

## 需要你补充的部分

- `adapters/tdx/attest_tdx.c`  
  调用 **QGS/DCAP** 获取 TDQUOTE，解析 measurement/SVN/debug。  
- `adapters/optee/attest_optee.c`  
  在 **TA** 内获取 PSA/FF-A Token，解析 measurement/生命周期。  
- `adapters/keystone/attest_keystone.c`  
  调用 keystone driver 获取 **SM 报告**，填充 enclave/SM measurement。  
- `core/util/cose_min.c`  
  替换为 **t_cose** 实现 COSE_Sign1 签名。  
- `verifier/`  
  编写验证器（推荐 Go/Rust），完成证据链和策略校验。  

---

## 扩展路线

- **阶段一**：源码级可移植 —— 同一份应用代码，编译时切换 `-DTEE_PLATFORM`。  
- **阶段二**：二进制级可移植 —— 应用编译为 WebAssembly，TEE runtime 通过 hostcall 暴露 `tee_fusion` API。  

---

## 参考

- Intel TDX: [https://www.intel.com](https://www.intel.com)  
- OP-TEE PSA Token: [https://optee.readthedocs.io](https://optee.readthedocs.io)  
- Keystone: [https://keystone-enclave.org](https://keystone-enclave.org)  
- COSE/CBOR: [RFC 8152](https://datatracker.ietf.org/doc/html/rfc8152), [RFC 7049](https://datatracker.ietf.org/doc/html/rfc7049)  
