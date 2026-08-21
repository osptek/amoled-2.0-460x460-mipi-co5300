# Hynitron CST820 Touchscreen Driver

Minimal Linux kernel driver for Hynitron (海栎创) CST820 capacitive touch controller.

本例程来源于鱼鹰光电的工程师的开源分享 [Github.com/osptek](https://github.com/osptek)，欢迎提出改进意见。

---

## 1. 文件说明

| 文件 | 说明 |
|------|------|
| `hynitron_cst820.c` | 驱动源码 |
| `Makefile` | 编译与安装脚本 |
| `cst820-touch.dts` | 设备树 overlay（仅触摸） |
| `README.md` | 本说明文件 |

---

## 2.开发环境要求

本项目基于树莓派 5 (Raspberry Pi 5) 开发与测试，运行环境要求如下：

### 1. 系统与硬件配置
* **硬件平台：** Raspberry Pi 5 (Model B)
* **操作系统：** Raspberry Pi OS / Debian 13 (Trixie) 64-bit (`aarch64`)
* **内核版本：** `Linux 6.18.39+rpt-rpi-2712` 或更高

### 2. 依赖环境安装

在编译项目或内核模块前，请先更新系统软件源并安装基础编译工具与当前内核头文件：

```bash
# 更新软件包列表
sudo apt update

# 安装编译工具链与匹配的内核头文件
sudo apt install build-essential linux-headers-$(uname -r)
```

## 3. 编译驱动


> 创建文件夹和驱动文件
> 
```
mkdir cst820-touch && cd cst820-touch

sudo nano hynitron_cst820.c
sudo nano Makefile
```

> 编译

```
make clean
make
```
成功后会生成 hynitron_cst820.ko


## 4. 安装驱动
# 方法一：临时加载（测试用）

```
sudo insmod ./hynitron_cst820.ko
```

# 方法二：安装到系统（推荐）

```
make install
sudo modprobe hynitron_cst820

```

开机自动加载（可选）：

```
echo "hynitron_cst820" | sudo tee -a /etc/modules
```

## 5. 设备树配置（DTS）

### 5.1 编译 overlay

```
sudo nano cst820-touch.dts
```

```
dtc -@ -I dts -O dtb -o cst820-touch.dtbo cst820-touch.dts
sudo cp cst820-touch.dtbo /boot/firmware/overlays/
```

### 5.2 启用 overlay编辑 /boot/firmware/config.txt，添加：

```
sudo nano /boot/firmware/config.txt
```



```
dtoverlay=cst820-touch
```

然后重启：

```
sudo reboot
```

## 6. 单独测试触摸
### 6.1 确认驱动已加载

```
lsmod | grep cst820
dmesg | grep -i cst820

```

期望看到类似信息：

```
CST820 touchscreen registered (addr 0x5a)
using polling mode (20 ms)
# 或
using interrupt mode (IRQ xx)
```

6.2 确认 I2C 设备存在

```
# Pi5 DSI1 常见总线号为 10 或 11，请根据实际情况修改
sudo i2cdetect -y 10
```

6.3 查看输入设备

```
cat /proc/bus/input/devices
```

找到：

```
N: Name="Hynitron CST820 Touchscreen"
H: Handlers=eventX
```
记下 eventX（例如 event2）。

6.4 使用 evtest 测试（推荐）

```
sudo apt install evtest
sudo evtest
```

选择对应的触摸设备编号，然后用手指点击屏幕。正常时会持续输出坐标：

```
Event: time ..., type 3 (EV_ABS), code 53 (ABS_MT_POSITION_X), value 230
Event: time ..., type 3 (EV_ABS), code 54 (ABS_MT_POSITION_Y), value 180
Event: time ..., type 1 (EV_KEY), code 330 (BTN_TOUCH), value 1
```

卸载驱动

```
sudo rmmod hynitron_cst820
# 或
make uninstall

```