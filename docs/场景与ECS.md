# 场景与 ECS

Concord Flash 最核心的设计决定：**一个 Scene 就是一个 ECS World**。
面向对象的生成语法和数据导向的查询语法是同一份存储的两个视图，
永远不会出现"改了一边另一边看不见"的问题——这是相对第一代最大的改进
（第一代的 `Scene::Spawn` 和 `Ecs::World` 是两套互不相关的状态）。

## 两种写法，一份数据

```cpp
// 面向对象视图：生成一个原型，再链式挂自定义组件。
scene.Spawn<Concord::Object::Box>({
         .transform = {.position = {-4.0f, 1.0f, -2.0f}},
         .material  = {.albedo = COLOR_RGB(224, 64, 64)},
     })
    .Add<Spin>({.degreesPerSecond = 60.0f});

// 数据导向视图：逐个组件拼装一个实体，完全不经过任何原型。
scene.CreateEntity()
    .Add<Concord::Transform>({.position = {4.0f, 1.5f, 1.0f}})
    .Add<Concord::MeshRenderer>({});

// 一次查询同时看到上面两个实体。
scene.Query<Concord::Transform, Concord::MeshRenderer>(
    [](Concord::Entity, Concord::Transform& t, Concord::MeshRenderer& m) { /* ... */ });
```

## Entity 是句柄，不是对象

```cpp
struct Entity {
    u32 index;
    u32 generation;
};
```

真正的数据全部存在按组件类型分开的稀疏集里。销毁一个实体后，它的槽位
会被回收利用并把世代号加一——所以持有旧句柄的代码调用 `Get` 只会拿到
`nullptr`，绝不会读到新占用者的数据。

## EntityHandle：Spawn 的返回值

`Scene::Spawn<T>` 返回 `EntityHandle`，一个对 `Entity` 的轻量封装，
支持链式调用：

```cpp
Concord::EntityHandle box = scene.Spawn<Concord::Object::Box>({});
box.Add<Concord::Name>({"MyBox"})
   .Add<Spin>({.degreesPerSecond = 30.0f});

if (box.IsAlive()) { /* ... */ }
box.Destroy();
```

它不拥有任何数据，只是 `World*` + `Entity` 的组合，复制它是零成本的。

## Query 的迭代规则

```cpp
scene.Query<A, B, C>([](Entity e, A& a, B& b, C& c) { ... });
```

- 迭代由第一个类型 `A` 的稠密数组驱动，其余类型只做"是否拥有"的检查——
  **把最稀有的组件放在第一位**，可以让循环体本身最短。
- **回调函数内不能创建或销毁实体**。结构性变更会让正在遍历的稠密数组
  失效；需要延迟到循环结束后再执行。
- `Get<T>()` 返回的指针只在下一次针对该组件类型的结构性变更之前有效，
  不要长期持有。

## MainCamera 与环境设置

```cpp
Concord::Entity camera = scene.MainCamera();  // priority 最低的已生成摄像机
scene.SetEnvironment({.skyColor = COLOR_RGB(38, 48, 66)});
```

场景不强制要求恰好一个摄像机；`MainCamera()` 在没有摄像机时返回
`kInvalidEntity`，渲染器据此决定是否可以画出有效的一帧。
