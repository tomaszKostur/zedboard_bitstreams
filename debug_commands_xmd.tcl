# zrobil sie balagan w kodzie wiec napisze od poczatku

# aby skorzystać z cdma in sg nalerzy:
# najpierw utworzyc tablice deskryptorow pamietajac o tym zeby najpierw ja wyzerowac
# nastepnie przełaczyć dma w sg mode
# potem zapisac addres pierwszego deskryptora a potem tail deskryptor
# teraz wlasnie sie przekopiowalo
# zeby dokonac nastepnej operacji sg nalerzy
# odswierzyc tablice deskryptorow poniewaz jest pozaznaczana jako zrobiona w (ostatnich 4 bajtach)
# zdjać interrupt z status register
# teraz morzemy zresetowac sg za pomoca wyjscia ze sg i wejscia zpowrotem
# currdesc pointer oraz taildesc pointer beda zresetowane

# byc morze cyclic mode bedzie jakims wyjsciem z tej niewygodnej sytuacji odswierzania tablicy deskryptorow






#test do tcla
proc debug_test {} {
	mwr 0x10000000 0xaabbccdd
	mwr 0x10000004 0xffeeddcc
	mrd 0x10000000 3
	mrd 0x10000000 4
}

#central dma base addres                0x4E200000
#matcher base addres from central dma   0x76000000
proc simple_dma_test_pt1 {} {
	# test maski
	mwr 0x10000000 5
	mwr 0x10000004 3

	#zadanie dla dma

	#source addres place    0x4E200018
	#dest addres place      0x4E200020
	#number of bytes place  0x4E200028

	mwr 0x4E200018 0x10000000
	mwr 0x4E200020 0x76000000
	mwr 0x4E200028 4


}

proc simple_dma_test_pt2 {} {
	#spowrotem do pamieci

	mwr 0x4E200018 0x76000000
	mwr 0x4E200020 0x11000000
	mwr 0x4E200028 4

	mrd 0x11000000 4
}

proc simple_dma_test_pt3 {} {
	mwr 0x10000000 3
	mwr 0x4E200018 0x10000000
	#zapis do pierwszej komorki drogiej macierzy
	mwr 0x4E200020 0x76000024 
	mwr 0x4E200028 4

}
proc simple_dma_test_pt4 {} {

	mwr 0x4E200018 0x76000000
	mwr 0x4E200020 0x11000000
	mwr 0x4E200028 4

	mrd 0x11000000 4
}

proc in_processor_simple_dma_test {} {
	mwr 0x10000000 0xaabbccdd
	mwr 0x10000004 0xaabbbbbb
	mwr 0x10000008 0xfefefefe

	mwr 0x4E200018 0x10000000
	mwr 0x4E200020 0x11000000
	mwr 0x4E200028 1

	mrd 0x11000000 3
}

#alternative axi matcher visible from ps7 0x4AA00000

proc in_processor_matcher_test {} {
	mwr 0x4aa00000 1
	mwr 0x4aa00004 3
        mwr 0x4aa00024 1

	mrd 0x4aa00000 3
}

proc clear {} {
	mwr 0x10000000 0
	mwr 0x10000004 0
	mwr 0x10000008 0
	mwr 0x1000000C 0
	mwr 0x11000000 0
	mwr 0x11000004 0
	mwr 0x11000008 0
	mwr 0x1100000C 0

# czyszczenie deskryptorów

	
}

# only_axi base address 0x76010000
proc only_axi_dma_test_pt1 {} {
	mwr 0x10000000 1
	mwr 0x10000004 2
	mwr 0x10000008 3
	mwr 0x1000000C 4

	#zadanie dla dma

	#source addres place    0x4E200018
	#dest addres place      0x4E200020
	#number of bytes place  0x4E200028

	mwr 0x4E200018 0x10000000
	mwr 0x4E200020 0x76010000
	mwr 0x4E200028 4
}
proc only_axi_dma_test_pt2 {} {	
	mwr 0x4E200018 0x76010000
	mwr 0x4E200020 0x11000000
	mwr 0x4E200028 4

	mrd 0x11000000 4



}

proc d_dma {} {
	mrd 0x4E200000 11
}



