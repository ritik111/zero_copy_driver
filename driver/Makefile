obj-m += my_device.o

MY_PWD := $(shell pwd)

KDIR := /lib/modules/$(shell uname -r)/build

all:
	$(MAKE) -C $(KDIR) M=$(MY_PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(MY_PWD) clean
