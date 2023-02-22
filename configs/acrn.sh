qemu-system-x86_64 \
	-name acrn \
	-drive file=/usr/share/OVMF/OVMF_CODE.fd,if=pflash,format=raw,unit=0,readonly=on \
	-smbios type=0,uefi=on	\
	-cpu host,+invtsc,-vmx-apicv-vid	\
	-rtc base=localtime,clock=host	\
	-machine q35,usb=off,vmport=off	\
	-accel kvm,kernel-irqchip=split \
	-smp 6 \
	-m 10240 \
	-drive if=none,file=ubuntu-22.04-minimal-cloudimg-amd64.qcow2,format=qcow2,id=rootfs	\
	-device virtio-blk-pci,drive=rootfs,bootindex=1	\
	-netdev bridge,id=net0,br=acrn-br0 \
	-device virtio-net-pci,netdev=net0,id=net0,mac=52:54:00:c9:2d:4f \
	-object rng-random,filename=/dev/urandom,id=rng0 \
	-device virtio-rng-pci,rng=rng0	\
	-serial stdio	\
	-device virtio-vga-gl,xres=1920,yres=1080	\
	-device qemu-xhci,id=usb0	\
	-device usb-kbd	\
	-device usb-mouse	\
	-device usb-tablet	\
	-device intel-iommu,intremap=on,caching-mode=on,aw-bits=48	\
	-drive if=none,file=vda.qcow2,format=qcow2,id=vda	\
	-device virtio-blk-pci,drive=vda,bootindex=2	\
	-drive if=none,file=vdb.qcow2,format=qcow2,id=vdb	\
	-device virtio-blk-pci,drive=vdb,bootindex=3	\
	-drive if=none,file=vdc.qcow2,format=qcow2,id=vdc	\
	-device virtio-blk-pci,drive=vdc,bootindex=4	\
	-drive if=none,file=vdd.qcow2,format=qcow2,id=vdd	\
	-device virtio-blk-pci,drive=vdd,bootindex=5	\
	-device virtio-balloon-pci,id=balloon0	\
	-fsdev local,path=/home/me,security_model=none,id=fsdev0	\
	-device virtio-9p-pci,fsdev=fsdev0,mount_tag=share	\
	-display egl-headless,gl=on	\
	-gdb tcp::1234	\
	-monitor telnet::1235,server,nowait	\
	-vnc 0.0.0.0:100

# host cpu: i5-1135G7

	#-cpu Snowridge,kvmclock=off,+invtsc,+lm,+nx,+smep,+smap,+mtrr,+clflushopt,+vmx,+x2apic,+popcnt,-xsave,+sse,+rdrand,-vmx-apicv-vid,+vmx-apicv-xapic,+vmx-apicv-x2apic,+vmx-flexpriority,+tsc-deadline,+pdpe1gb	\
	#-chardev stdio,id=serialport0,signal=on	\
	#-device virtio-serial-pci	\
	#-device virtserialport,chardev=serialport0	\
# viommu !!!
	#-device isa-serial,id=console0	\
	#-chardev stdio,id=console0,signal=on	\
	#-append "root='PARTUUID=4bf52287-ced3-440a-be4e-d367f63af27f' ro debug loglevel=8 console=ttyS0"	\
	#-drive file=/usr/share/OVMF/OVMF_CODE.fd,if=pflash,format=raw,unit=0,readonly=on \
	#-drive file=OVMF_VARS.fd,if=pflash,format=raw,unit=1 \
