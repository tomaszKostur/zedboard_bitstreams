# 0 500 9c0 e80 1340 mwr 0x4e200010 0x10005580


proc data_init {} {
	mwr 0x11000000 0xaabbccdd
	mwr 0x11000004 0xffeeddcc
	mrd 0x11000008 3
	mrd 0x1100000c 4
}

#central dma base addres                0x4E200000
#matcher base addres from central dma   0x76000000



proc d_dma {} {
	mrd 0x4E200000 11
}

proc dma_sg_test {} {
#dane do przekopiowania
        data_init

# teraz deskryptory
	mwr 0x10000000 0x10000040
	mwr 0x10000008 0x11000000
	mwr 0x10000010 0x76000000
	mwr 0x10000018 4

	mwr 0x10000040 0x10000080
	mwr 0x10000048 0x11000004
	mwr 0x10000050 0x76000004
	mwr 0x10000058 4

#dwa zapisane do targetu to teraz odczytajmy
	mwr 0x10000080 0x100000c0
	mwr 0x10000088 0x76000000
	mwr 0x10000090 0x11000000
	mwr 0x10000098 4
# jeszcze raz zapiszmy
        mwr 0x100000c0 0x10000100
        mwr 0x100000c8 0x11000080
        mwr 0x100000d0 0x76000000
        mwr 0x100000d8 4
#
#        mwr 0x10000100 0x10000000
#        mwr 0x10000108 0x11000040
#        mwr 0x10000110 0x76000004
#        mwr 0x10000118 4



# ok no to terz zapiszmy cdma controll register i inne
	mwr 0x4E200000 0x00010002
	mwr 0x4E200000 0x0001000A
	mwr 0x4E200008 0x10000000
	mwr 0x4E200010 0x100000c0

}

proc d_reset {} {
	rst
	ps7_init
	ps7_post_config
	fpga -f match_3x3.bit
#	clear
#	desc_clear
}


proc desc_clear {} {
	for {set i 0x10000000} {$i < 0x10000200} {incr i 4} {
		mwr $i 0
	}
}


proc refr {} {
	source ioc_xmd.tcl
}

