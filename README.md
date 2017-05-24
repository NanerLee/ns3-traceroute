# Implement of Traceroute in ns-3

在 [ns-3](http://www.nsnam.org/) 中实现 Traceroute 应用, 采用 Ipv4RawSocket(原始套接字) 和 ICMP 实现.

## 用法

由于没有学习 **[waf](https://github.com/waf-project/waf)** 编译软件, 所以没有制作为 ns-3 的模块.

将头文件放入 **scratch** 文件中, 在自己的仿真源文件中引入头文件. 使用方法和 ns-3 内置的应用基本类似, 具体如下:
```cpp
Ptr<Traceroute> tr = CreateObject<Traceroute>();  // 创建应用
tr->Setup(dst_nodeid, dst_address);  // 设置目的节点和地址
src_node->AddApplication(tr);  // 向源节点中添加应用
tr->SetStartTime(Seconds(starttime));  // 设置应用开始时间
```
其中:
- `dst_nodeid`的类型为`uint32_t`
- `dst_address`的类型为`Ipv4Address`
- `src_node`的类型为`Node`
- `starttime`的类型为`double`

## 细节

- 发送的探测包为 ICMP ECHO 类型
- Traceroute 的测量跳数不超过 16
- 等待响应报文的时间为 1 秒, 超时时此跳地址表示为`*.*.*.*`
- 超时后不会对此跳再次发送探测报文, 即每一跳只发送一次报文
- 等待响应报文采用轮询机制, 每 0.1 秒查询一次, 共查询 10 次
- 测量跳数超过 16, 或者收到 ICMP REPLY 报文后应用停止, 并输出文件
- 输出的文件名为`Traceroute-srcid-dstid.txt`

## 注意

- 只支持IPv4协议
- 未遵循 ns-3 的代码格式规范
- 提取响应报文中的序号的时候, 如果序号大于10, 可能需要更改代码, 在函数`Receive()`中,
- 本人在测试过程中发现了 ns-3.25 中存在设置 TTL 的 bug(详细见这里 -- [Using Ipv4RawSocket to change IpTtl doesn't work?](https://groups.google.com/forum/?hl=zh-Cn#!topic/ns-3-users/QgEon8y7hnw)), 可以[用 patch 修复](https://www.nsnam.org/bugzilla/show_bug.cgi?id=2500), 或者直接修改文件 **ipv4-raw-socket-imple.cc** 函数 **SendTo()** 中的设置TTL的代码为:
```cpp
   if (IsManualIpTtl() && GetIpTtl() != 0 && !dst.IsMulticast() && !dst.IsBroadcast())
    {
       SocketIpTtlTag tag;
       tag.SetTtl(GetIpTtl());
       p->AddPacketTag(tag);
    }
```
- ns-3 中默认会屏蔽向原始套接字递交 ICMP 报文, 需要手动修改其源码, 在文件 **ipv4-l3-lprotocol.cc** 的 **IpForward()** 函数中, 将以下代码注释掉:
```cpp
ipHeader.GetProtocol () != Icmpv4L4Protocol::PROT_NUMBER &&
```
- ns-3 中的原始套接字不会接收中间节点发送的 ICMP 报文, 需要修改源码, 在文件 **ipv4-raw-socket-imple.cc** 的 **ForwardUp()** 中, 将以下代码注释掉:
```cpp
(m_dst == Ipv4Address::GetAny () || ipHeader.GetSource () == m_dst) &&
```
