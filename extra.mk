
size:
	python binsize.py

flash: all
	openocd -f cfg/openocd.cfg -c "program build/mgmt.elf verify reset exit"

openocd:
	openocd -f cfg/openocd.cfg

gpio:
	./ChibiOS-Contrib/tools/mx2board.py -m ~/STM32Cube/STM32CubeMX -p ../../hardware/core-hw/cubemx/mgmt-v1/mgmt-v1.ioc
