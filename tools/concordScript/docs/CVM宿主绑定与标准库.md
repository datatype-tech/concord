# CVM 宿主绑定与标准库

状态：语言核心、标准库、引擎绑定均已实现并接入 JIT/AOT。引擎绑定 **ABI 版本 12**，
语言有 **i64 / f64 / str 三种 ABI 值类型**，以及 **CVM 本地 `struct`**（不过宿主边界）。
一个完整的 .cx 游戏程序可以开窗运行：场景、输入、碰撞、HUD、存档、退出；Jolt 刚体
可通过 `concord_add_physics_system` 或离线的 `concord_physics_step` 步进。Steam Audio
空间声通过 `concord_add_audio_system` 混到立体声设备。

本文记录 ConcordScript 的 LLVM 后端（Concord Visual Machine，CVM）如何从整数表达式求值器变成
能写游戏的脚本语言，以及这套绑定为什么必须长成这样。

## 1. 为什么必须有 C ABI

CVM 把 `@cvm` 块直接降级为 LLVM IR。IR 只能调用 **C ABI**：没有 C++ 类、模板、重载、
异常、对象布局。而 Concord 的公开 API 几乎全是 C++：

    scene.Spawn<Concord::Object::Box>({.transform = {...}});
    game.Systems().Add<Concord::PhysicsSystem>();

要从 IR 调用它，需要知道修饰后的符号名、`this` 指针布局和构造/析构语义 —— 这不是"绑定"，
而是重新实现一遍 C++ ABI。因此**引擎 API 不能直接绑定**，必须先由引擎侧提供一层
`extern "C"` 薄包装（见第 9 节）。这一层属于引擎，不属于编译器。

同一个理由决定了标准库的语言选择：`script/runtime/` 是 **纯 C**，因为同一份实现要同时服务
两条路径 —— JIT 把它绑进进程，AOT 让目标文件按名字链接它。

## 2. 分层

| 层 | 位置 | 职责 |
|---|---|---|
| 语言核心 | `script/src/CvmEmitter.cpp` | 词法与类型化表达式、局部变量、参数、全局变量、分支循环 → LLVM IR |
| 宿主注册表 | `script/src/CvmHostRegistry.cpp` | 唯一决定"哪些 C 符号可被 .cx 调用、签名是什么"的地方 |
| 标准库 | `script/runtime/*.c` | 字符串、容器、线程、互斥量、计数器、系统调用 |
| 引擎绑定 | `concord/src/engine/cvm/*.cpp` | `extern "C"` 包装真正的引擎对象，运行时按名字解析 |

依赖方向单向：语言核心只通过注册表查询签名，注册表只依赖 `CvmRuntime.h` 的函数声明，
标准库不依赖前两者中的任何一个。注册表**刻意不包含任何 LLVM 类型** —— 它用 `CvmType`
描述签名，把"物化成 `llvm::FunctionType`"留给调用方，这样未来一个列表工具、绑定生成器或测试
都能读取它而不必拖进 LLVM。

## 3. 类型

CVM 有三种会穿过宿主 ABI 的值类型，以及一种不过边界的记录类型：

| 类型 | 用途 | 为什么是它 |
|---|---|---|
| `i64` | 句柄、计数、索引、键码、颜色通道、比较结果 | 宿主 ABI 传这些就是传 64 位整数 |
| `f64` | 位置、尺寸、角度、速度、时间、强度 | 物理量；把米在每次调用点四舍五入成毫米，正是让一个绑定变得难用的原因 |
| `str` | 文本 | ABI 层是一个指针，但脚本永远看不到指针：能对它做的只有 stdlib 声明的那些操作 |
| `struct` | 把字段捆在一起的值 | 只存在于 CVM 模块内部：不能作为宿主参数或返回值 |

规则只有五条：

1. **加宽是隐式的**：`i64` 在需要 `f64` 的地方自动转过去。这是无损的，
   而替代方案是每个混了字面量的调用点都写一次强制转换。
2. **收窄是显式的**：`f64` 不会自动变成 `i64`，因为那会静默丢掉小数部分。
   写 `to_i64(x)`。
3. **字符串不参与转换**：既不会变成数字，也不会从数字变来。用 `str_to_int` /
   `str_from_int` 这类函数，它们把"这是一次文本解析/格式化"写在脸上。
4. **函数返回类型是声明的，不是推断的**（`ret="f64"`，默认 `i64`）。
   推断需要预扫描每个函数体；而声明让签名只由 .cx 文件本身决定。
5. **`struct` 不过 ABI**：宿主函数看不到记录，字段必须拆成 i64/f64/str 再传出去。

