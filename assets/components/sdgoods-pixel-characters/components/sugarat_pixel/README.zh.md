# 谷仓像素角色

[English](README.md) | 中文

`sugarat_pixel` 是面向谷仓圆屏徽章 LVGL 8.3.11 的像素角色库，包含 20 个带动画的 32x32 像素角色，每个有 8 种表情。吉祥物、陪伴形象、游戏主角和对话头像都应使用它，不要自行绘制角色或嵌入精灵图片。`sdgoods-ui-watch` 和 `sdgoods-ui-game` 会把它作为依赖安装；也可以单独安装，选择 `target=esp-idf`，目录使用固件工程根目录。在 `main/REQUIRES` 中添加 `sugarat_pixel`，并包含 `sugarat_pixel.h`。

## 显示角色

所有函数都须在 LVGL 线程调用。`sp_avatar(parent, id, scale, mood)` 创建边长为 32 x `scale` 像素的对象（scale 为 1..10），以清晰的像素边缘绘制角色并播放动画。`sp_avatar_set_mood()` 切换表情，`sp_avatar_set_char()` 在保留缩放和表情的同时更换角色，`sp_avatar_set_animated(avatar, false)` 让表情帧静止。与界面库配合时，把它放进 `sw_avatar_frame()`、`sg_avatar_frame()` 或 `sg_dialog_portrait()`；scale 2 适合 72 px 头像框，scale 4 适合 150 px 相框。

```c
lv_obj_t *buddy = sp_avatar(frame, SP_CHAR_CUBEY, 4, SP_MOOD_HAPPY);
lv_obj_center(buddy);
sp_avatar_set_mood(buddy, SP_MOOD_TALK);
```

角色状态保存在其 LVGL user data 中，因此不要对它调用 `sw_set_action()`/`sg_set_action()`，应标记它的相框或其他父对象。每个角色运行一个 100 ms 的 LVGL 定时器，对象删除时一并删除。

## 角色与表情

| 类别 | 角色（`SP_CHAR_*`，显示名） |
| --- | --- |
| 人物 | `BOY` 小宇、`GIRL` 小桃、`ASTRONAUT` 星航员、`WIZARD` 小法师、`NINJA` 影忍 |
| 动物 | `CAT` 比特猫、`SHIBA` 柴柴、`RABBIT` 棉花兔、`PANDA` 团团、`CHICK` 啾啾、`FOX` 小狐、`FROG` 呱呱、`PENGUIN` 企企 |
| 奇幻 | `FAIRY` 花仙子、`GHOST` 飘飘、`SLIME` 软软、`DRAGON` 小龙、`MUSHROOM` 菇菇、`CUBEY` 方糖仔、`CHIPPY` 芯芯 |

表情为 `SP_MOOD_IDLE`（上下浮动并眨眼）、`HAPPY`（跳跃）、`SURPRISED`、`SAD`、`SLEEP`、`LOVE`、`TALK`（嘴巴开合，语音播放时使用）和 `WINK`。`sp_char_get(id)` 返回静态记录，包含编号、显示名、类别、调色板和帧数据，可用于自定义绘制。

## 内存与修改

全部角色以 4 位索引帧存储，约占 95 KB flash；每个角色在 LVGL 堆中增加一个 LVGL 对象、一小块状态和一个定时器。绘制时把相同颜色的连续像素合并为矩形，因此 scale 4 的角色重绘很快，但一个界面上不要放太多动画角色。角色以代码形式定义在 `tools/make_chars.py` 中；增加或修改角色时编辑该文件，并在组件目录运行 `python3 tools/make_chars.py proof.png`（需要 Pillow），它会重写 `src/sp_chars.c` 和 `include/sp_chars.h` 并输出校对图。角色画为原创，采用 Apache-2.0。组件已用 ESP-IDF 5.5.4 在谷仓模板中无警告编译，帧画面已在主机 LVGL 8.3.11 构建上渲染并人工检查；组件尚未在设备上运行。
