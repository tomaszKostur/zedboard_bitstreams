17.06.15

Załączyłem funkcje z przykładem w main jak działają bitstreamy i ich obsługa

BOOT.bin -y w folderach odpowiadają programom sterowników w folderach i zawierają odpowiednie
bitstreamy





// ************************************************************************** //

bitstream "match_3x3.bit" jest bittstreamem zawierającym tylko bloczek
    matchowania 2-uch masek 3x3.


bitstreamy "disp_3x3_18.bit" oraz "disp_3x3_66.bit" to bitstreamy z bloczkami
    zwracającymi dysparycje odpowiednio szukając w przestrzeni 3x16 pikseli i 
    3x66 pikseli

we wszystkich bitstreamach są po 2 bloczki funkcyjne
    przede wszystkim jest ten dostępny (tylko) poprzez dma
    jest jeszcze alternatywny (robiący to samo) ale dostępny bezposrednio
    z procesora (tj do tego alternatywnego jak i do dma możesz się odniaść
    poprzez mmap na /dev/mem, ten docelowy jest widziany tylko poprzez dma, jak
    byś wpisał te fizyczne adresy do mmap-a to zwróci błąd)

oczywiście każda jedna zmienna zaczyna sie o 4 adresy dalej
tak jak to jest w adresowaniu 32-bitowym czyli np.
0x4aa00000 - pierwszy piksel
0x4aa00004 - drógi piksel
0x4aa00008 - ...
0x4aa0000c - ...
0x4aa00010 - ...

bloczki sa zbudowane w ten sposob, że zapisujesz do nich dane,
        ale jeśli chcesz cokolwiek odzczytać to na każdym odczytanym
        adresie bedziesz mial wynik

//////////////////////////////////////////////////////////////////////////
dla bitstreamów zwracających dysparycje

adresy widziane z pktu widzenia procesora (czyli widziane w mmap /dev/mem)

    axi_cdma_0 (czyli dma):
        baseaddr = 0x4e200000
            o dma bedzie niżej
    
    bloczek dysparycja widziany z procesora:
        baseaddr = 0x4aa00000

            pierwsze 9 zmiennych (bo maska 3x3) to piksele maski z lewego
            obrazka wpisywane od lewej do prawej z góry do dołu
    
            następne 54 (bo 3*18) albo 198 (bo 3x66) piksele przestrzeni
                poszukiwań z prawego obrazka
    
            następnie zmienna określająca pozycje maski (ilość pikseli
                    począwszy od lewej krwaedzi obrazka do pierwszego
                    piksela maski)

            następnie zmienna określająca pozycjeprzestrzeni poszukiwań (ilość
                    pikseli począwszy od lewej krawedzi obrazka aż do
                    pierwszego piksela przestrzeni)

adresy widziane z pktu widzenia cdma (czyli te które morzesz wpisać jako
        source lub destination do cdma)
    
    ram procesora:
        baseaddr   = 0x00000000
        highaddres = 0x1fffffff

    bloczek dysparycja widziany z dma:
        baseaddr = 0x76000000
            zmienne poukładane identycznie jak w tym z procesora

////////////////////////////////////////////////////////////////////////

adresy dla bitstreamu zwracajacego matchowanie 2 masek
praktycznie takie same jak poprzednio


adresy widziane z pktu widzenia procesora (czyli widziane w mmap /dev/mem)

    axi_cdma_0 (czyli dma):
        baseaddr = 0x4e200000
            o dma bedzie niżej
    
    bloczek matchowanie widziany z procesora:
        baseaddr = 0x4aa00000

            pierwsze 9 zmiennych (bo maska 3x3) to piksele maski z lewego
            obrazka wpisywane od lewej do prawej z góry do dołu

            nastepne 9 to maska 3x3 z prawego obrazka
    
adresy widziane z pktu widzenia cdma (czyli te które morzesz wpisać jako
        source lub destination do cdma)
    
    ram procesora:
        baseaddr   = 0x00000000
        highaddres = 0x1fffffff

    bloczek matchowanie widziany z dma:
        baseaddr = 0x76000000
            zmienne poukładane identycznie jak w tym alternatywnym
//////////////////////////////////////////////////////////////////////////

teraz jak ogarnać dma

baseaddr = 0x4e200000

// te sa uzywane do konfiguracji i statusu
    0x4e200000 = CDMA Control
    0x4e200004 = CDMA Status
// te sa używane do docelowego scatter gather mode
    0x4e200008 = Current Descriptor Pointer
    0x4e20000c = Current Descriptor Pointer 64 -> to musi byc 0
    0x4e200010 = Tail Descriptor Pointer
    0x4e200014 = Tail Desc Point 64 -> to też musi byc 0
// te sa używane do normal mode gdzie kazda pojedyncza operacje trzeba 
    inicjowac osobno
    
    0x4e200018 = Source Address
    0x4e20001c = source addres 64
    0x4e200020 = destination address
    0x4e200024 = destination address 64
    0x4e200028 = Bytes to transfer

domyslnie jest ustawione simple mode a wiec na:
0x4e200000 = 0x00010002

w tym mode wpisujesz do source address fizyczny adderss piksela (w ram)
    i morzesz go wyslac do bloczka wpisujac destination address i bytes 
    to transfer. W bytes to transfer wpisuj zawsze 4. z innymi z jakiegos
    powodu potrafi byc problem.

scatter gather mode konfigurujesz
0x4e200000 = 0x0001000a

teraz zamiast wpisywac do source address i dest address mamy deskryptory w 
    ramie procesora

najpierw tworzysz te deskryptory a potem wpisujesz pierwszy do current
    descriptor pointer np.
0x4e200008 = 0x12000000

    i tail descryptor pointer jako fizyczny address ostatniego deskryptora
    z chwila kiedy zostaje przetwarzany ten deskryptor dma nie szuka juz
    nastepnego i zapala sie bit na status register o tym że dma jest w stanie
    iddle ( 2 bit na 0x4e200004 )

    z chwila zapisania wartosci "tail desc" dma rozpoczyna prace


tera najwazniejsze czyli deskryptory

deskryptory robisz oczywiscie w ramie gdzie chcesz wazne zebys zapisał do dma
    te adresy poczatkowy i tail a wiec dajmy np. deskryptory beda sie zaczynac
    od 0x12000000

0x12000000 -> address nastepnego deskryptora np 0x12000040
0x12000004 -> adres nast 64  to b. wazne aby tu bylo 0 a to nie konieczne w ram
0x12000008 -> source address
0x1200000c -> source addr 64. a wiec 0
0x12000010 -> destination address
0x12000014 -> dest 64 a wiec 0
0x12000018 -> bytes to transfer  wpisuj 4 z innymi cos nie dziala
0x1200001c -> status tu tez powinno byc 0 jesli deskryptor byl juz
                to tu bedzie cos innego i na status wyskoczy error

deskryptory musza byc od siebie oddalone minimalnie o 64 adresy a wiec
    jak pierwszy mamy 0x12000000 to nastepny 0x12000040

    np adresy kolejnych deskryptorow:

0x12000000
0x12000040
0x12000080
0x120000c0
0x12000100
...


dokumentacja jak by co jest całkiem przystepna

w google: xillinx central dma pierwszy pdf

wkurzajace jest to że da znakuje te deskryptory, ale byc może można mu to 
    wyłączyć, ogarne to...