`to_i64(x)` 与 `to_f64(x)` 是**语言内建**而不是标准库函数：它们编译成一条指令，
而且类型转换本来就该由语言自己负责，不该变成一次函数调用。

字面量里带小数点或指数就是 `f64`，双引号包起来就是 `str`，否则是 `i64`。

## 4. 语言

### 4.1 语句

```cx
let total = 0;                  // i64，从初值推断
let ratio: f64 = 1.5;           // 显式标注
let name: str = "player";       // 文本
total = total + 1;              // 赋值（局部或全局）
total += 1;                     // 复合赋值：+= -= *= /= %=
name += "!";                    // 字符串 += 走 str_concat
if (cond) { ... } else if (c) { ... } else { ... };
while (cond) { ... };
for (let i = 0; i < n; i += 1) { ... };
for (;;) { break; }             // 条件为空即恒真；continue 跳到 step
break;                          // 跳出最近的 while / for
continue;                       // while：下一次判断；for：先跑 step 再判断
print(expr);                    // i64 → 一行 "cvm: N"
print_float(expr);              // f64 → 一行 "cvm: N"，最多六位有效数字
print_str(expr);                // str → 一行文本
return expr;                    // 每个 @cvm 函数都必须以某个 return 收尾
```

判真规则是"非零即真"，NaN 判为假，空字符串指针判为假。`if` 的两个分支都 `return` 时
不会生成汇合块 —— 一个没有终结指令的基本块在 LLVM 里是非法的。

### 4.2 表达式

`+ - * / %`、一元 `-` 与 `!`、比较 `< <= > >= == !=`、短路 `&&` `||`、三元 `cond ? a : b`
（右结合，两臂按数值加宽或要求类型一致）、括号、字面量（含 `true` / `false`，即 i64 的 1 / 0）、
函数调用、变量读取。`&&` 与 `||` 会跳过右侧：`0 && mark()` 不会调用 `mark()`。结果是 i64 的 0 或 1。
`!` 按与 `if` 相同的判真规则取反。

二元运算遇到 `f64` 就整体提升到 `f64`：`1 + 0.5` 是 `1.5`。
`%` 对 `f64` 是 `frem`，对 `i64` 是取余（同号规则跟 C 一致）。

**字符串只有 `==`、`!=` 和 `+=`**。相等按内容比较（两个同字节的字面量在读者眼里就是相等
的）。`+=` 编译成 `str_concat`。不提供排序：这里没有排序规则，凭空规定字节序等于对文本许下一个
这一层守不住的承诺。对其它算术是错误而不是隐式拼接。

### 4.3 局部变量与作用域

局部变量是提升到入口块的 `alloca`：声明写在循环体或分支里时，槽位仍然支配循环或分支能到达的
每一次使用。作用域是**函数级**的，同一函数内重复 `let` 同名变量是错误而不是遮蔽。

插入点每次重新计算而不是长期持有，这不是风格问题：锚定在空基本块上的 builder 会存下 `end()`
哨兵，而哨兵不会随基本块填充而移动，于是之后每一个槽位都会落到入口块的终结指令之后 ——
又因为 `BasicBlock::getTerminator()` 只认最后一条指令，整个基本块会被验证器判为"没有终结指令"。

### 4.4 函数、参数与返回类型

```cx
@cvm(name="clamp", ret="f64", args="value: f64, low: f64, high: f64") {
    if (value < low)  { return low;  }
    if (value > high) { return high; }
    return value;
};

@cvm(name="label", ret="str", args="index: i64") {
    return str_concat("node ", str_from_int(index));
};
```

`args=` 里不带 `: 类型` 的参数就是 `i64`。参数被复制进一个可赋值的局部槽位 ——
CVM 没有只读名字的概念。入口函数（默认的 `main`）**不能**有参数，因为它由 JIT 无参调用。

一个裸标识符（后面不跟 `(`）按以下顺序解析：局部变量 → 全局变量 → 本项目的 `@cvm`
函数地址 → 已注册的宿主函数地址。**把函数当值取地址**是回调与线程入口的传递方式：

```cx
concord_set_update(update);              // 每帧回调
let worker = thread_spawn(do_work);      // 无参线程入口
let shared = thread_spawn_arg(bump, counter);  // 带一个 i64 的线程入口
```

**回调的形状会被检查**。地址是个整数，本身不携带任何签名信息；但当实参写成裸函数名时，
编译器知道那是哪个函数，于是 `thread_spawn_arg(worker, 1)` 里的 `worker` 若声明成两个
参数，就是一个编译错误，而不是运行时按错误签名调用后返回一个垃圾值。

