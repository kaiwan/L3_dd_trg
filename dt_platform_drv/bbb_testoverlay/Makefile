all: bbb_testoverlay

bbb_testoverlay: bbb_testoverlay.dto
	dtc -@ -I dts -O dtb -o BBB-TESTOVERLAY.dtbo bbb_testoverlay.dto
install:
	sudo cp BBB-TESTOVERLAY.dtbo /boot/dtbs/$(uname -r)/overlays
clean:
	rm -f BBB-TESTOVERLAY.dtbo
