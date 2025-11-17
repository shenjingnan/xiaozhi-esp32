# 面包板紧凑型WiFi舵机控制板

## 概述

这是一个基于面包板紧凑型WiFi设计的舵机控制板，支持通过语音指令控制SG90舵机的角度和运动。

## 硬件配置

硬件配置和小智 AI 的面包接线方式完全相同。 可以参考 [小智 AI 聊天机器人面包板 DIY 硬件清单与接线教程](https://ccnphfhqs21z.feishu.cn/wiki/EH6wwrgvNiU7aykr7HgclP09nCh)
**唯一区别是需要连接一个 SG90 舵机**

舵机一般有三根线：

| 颜色   | 含义           |
| ------ | -------------- |
| 橙色   | 表示信号线     |
| 红色   | 表示电源正极线 |
| 咖啡色 | 表示电源负极线 |

舵机和开发板的接线方式如下

| ESP32S3 开发板 | SG90 舵机    |
| -------------- | ------------ |
| GPIO18         | 橙色信号线   |
| 3V3 电源       | 红色正极线   |
| GND            | 咖啡色负极线 |

### MCP 工具列表

- `self.servo.set_angle` - 设置舵机角度
- `self.servo.rotate_clockwise` - 顺时针旋转
- `self.servo.rotate_counterclockwise` - 逆时针旋转
- `self.servo.get_position` - 获取当前位置
- `self.servo.stop` - 停止舵机
- `self.servo.sweep` - 扫描模式
- `self.servo.reset` - 复位到中心位置

## 编译和烧录

编译和烧录有两种方式，一种是直接编译出固件(一般在发布阶段使用)，一种是手动编译和烧录（一般在开发阶段使用）

### 方式一：直接编译出固件

**第一步：设置 ESP-IDF 环境**:

```bash
source /path/to/esp-idf/export.sh
```

**第二步：编译固件**:

```bash
python scripts/release.py bread-compact-wifi-with-servo
```

提示：生成的固件文件在: `releases/v1.8.1_bread-compact-wifi-with-servo.zip`

### 方式二：手动编译和烧录

**第一步：设置 ESP-IDF 环境**:

```bash
source /path/to/esp-idf/export.sh
```

**第二步：设置芯片**

```bash
idf.py set-target esp32s3
```

**第三步：设置开发板**

```bash
idf.py menuconfig

# 然后依次选择：Xiaozhi Assistant -> Board Type -> 面包板紧凑型WiFi舵机控制板
```

**第三步：编译固件**

```bash
idf.py build
```

**第三步：烧录与监控器**

```bash
idf.py flash monitor
```

### 语音控制

设备配置完成后，可以使用以下语音指令试一下：

- **"设置舵机角度到 90 度"** - 设置舵机到指定角度
- **"顺时针旋转 45 度"** - 顺时针旋转指定角度
- **"逆时针旋转 30 度"** - 逆时针旋转指定角度
- **"开始扫描模式"** - 在 0-180 度范围内来回摆动
- **"停止舵机"** - 立即停止舵机运动
- **"复位舵机"** - 回到中心位置（90 度）
- **"查看舵机状态"** - 获取当前角度和运动状态

## 项目迁移

### ⚠️ 重要提醒

很多用户在将此开发板迁移到其他项目时，发现在 `idf.py menuconfig` 中找不到对应选项。这是因为只复制开发板目录是不够的，还需要手动修改配置文件。

### 迁移概述

要将 `bread-compact-wifi-with-servo` 开发板成功迁移到其他项目，需要修改以下文件：

1. ✅ 复制 `main/boards/bread-compact-wifi-with-servo/` 目录
2. ⚠️ **修改 `main/Kconfig.projbuild`** - 添加开发板选项
3. ⚠️ **修改 `main/CMakeLists.txt`** - 添加构建配置

### 详细迁移步骤

#### 步骤 1：复制开发板目录
```bash
# 将整个开发板目录复制到新项目
cp -r main/boards/bread-compact-wifi-with-servo/ /path/to/your/project/main/boards/
```

#### 步骤 2：修改 `main/Kconfig.projbuild`

在文件中找到其他开发板配置的位置（大约在第438行附近），添加以下内容：

```kconfig
    config BOARD_TYPE_BREAD_COMPACT_WIFI_WITH_SERVO
        bool "Bread Compact ESP32 DevKit With Servo（面包板紧凑型WiFi舵机控制板）"
        depends on IDF_TARGET_ESP32S3
```

**注意：**
- 确保缩进与其他 `config BOARD_TYPE_*` 项保持一致
- 位置要在 `endchoice` 之前
- 语法要严格遵循 Kconfig 格式

#### 步骤 3：修改 `main/CMakeLists.txt`

在文件中找到其他开发板条件判断的位置（大约在第553行附近），添加以下内容：

```cmake
elseif(CONFIG_BOARD_TYPE_BREAD_COMPACT_WIFI_WITH_SERVO)
    set(BOARD_TYPE "bread-compact-wifi-with-servo")
endif()
```

**注意：**
- 确保位置在其他 `elseif(CONFIG_BOARD_TYPE_*)` 项的合适位置
- 语法要正确，特别是 `endif()` 的位置

#### 步骤 4：验证配置

1. **检查 Kconfig 语法**：
```bash
idf.py menuconfig
# 应该能在 Xiaozhi Assistant -> Board Type 中找到 "面包板紧凑型WiFi舵机控制板"
```

2. **检查 CMake 语法**：
```bash
idf.py reconfigure
# 应该没有配置错误
```

#### 步骤 5：测试编译和烧录

```bash
idf.py build flash monitor
```

### 常见问题和解决方案

#### 问题 1：menuconfig 中找不到开发板选项

**可能原因：**
- `main/Kconfig.projbuild` 语法错误
- 缩进不正确
- `depends on IDF_TARGET_ESP32S3` 条件不满足

**解决方法：**
1. 检查 Kconfig 语法，确保与其他 `config` 项格式一致
2. 确认当前目标是 `esp32s3`：
   ```bash
   idf.py set-target esp32s3
   ```
3. 检查文件保存和编码格式

#### 问题 2：编译时找不到开发板源文件

**可能原因：**
- `main/CMakeLists.txt` 条件判断错误
- 开发板目录路径不正确
- 缺少必要的源文件

**解决方法：**
1. 检查 `CMakeLists.txt` 中的条件判断语法
2. 确认开发板目录存在且包含正确的源文件：
   ```bash
   ls main/boards/bread-compact-wifi-with-servo/
   # 应该看到 .cc 和 .c 文件
   ```
3. 检查 `BOARD_TYPE` 变量设置是否正确

#### 问题 3：编译警告或错误

**常见错误：**
- `unknown option "BOARD_TYPE_BREAD_COMPACT_WIFI_WITH_SERVO"`
- `No rule to make target`

**解决方法：**
1. 清理构建缓存：
   ```bash
   idf.py fullclean
   idf.py reconfigure
   ```
2. 检查所有配置文件的语法
3. 确认 ESP-IDF 版本兼容性

### 迁移验证清单

完成迁移后，请确认以下各项：

- [ ] `idf.py menuconfig` 中能看到 "面包板紧凑型WiFi舵机控制板" 选项
- [ ] 选择该选项后能正常保存配置
- [ ] `idf.py build` 编译无错误
- [ ] `idf.py flash` 烧录成功
- [ ] 设备启动正常
- [ ] 语音控制舵机功能正常

### 迁移失败排查

如果迁移后仍有问题，请按以下顺序排查：

1. **检查文件完整性**：确保所有必要的文件都已复制
2. **检查配置语法**：使用 `idf.py menuconfig` 验证 Kconfig 语法
3. **检查构建配置**：查看 `build/config` 目录下的生成文件
4. **查看详细错误**：使用 `idf.py build -v` 获取详细编译信息
5. **对比原始项目**：逐个对比配置文件的差异

> 💡 **提示**：建议在迁移前先备份原始项目，以便对比和回滚。
