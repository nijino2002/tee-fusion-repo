
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
sudo apt-get install -y build-essential cmake pkg-config libssl-dev
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
