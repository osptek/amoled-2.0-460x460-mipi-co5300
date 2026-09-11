obj-m += hynitron_cst820.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	sudo mkdir -p /lib/modules/$(shell uname -r)/extra
	sudo cp hynitron_cst820.ko /lib/modules/$(shell uname -r)/extra/
	sudo depmod -a

uninstall:
	sudo rm -f /lib/modules/$(shell uname -r)/extra/hynitron_cst9217.ko
	sudo depmod -a
