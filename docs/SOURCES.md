# 公开来源与接口核验

查阅日期：2026-10-10。只依据以下厂商公开网页、C++ 标准库及系统 POSIX 接口从零实现；未访问其他项目代码、配置、文档或历史。搜索结果中的第三方封装没有用作实现来源。未下载厂商 SDK、头文件或文档包，未接受厂商协议。

## 已实际查阅的官方资料

- [NET_DVR_Login_V40](https://open.hikvision.com/hardware/definitions/NET_DVR_Login_V40.html)：同步/异步登录、参数结构体名称和失败返回值；不足以确认具体 Linux 二进制布局。
- [NET_DVR_SetDVRMessageCallBack_V31](https://open.hikvision.com/hardware/definitions/NET_DVR_SetDVRMessageCallBack_V31.html)：回调参数、全局注册行为、接收后的返回约定及报警码与载荷类型的对应。页面示例提示回调内不宜保存文件。本项目没有复制示例或解析其中载荷。
- [NET_DVR_SetupAlarmChan_V41](https://open.hikvision.com/hardware/definitions/NET_DVR_SetupAlarmChan_V41.html)：布防参数结构体与句柄失败约定。
- [NET_DVR_SetExceptionCallBack_V30](https://open.hikvision.com/hardware/definitions/NET_DVR_SetExceptionCallBack_V30.html)：Linux 专用参数形式、异常回调参数及异常/重连通知类别（本轮通过检索结果查阅）。
- [NET_DVR_GetLastError](https://open.hikvision.com/hardware/definitions/NET_DVR_GetLastError.html)：错误码读取接口。
- [NET_DVR_Logout](https://open.hikvision.com/hardware/definitions/NET_DVR_Logout.html)：注销接口。

初始化、清理、版本读取和撤防网页未能成功获取；初始化、清理及撤防名称可见于上述回调页的流程示例。`NET_DVR_GetSDKVersion` 仅作为待核验的版本符号列入探测清单；未获得足以核验其 ABI/版本编码的官方资料，未调用它。

## 当前核验结论

符号名称、公开函数形式及流程信息可用于设计隔离的通用层；不能据此证明具体版本的类型宽度、对齐、结构体大小、调用约定或卸载安全性。没有任何厂商 SDK 版本、设备型号或原生事件结构体列入已验证支持列表。合成后端的版本检查仅验证本项目生命周期逻辑。

真实接入前还需要使用者合法取得的、明确许可用途的 Linux x86_64 SDK 包和配套官方头文件，核对精确版本/构建标识与 ABI，确认依赖加载路径、同步登录参数、回调注册、异常处理及撤防/注销/清理后的回调终止保证。届时基于官方头文件实现动态函数绑定，并在版本允许列表和实机验收通过后才开放真实路径；不得手抄或猜测 ABI。

## 依赖与许可状态

运行代码仅使用 C++20 标准库及系统 POSIX 动态加载/文件接口；没有捆绑第三方运行库。CMake、编译器和测试所用 Python 是外部开发工具，不进入源码候选包。本轮 CMake 4.4.4 从公开 PyPI 安装在本地忽略的工具环境中；[CMake 上游许可](https://gitlab.kitware.com/cmake/cmake/-/blob/master/Copyright.txt)和 [Python 上游许可](https://docs.python.org/3/license.html)可供发布前核对。

用户已选择 Apache-2.0，项目包含从 [Apache 官方网站](https://www.apache.org/licenses/LICENSE-2.0.txt)取得的完整 LICENSE；原创源码用 SPDX 标明许可，不加入个人身份或指定具名权利人。厂商资料仍不纳入本项目许可或发布物。厂商公开许可的核对结果见 [SDK 许可记录](SDK-LICENSE.md)。从公开资料重新实现并不自动构成法律上的独立开发认定。