未知标识符和未知函数会给出编辑距离最近的名字建议：

    @main: line 4: unknown function 'list_pus'; did you mean 'list_push'?

### 4.5 全局变量

`@cvm_global` 声明模块级变量，全项目共享：

```cx
@cvm_global {
    let title: str = "My Game";
    let speed = 6.0;      // f64，从初值推断
    let ticks = 0;        // i64
    let total: f64 = 0;   // 显式标注，整数字面量也接受
};
```

全局变量不是语法糖，是**必需**的：一个每帧运行的回调无法把状态返回给任何人，而 CVM 没有闭包，
所以跨帧状态只能放在全局里。它同时也是脚本给键码之类常量起名字的地方。

初值只能是字面量：全局在模块加载后、任何函数运行前初始化，此时没有任何东西可供初始化表达式调用。
块内允许注释（会被剥离）。同名全局、全局与函数重名、参数遮蔽全局，都是错误。

### 4.6 注释与字符串转义

`//` 与 `/* */` 在 `@cvm` 和 `@cvm_global` 块内都会被剥离，字符串字面量内容不受影响。

字符串支持的转义只有 `\n`、`\t`、`\r`、`\\`、`\"`；
其它转义是错误而不是"原样保留"，因为 `"C:\\path"` 里的一个笔误不该安静地变成两个字符。
字面量不能包含 NUL。

## 5. 宿主注册表

### 5.1 一个注册项有两个名字

    struct CvmHostSignature {
        const char* name;     // .cx 里写的名字，例如 "list_push"
        const char* symbol;   // 链接名，例如 "cvm_list_push"
        CvmType result;       // i64 / f64 / str / void
        const CvmType* params;
        std::size_t paramCount;
        void* address;        // 引擎绑定的这一项为 null
    };

两个名字是刻意的：`symbol` 是防冲突的 C 链接名，而语言表面应该可读。分开之后，
改一个 .cx 里写什么永远不需要动 C 文件。

注册表分成两张表：

- `HostFunctions()` —— **随编译器链接**的标准库，地址在编译期已知，JIT 直接绑定。
- `HostModuleFunctions()` —— **引擎绑定**，地址为 null，运行时从 `--cvm-host` 指定的
  模块里按名字解析。编译器因此完全不依赖引擎。

### 5.2 回调槽

`CallbackSlots()` 声明"哪些宿主参数接收的是函数地址，以及那个函数必须长什么样"：

    {"cvm_thread_spawn",     0, nullptr,      0, i64}   // i64()
    {"cvm_thread_spawn_arg", 0, {i64},        1, i64}   // i64(i64)
    {"ConcordCvmSetUpdate",  0, {f64},        1, i64}   // i64(f64)

没有这张表，把签名不对的函数传进去就是一个**静默的错误答案** —— 这是实测踩到的：一个两参数的
函数交给 `thread_spawn_arg` 之后，返回值是一串垃圾而不是报错。

### 5.3 声明是按需生成的

