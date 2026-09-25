# Neon Courier · 霓虹信使

使用 `concordc --init games/NeonCourier` 创建的 ConcordScript 小游戏。
玩法、场景和界面全部写在 `.cx` 中，CMake 自动转译并链接 Concord Flash SDK。

## 试玩

双击 `Play.cmd`，或直接运行 `build-cli/game.exe`。需要 Windows x64 和支持 Vulkan 的显卡。

你是青色信使：75 秒内收集全部 12 枚金色晶体，避开红色追击者。共有 3 格护盾。

| 按键 | 操作 |
| --- | --- |
| Enter | 开始 / 继续 |
| WASD / 方向键 | 移动 |
| 空格 | 朝当前方向冲刺，期间免疫碰撞；冷却 1.5 秒 |
| P / Escape | 暂停 / 继续 |
| R | 重新开始 |
| Alt+F4 | 退出 |

收集晶体可缩短冲刺冷却。先沿外围收集，再绕回中心，留好冲刺穿过追击者。

## 源码与构建

- `Main.cx`：游戏入口、键盘输入、主循环。
- `Rules.cx`：移动、冲刺、追击、碰撞、计分、胜负和规则自测。
- `Arena.cx`：场景几何、动画、HUD 与开始 / 暂停 / 结算界面。

在本工作区使用 UCRT64 MinGW、CMake、Ninja 和本地预览 SDK。修改源码前先关闭运行中的游戏，然后执行：

```powershell
concord build .
./Play.cmd
```

CLI 自动下载并缓存 SDK 与编译工具。生成的 C++ 与运行依赖均在 `build-cli/`。

已验证：CLI 项目初始化测试、三份 `.cx` 转译与 Release 构建、规则自测、隐藏窗口运行 90 帧以及可见窗口画面检查。