# niech przestrzen deskryptorow bedzie od addresu 0x12000000
# offsety:
# 0x00 Next des pointer
# 0x08 source addres
# 0x10 destination address 
# 0x18 controll ????
# 0x1C status ????
# kazdy nastepny deskryptor co 16 32_bitowy address
# czyli 0x00 0x40 0x80 0xC0
proc dma_sg_test {} {
#dane do przekopiowania
	mwr 0x10000000 10
	mwr 0x10000004 2
	mwr 0x10000008 3
	mwr 0x1000000C 4
# teraz deskryptory
	mwr 0x12000000 0x12000040
	mwr 0x12000008 0x10000000
	mwr 0x12000010 0x76000000
	mwr 0x12000018 4

	mwr 0x12000040 0x12000080
	mwr 0x12000048 0x10000004
	mwr 0x12000050 0x76000004
	mwr 0x12000058 4

#dwa zapisane do targetu to teraz odczytajmy
	mwr 0x12000080 0x12000000
	mwr 0x12000088 0x76000000
	mwr 0x12000090 0x11000000
	mwr 0x12000098 4

# ok no to terz zapiszmy cdma controll register i inne
	mwr 0x4E200000 0x00010002
	mwr 0x4E200000 0x0001000A
	mwr 0x4E200008 0x12000000
	mwr 0x4E200010 0x12000080
#	mwr 0x4E200000 0x

#	mrd 0x11000000 3

}

proc d_reset {} {
	rst
	ps7_init
	ps7_post_config
	fpga -f design_1_wrapper.bit
	clear
	desc_clear
}


# ten test absolutnie dziala, trzeba tylko pamietac ze zanim sie o odpali trzeba zapisac zerami
# cala przestreń adresowa przeznaczono an deskryptory
# poniewaz pamirć ram nie jest domyslnie wypelniona zerami
proc in_processor_sg_test {} {
#dane do przekopiowania
	mwr 0x10000000 1
	mwr 0x10000004 2
	mwr 0x10000008 3
	mwr 0x1000000C 4
# teraz deskryptory
	mwr 0x12000000 0x12000040
	mwr 0x12000008 0x10000000
	mwr 0x12000010 0x11000000
	mwr 0x12000018 4

	mwr 0x12000040 0x12000080
	mwr 0x12000048 0x10000004
	mwr 0x12000050 0x11000004
	mwr 0x12000058 4
	
	mwr 0x12000080 0x120000C0
	mwr 0x12000088 0x10000008
	mwr 0x12000090 0x11000008
	mwr 0x12000098 4

	mwr 0x120000C0 0x12000000
	mwr 0x120000C8 0x1000000C
	mwr 0x120000D0 0x1100000C
	mwr 0x120000D8 4

# ok no to terz zapiszmy cdma controll register i inne
#	mwr 0x4E200028 4
	mwr 0x4E200000 0x00010002
	mwr 0x4E200000 0x0001000A

	mwr 0x4E200008 0x12000000
	mwr 0x4E200010 0x120000C0
#	mwr 0x4E200000 0x

}

proc desc_clear {} {
	for {set i 0x12000000} {$i < 0x12000100} {incr i 4} {
		mwr $i 0
	}
}

proc d_read {} {
	mrd 0x11000000 5
}

proc refr {} {
	source debug_commands_xmd.tcl
}
#######################################################################################################################
# tutaj nieuzywane proce


proc descriptor_init {} {

#dane do przekopiowania
	mwr 0x10000000 1
	mwr 0x10000004 2
	mwr 0x10000008 3
	mwr 0x1000000C 4
# teraz deskryptory
	mwr 0x12000000 0x12000040
	mwr 0x12000008 0x10000000
	mwr 0x12000010 0x11000000
	mwr 0x12000018 4

	mwr 0x12000040 0x12000080
	mwr 0x12000048 0x10000004
	mwr 0x12000050 0x11000004
	mwr 0x12000058 4
	
	mwr 0x12000080 0x120000C0
	mwr 0x12000088 0x10000008
	mwr 0x12000090 0x11000008
	mwr 0x12000098 4

	mwr 0x120000C0 0x12000000
	mwr 0x120000C8 0x1000000C
	mwr 0x120000D0 0x1100000C
	mwr 0x120000D8 4


}