宿主函数不是在模块开头批量声明的，而是**第一次被引用时**才 `declare`。所以生成出来的
`.ll` 只包含这个程序真正调用的宿主面。函数参数在 IR 里也是有名字的，所以生成的 `.ll`
读起来像它的源文件：

    define double @clamp(double %value, double %low, double %high) {
    define i64 @update(double %deltaTime) {
    declare i64 @ConcordCvmSceneSpawnBox(i64, double, double, double, double, double, double, i64, i64, i64)
    declare i64 @ConcordCvmRunScene(i64, i64, i64, ptr)

JIT 侧相反：标准库**全部**绑成绝对符号，因为 .cx 也可以取宿主函数的地址；而引擎绑定只绑定
**实际解析成功**的符号，缺失的保持缺失，由 `UnresolvedHostModuleSymbols` 统一报告，
而不是变成一个空指针调用目标。

### 5.4 `print` 家族

`print` 的链接名就是 `print`，没有 `cvm_` 前缀。这是刻意的向后兼容：CVM 一直写
`print(...)`，生成的 IR 一直引用外部符号 `print`，README 也把"AOT 目标文件引用外部
`print`"写成了契约。运行库因此同时导出 `cvm_print` 和 `print`（后者是前者的别名）。

三个打印函数是分开的而不是重载：`print` 一直打印精确整数，一个恰好是整数的浮点值不该在既有
输出里突然长出小数点；而文本本来就不该冒充数字。

## 6. 标准库清单

### 6.1 控制台（3）

| CX 名 | 签名 | 说明 |
|---|---|---|
| `print` | i64 → i64 | 一行 `cvm: N` |
| `print_float` | f64 → f64 | 一行 `cvm: N`，最多六位有效数字 |
| `print_str` | str → str | 一行文本 |

### 6.2 字符串（12）

| CX 名 | 签名 | 说明 |
|---|---|---|
| `str_len` | str → i64 | 字节数 |
| `str_eq` | str,str → i64 | 内容比较，0/1 |
| `str_concat` | str,str → str | 新建，需要 `str_free` |
| `str_free` | str → i64 | 释放拥有的串；对字面量返回 0 |
| `str_find` | str,str → i64 | 字节下标，找不到为 -1 |
| `str_sub` | str,i64,i64 → str | 新建子串 |
| `str_from_int` | i64 → str | 新建，十进制 |
| `str_from_float` | f64 → str | 新建，同 `print_float` 的写法 |
| `str_to_int` | str → i64 | 解析前导整数，没有则 0 |
| `str_to_float` | str → f64 | 解析前导数字，没有则 0.0 |
| `str_char_at` | str,i64 → i64 | 字节值，越界为 -1 |
| `str_len_utf8` | str → i64 | Unicode 标量个数 |
| `str_at_utf8` | str,i64 → i64 | 第 n 个标量的码点，越界为 -1 |
| `str_from_code` | i64 → str | 从码点新建 UTF-8 |
| `str_upper` / `str_lower` | str → str | ASCII 大小写 |

**每个 CVM 字符串前面都有一个所有权字。** 编译器发出的字面量写 0，运行库分配的写 1，
所以 `str_free` 作用在字面量上是空操作而不是崩溃 —— 这是持有字符串的脚本最容易犯的错，
而它是无代价地安全的。字符串是 NUL 结尾的 UTF-8 字节；`str_len` 报字节数，
`str_len_utf8` 报标量个数，两者回答的不是同一个问题。

### 6.3 容器

i64 家族：`list_*`（12 个）、`array_*`（9 个）。
f64 家族：`flist_*`（12 个）、`farray_*`（9 个）。
文本列表：`slist_*`（槽位仍是 64 位指针，读写走字符串所有权）。
四字节槽：`list32_*` / `array32_*`，给必须贴近引擎 f32 精度的数据。
关联容器：`map_*`（i64→i64）、`smap_*`（str→i64）。缺失的键读回 0，与 `list_get`
越界一样，所以 `map_has` / `smap_has` 用来区分「存了 0」和「根本没有」。

**两个 64 位家族共用同一个容器**：一个槽位就是 64 位，f64 也是 64 位，于是 f64 版本只是把位模式搬过去再
搬回来，而不是把增长、越界和句柄逻辑抄一遍。不需要重新解释的那些别名（`*_new`、
`*_free`、`*_size`、`*_clear`、`*_copy`、`*_reverse`）直接指向 i64 符号。
推论值得说明白：list 和 flist 是同一个对象，一边的句柄在另一边也能用 —— 在这里这是特性，因为
两种情况下它就是同样的八个字节。

只有 `flist_index_of` / `flist_contains` 是按**数值**而不是按位模式比较的：
`-0.0` 和 `0.0` 位不同但作者认为它们相等，按位比较会回答一个没人问过的问题。

### 6.4 线程与同步（13）

| CX 名 | 说明 |
|---|---|
| `thread_spawn(entry)` | `entry` 是 `i64()` 的地址 |
| `thread_spawn_arg(entry, arg)` | `entry` 是 `i64(i64)` 的地址，接收 `arg` |
| `thread_join(t)` | 等待并返回入口函数的返回值 |
| `thread_yield()` / `thread_count()` | 让出时间片 / 逻辑处理器数 |
| `mutex_new/free/lock/unlock` | 互斥量，解锁校验持有线程 |
| `counter_new/free/add/get` | 原子计数器 |

### 6.5 系统（7）

`sys_time_ms()`（单调毫秒）、`sys_sleep_ms(ms)`、`sys_cpu_count()`、
`sys_random_seed(s)`、`sys_random_next()`、`sys_random_range(lo, hi)`、
`sys_random_float()`（`[0, 1)`）、`sys_exit(code)`

`file_read(path)` 把整个文件读成 `str`（失败为 null）、`file_write(path, text)` 覆盖写入、
`file_exists(path)`。这是存档和设置够用的文本 I/O，不是任意二进制资源加载。

伪随机用 xorshift64*，实现在本仓库里而不是调用库 `rand()`：这样同一个种子在任何工具链、
JIT 与 AOT 两条路径上都产出同一串数。

### 6.6 数学

游戏要算轨道、瞄准和插值，所以标准库提供一组 `f64` 标量函数。**角度是弧度**；
脚本里用的度写成 `radians(yaw)`。

| CX 名 | 说明 |
|---|---|
| `pi()` / `tau()` | 3.14159… / 2π |
| `sin` `cos` `tan` `asin` `acos` `atan` `atan2` | 与 libm 相同 |
| `sqrt` `pow` `hypot` | `hypot` 是欧几里得长度 |
| `floor` `ceil` `round` `abs` `sign` `fract` | `fract` 是 `x - floor(x)` |
| `min` `max` `clamp` `lerp` | `lerp(a, b, t)` 不钳制 t |
| `radians` / `degrees` | 度 ↔ 弧度 |
| `iabs` `imin` `imax` | 整数对照；`iabs(INT64_MIN)` 饱和到 `INT64_MAX` |

## 7. AOT 链接

`--cvm aot` 产出的目标文件按名字引用符号，宿主链接时需要
`ConcordCvmRuntime`（CMake 目标 `concord_cvm_runtime`），引擎符号则由引擎提供：

```cmake
add_subdirectory(script)
target_link_libraries(my_host PRIVATE concord_cvm_runtime)
```

目标文件由 `getDefaultTargetTriple()` 决定，因此和构建 `cc.exe` 的工具链一致（本机为
MSYS2 UCRT64 / MinGW）。不要跨 MSVC/MinGW 混用。AOT 模式下，程序引用的引擎符号会逐条打印出来，
这就是"宿主还要链接什么"的完整清单。

## 8. 线程与共享数据

线程入口有两种形状：`thread_spawn` 收 `i64()`，`thread_spawn_arg` 收
`i64(i64)` 并把一个值交给它。**没有闭包，也共享不了全局变量** —— 两个线程唯一能共同触及的
东西是一个值，而能触及共享状态的只有句柄。所以"多线程操作同一份数据"的写法是：把 `counter`、`mutex`、`map` 或 `smap` 的句柄传给两个线程。

主循环是单线程的：帧回调、键鼠查询都跑在主线程上，全局变量因此不需要原子性。

## 9. concord 引擎绑定（ABI 版本 12）

| 文件 | 内容 |
|---|---|
| `concord/include/Concord/CCvm.h` | 公开 facade，只含 `#include` |
| `concord/include/engine/cvm/CvmApi.h` | 真实声明：`extern "C"` + `CENGINE_API` |
| `concord/include/engine/cvm/CvmSceneRegistry.h` | 场景句柄表 |
| `concord/include/engine/cvm/CvmEntityRegistry.h` | 实体句柄表 |
| `concord/src/engine/cvm/CvmScene.cpp` | 场景构造、实体编辑、环境 |
| `concord/src/engine/cvm/CvmQuery.cpp` | 实体枚举、名字、Jolt 射线（回退网格 AABB） |
| `concord/src/engine/cvm/CvmRun.cpp` | 版本、帧回调、运行、输入、UI、窗口 |
| `concord/src/engine/cvm/CvmAudio.cpp` | Steam Audio 监听者、空间声、播放 |

这些符号从 `ConcordFlashGameEngineRuntime.dll` 导出（CENGINE_API），**不是** Render.dll，
所以 `Runtime → Render` 的单向依赖没有被破坏，也没有引入任何 `vulkan.h`。

### 9.1 什么过 ABI，什么不过

| 量 | 类型 |
|---|---|
| 场景/实体句柄、轴、键码、像素尺寸 | `int64_t` |
| 位置、尺寸、角度、强度、鼠标位移 | `double` |
| 窗口标题 | `const char*` |

颜色通道保持 `int64_t` 的 0..255：一个通道字节是精确的，脚本没有理由去写 0.878431。

**版本 2 之前的按比例整数化已经删除。** 那时长度按毫米、角度按十分之一度、强度按千分之一传，
因为语言还没有浮点。现在脚本直接写米和度 —— 也就是作者本来就在脑子里用的单位。
引擎内部仍是 f32，收窄只发生在 `CvmScene.cpp` 里一处。

### 9.2 接口清单（ABI 版本 12）

**场景与实体**

| CX 名 | 说明 |
|---|---|
| `concord_version()` | ABI 版本，用于发现两侧漂移 |
| `concord_scene_new()` / `concord_scene_free(s)` | 新建 / 释放（连带注销它的实体句柄） |
| `concord_scene_entity_count(s)` | 存活实体数 |
| `concord_scene_has_camera(s)` | 是否有完整启用的相机 |
| `concord_spawn_camera(s, px,py,pz, tx,ty,tz)` | look-at 相机 |
| `concord_spawn_sun(s, elevDeg, azimDeg, intensity)` | 方向光 |
| `concord_spawn_box(s, px,py,pz, sx,sy,sz, r,g,b)` | 每轴独立尺寸的盒子 |
| `concord_spawn_sphere(s, px,py,pz, diameter, r,g,b)` | 球 |
| `concord_spawn_plane(s, px,py,pz, sx,sz, r,g,b)` | 水平面 |
| `concord_spawn_point_light(s, px,py,pz, r,g,b, intensity, range)` | 点光源 |
| `concord_spawn_spot_light(s, px,py,pz, elev, azim, r,g,b, intensity, range, angle)` | 聚光灯，仰角/方位与方向光相同 |
| `concord_entity_set_position(e, x, y, z)` | 绝对位置 |
| `concord_entity_translate(e, dx, dy, dz)` | 相对移动 —— 逐帧运动真正需要的那个 |
| `concord_entity_set_rotation(e, x, y, z)` | 欧拉角，度 |
| `concord_entity_position(e, axis)` | axis 0/1/2 = X/Y/Z，返回 f64 |
| `concord_entity_rotation(e, axis)` | 同一套轴约定，度 |
| `concord_entity_set_scale(e, x, y, z)` / `concord_entity_scale(e, axis)` | 每轴缩放 |
| `concord_entity_set_visible(e, v)` / `concord_entity_set_shadow(e, v)` | 网格或模型渲染器；都没有则返回 0 |
| `concord_entity_set_color(e, r, g, b)` | 反照率 |
| `concord_entity_set_material(e, metallic, roughness, emissive)` | 金属度 / 粗糙度 / 自发光 |
| `concord_entity_set_fov(e, degrees)` | 相机竖直视角 |
| `concord_entity_destroy(e)` | 从场景移除并注销句柄 |
| `concord_entity_alive(e)` | 句柄是否仍指向活实体 |
| `concord_entity_look_at(e, x, y, z)` | 把局部 -Z 对准世界点；相机跟随用这个 |
| `concord_entity_forward(e, axis)` | 面朝方向，轴约定与 position 相同 |
| `concord_entity_set_size` / `size` | 网格图元的轴向尺寸（不含 scale） |
| `concord_entity_overlaps(a, b)` | 两个网格 AABB 是否相交；没有网格则 0 |
| `concord_entity_distance(a, b)` | 两个实体位置的距离 |
| `concord_scene_camera(s)` | 当前主相机句柄 |
| `concord_light_set_intensity` / `set_color` / `set_angles` | 编辑光源 |
| `concord_entity_color` / `material` / `fov` / `visible` / `shadow` | 对应 setter 的读取 |
| `concord_light_intensity` / `color` / `angle` | 光源读取；angle 的 axis 0 仰角 1 方位 |
| `concord_scene_snapshot` / `snapshot_at` | 枚举活实体（引擎不能返回 CVM list 句柄） |
| `concord_entity_kind` | 1 相机 / 2 网格 / 3 模型 / 4 灯 / 5 粒子 / 0 无 |
| `concord_entity_set_name` / `name` | Name 组件；指针由组件持有 |
| `concord_raycast` / `raycast_distance` / `raycast_point` | 有物理时走 Jolt，否则网格 AABB |

**环境**（每个都是"读出、改一个字段、写回"，所以互不覆盖）

| CX 名 | 说明 |
|---|---|
| `concord_scene_set_sky(s, r, g, b)` | sRGB 天空/清屏色 |
| `concord_scene_set_ambient(s, r, g, b, intensity)` | 环境色与强度 |
| `concord_scene_set_grade(s, exposure, contrast, saturation, vignette, bloom)` | 调色 |
| `concord_scene_set_fog(s, density, heightFalloff, baseHeight, anisotropy)` | 高度雾 |
| `concord_scene_set_clouds(s, coverage, density, altitude)` | 体积云层 |

**帧循环与输入**

| CX 名 | 说明 |
|---|---|
| `concord_set_update(fn)` | 注册每帧回调，返回被替换的那个；传 0 清除。回调返回负数则在本帧结束后退出 |
| `concord_quit()` | 请求离开 `RunScene`；没有正在跑的游戏则返回 0 |
| `concord_run_scene(s, w, h, title)` | 开窗并运行到关闭（阻塞，需要 GPU/显示器） |
| `concord_key_down/pressed/released(key)` | 按住 / 按下那一帧 / 松开那一帧 |
| `concord_mouse_delta(axis)` | 轴 0/1 = X/Y，像素（f64） |
| `concord_mouse_down/pressed/released(button)` | 0 左 … 4 X2 |
| `concord_mouse_position(axis)` / `concord_mouse_wheel(axis)` | 客户区像素 / 滚轮增量 |
| `concord_set_mouse_captured(v)` / `concord_mouse_captured()` | 相对鼠标，用于鼠标视角 |
| `concord_window_width()` / `height()` | 客户区像素；窗外为 0 |
| `concord_set_overlay(v)` / `concord_overlay()` | 调试叠加层 |
| `concord_set_title(text)` / `concord_set_window_mode(m)` / `window_mode()` | 标题；0 窗口 1 无边框 2 独占全屏 |
| `concord_frame_count()` / `concord_delta_time()` / `concord_time()` | 帧序号、上一帧秒数、本局累计秒 |
| `concord_ui_panel` / `ui_label` / `ui_button` | 立即模式 HUD；只在帧回调里有效，按钮点击那一帧返回 1 |

**模型、动画、粒子、水面**

| CX 名 | 说明 |
|---|---|
| `concord_model_load(path, scale)` / `concord_model_free(m)` | 导入；失败返回 0，`concord_last_error()` 给出原因 |
| `concord_spawn_model(s, m, x, y, z)` | 实例化已加载的资产 |
| `concord_model_material_count` / `clip_count` / `skeleton_count` | 资产查询 |
| `concord_model_clip_name` / `clip_duration` / `find_clip` | 片段；找不到名字返回 -1 |
| `concord_model_set_albedo_texture(m, index, path)` | 写在资产上，已生成的实例下一帧看见 |
| `concord_entity_play_animation(e, clip, speed, loop)` | 需要骨架和该片段；没有则返回 0 |
| `concord_entity_stop_animation` / `set_animation_speed` / `animation_time` | 已在播的片段 |
| `concord_spawn_particles` 及 `concord_particles_set_*` | 先生成再配置 |
| `concord_spawn_water` | 水面材质盖到模型网格上 |
| `concord_spawn_ripple` / `concord_spawn_ripple_field` | 一次性涟漪 / 持续扰动 |
| `concord_add_particle_system` / `add_water_ripple_system` / `add_animation_system` | 在 `RunScene` 时登记 |
| `concord_add_day_cycle` / `concord_add_first_person` / `concord_add_physics_system` | 日夜、第一人称与 Jolt |
| `concord_add_audio_system` / `concord_spawn_listener` / `concord_entity_add_listener` | Steam Audio 混音；耳朵默认跟主相机 |
| `concord_spawn_sound` / `concord_entity_play` / `stop` / `set_volume` / `volume` | WAVE 空间声；`loop`/`spatial` 为 0/1 |
| `concord_entity_set_pitch` / `pitch` / `set_range` | 播放速率与衰减距离 |
| `concord_set_fly_mode(v)` | 运行中切换飞/走；没有第一人称控制器时只记下请求 |
| `concord_spawn_static_body` / `spawn_dynamic_box` / `spawn_dynamic_sphere` | 刚体；没有步进时不会动 |
| `concord_entity_add_body` / `set_velocity` / `velocity` / `apply_impulse` | 给已有实体挂体、扔出去 |
| `concord_entity_set_angular_velocity` / `angular_velocity` | 角速度 |
| `concord_entity_add_motor` / `set_wish_velocity` / `wish_velocity` / `grounded` | 行走胶囊 |
| `concord_physics_step(s, dt)` | 离线步进（不需要开窗）；游戏循环里是空操作 |
| `concord_set_gravity` / `concord_gravity` | 世界重力 |
| `concord_overlap_sphere` | 球体重叠；需要已经 Step 过 |

实体句柄记录它来自哪个场景，并且**同时**记住场景句柄和场景指针：句柄会被回收，只比对句柄的话，
一个旧实体可能在新场景里命中一个不属于它的槽位。释放场景会连带注销它的所有实体句柄。

**帧回调必须是 `i64(f64)`**：一个参数、标成 `f64`、没有 `ret=`。
两个部分都是硬性的 —— 引擎就是按这个签名去调用它的。delta 时间必须是浮点，
因为一帧大约是 0.016 秒，整数秒会把每一帧都截断成 0。
返回负数等于调用 `concord_quit()`：这是把 Escape 绑到退出的写法。

**编辑某个实体并不携带的组件会返回 0**，而不是悄悄什么都不做：给盒子设 fov 会失败并报告。

**输入在没有窗口时回答 0**，而不是报错 —— 脚本因此不需要先判断窗口是否存在。

### 9.3 键码

`Concord::KeyCode` 是一个**稠密枚举**，从 `None = 0` 开始按声明顺序递增，
所以键码就是小整数，可以直接写进脚本，也可以用 `@cvm_global` 起名字：

| 键 | 码 | 键 | 码 |
|---|---|---|---|
| A … Z | 1 … 26 | Space | 61 |
| Digit1 … Digit9 | 27 … 35 | Enter | 62 |
| Digit0 | 36 | Escape | 63 |
| F1 … F12 | 37 … 48 | Tab / Backspace | 64 / 65 |
| Up / Down / Left / Right | 49 … 52 | Delete / Insert | 66 / 67 |
| LeftShift / RightShift | 53 / 54 | Home / End | 68 / 69 |
| LeftControl / RightControl | 55 / 56 | PageUp / PageDown | 70 / 71 |
| LeftAlt / RightAlt | 57 / 58 | CapsLock / NumLock | 72 / 73 |

完整取值见 `concord/include/engine/input/KeyCode.h`。

### 9.4 怎么用

```sh
cc --project examples/cvm_game --out examples/cvm_game/generated \
   --cvm-host build/ConcordFlashGameEngineRuntime.dll \
   --cvm-host build/ConcordFlashGameEngineRender.dll
```

**两个 DLL 都要给**，而且原因不是"引擎大"：`--cvm-host` 可以重复，Runtime.dll 导出 C ABI，
而**渲染后端只在 Render.dll 被加载时自注册**（AGENTS.md §12 的强制链接锚点，在普通宿主里由链接器
完成，在这里由 `LoadLibraryEx` 完成）。只给 Runtime 的话，`concord_run_scene` 会开出一个
没有后端的窗口并立刻退出。缺符号时的报错会点名这两个 DLL。

完整可运行版本见 `script/examples/cvm_game/Main.cx`：点击捕获鼠标、WASD 移动、空格射击、左上角分数 HUD、Esc 退出。

### 9.5 路径与依赖加载

加载用 `LoadLibraryExW` + `LOAD_WITH_ALTERED_SEARCH_PATH`，并先把路径转成绝对路径、
改用本地分隔符。三点都是必须的：引擎 DLL 的依赖（`SDL3.dll`、`phonon.dll`）在它自己
的目录里而不是 `cc.exe` 旁边；而 CMake 传过来的是正斜杠路径，直接交给 `LoadLibraryExW`
会以 `ERROR_MOD_NOT_FOUND` 失败，和"依赖缺失"无法区分。

### 9.6 注册表与引擎的漂移

注册表在 `script/` 侧独立声明签名，所以它和 `CvmApi.h` 之间理论上会漂移。
`ConcordCvmVersion` 就是为此存在的：模块加载后会调用它并与编译器期望的版本比对。
只有**导出了版本入口的模块**才会被比对 —— Render.dll 没有这个入口，它的沉默不该被当成不匹配。

## 10. 已知限制

- CVM 不是完整的类型检查器：有 i64/f64/str、加宽规则和本地 struct，但没有指针或泛型。
  记录不能穿过宿主 ABI。
- 容器共用一种 64 位存储时 list 与 flist 是同一个对象。`map` / `smap` 是另外的句柄种类。
- 线程之间不能共享 CVM 全局或局部变量，只能传句柄（第 8 节）。
- 同一容器或同一场景被两个线程并发修改是数据竞争；句柄表本身是安全的，内容不是。
- `list_get` / `map_get` 越界（或缺失）与「存了 0」无法区分；这是把失败值定成 0 的代价，
  所以有 `map_has`。
- `concord_run_scene` 阻塞并在当前进程里开窗，需要 GPU 和显示器，因此不在自动化测试覆盖范围内：
  CI 覆盖的是它之前的全部逻辑，加上 AOT 产物的结构。
- 语言核心之外的一切（`@shader`、`@pass`、类、`@entry`）在 CVM 模式下仍然是错误，
  需要用 `--cpp`。`use std.xxx` 也只在 `--cpp` 路径上生效。
- Concord Flash 的刚体与角色走 Jolt Physics 5.6。Steam Audio 已 vendored，但还没有播放 API，
  CVM 因此也没有 `concord_sound_*`。
- 局部 `let` 是函数级作用域：同一函数里不能第二次 `let` 同名变量，改个名字即可。
