# 1. 准备工作

```
# 更新软件包列表
sudo apt update

# 安装编译工具链与匹配的内核头文件
sudo apt install build-essential linux-headers-$(uname -r)

# 创建文件夹并进入
mkdir co5300-460x460 && cd co5300-460x460
```

# 2. 驱动源码（panel-co5300-460x460.c）

```
sudo nano panel-co5300-460x460.c
```



# 3. Makefile

```
sudo nano Makefile
```



```
obj-m += panel-co5300-460x460.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```

> 编译：

```
make clean
make
```

# 4. 设备树 Overlay（vc4-kms-dsi-co5300-460x460.dts）

```
sudo nano vc4-kms-dsi-co5300-460x460.dts
```



> 编译并安装：

```
dtc -@ -I dts -O dtb -o vc4-kms-dsi-co5300-460x460.dtbo vc4-kms-dsi-co5300-460x460.dts
sudo cp vc4-kms-dsi-co5300-460x460.dtbo /boot/firmware/overlays/
sudo cp panel-co5300-460x460.ko /lib/modules/$(uname -r)/kernel/drivers/gpu/drm/panel/
sudo depmod -a
```

# 5. 启用

> 编辑 /boot/firmware/config.txt，添加：

```
sudo nano  /boot/firmware/config.txt
```



```
# 关闭自动检测，避免和手动 overlay 冲突
display_auto_detect=0

dtoverlay=vc4-kms-v3d

dtoverlay=vc4-kms-dsi-co5300-460x460

# 忽略官方 LCD
ignore_lcd=1
```

> 重启：

```
sudo reboot
```

