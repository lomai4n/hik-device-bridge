# v0.1.0-preview.1 — 模拟预发布

这是通用层的预发布，可用于离线构建、合成测试和明确标识的模拟演示。真实 SDK 接入禁用，原生支持列表为空；不适用于真实设备生产接入。

## 已提供

- C++20/CMake 小型库及命令行，固定元数据有界异步队列与丢失/畸形/输出失败计数。
- 通用后端的精确版本比较、模拟登录/布防生命周期及失败回滚、安全退出。
- 权限受控的独立密码文件检查；默认输出不包含报警原文、图片、账号或密码。
- 动态库必要符号检查，ABI/版本未核验时明确拒绝真实运行。

## 验证范围

只在 macOS ARM64 现有环境验证通用层、模拟生命周期、密码文件、并发退出与输出边界。Debug、AddressSanitizer/UndefinedBehaviorSanitizer 和源码包独立 Release 构建的合成测试结果见 [验证记录](VALIDATION.md)。没有做真实厂商 SDK、Linux x86_64 或设备验收；这些结果不代表 SDK 兼容性。

## 下载与许可

附加资产为明确允许清单生成的源码包及 SHA-256 文件，没有厂商 SDK、头文件、配套材料、凭据、现场数据、构建环境或 Git 历史。本项目原创源码和文档采用 [Apache-2.0](../LICENSE)，厂商 SDK 权利另见 [许可核对记录](SDK-LICENSE.md)。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
ctest --test-dir build --output-on-failure
./build/hik-device-bridge simulate --seconds 1 --alias camera-demo
```
