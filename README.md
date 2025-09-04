
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

3) 运行测试
```bash
./bin/optee_smoke
```

输出将包含：
- 随机数；
- 原生 token（若 TA 或环境变量提供）；
- 生成的公钥与签名校验结果（verify=ok）；
- U‑Evidence（CBOR）大小；
- OCall 回显。


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
