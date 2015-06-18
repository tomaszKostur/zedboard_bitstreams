#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>


// test functions not needed in main program //////////////////////////////////

// simpliest memmory allocation
void devmem_wr_0 (unsigned int physical_address, unsigned int data[] );
// test for number of pointers at /dev/mem
void devmem_multi_place_1_file_test();
///////////////////////////////////////////////////////////////////////////////


// struct for generate two mask descriptors

typedef unsigned int u_int;

typedef struct generator_out
{
    u_int my_addr;
    u_int next_desc_addr;
    u_int source_addr;
    u_int destiny_addr;
    u_int bytes_to_transfer;
} g_out;

typedef struct generator_config
{
    u_int mask_size;
    u_int l_match_space_wide;
    u_int descriptor_start_address;
    u_int l_match_space_start_address;
    u_int r_mask_start_address;
    u_int destination_start_address;
    u_int unit_start_address;

} g_conf;

// gives position of best matching referenced to match space wide
// define STATIC_DESC_ARRAY or DYNAMIC_DESC_ARRRAY
#define DYNAMIC_DESC_ARRRAY
u_int get_best_from_matching(u_int left_match_space[], u_int left_match_cells,
        u_int right_mask[], u_int right_mask_cells, g_conf* config);

// descriptor generator
g_out desc_generator_for_matching_unit(
        g_conf* config, unsigned long iteration, char last_iteration);

// returns pointer to memmory
void* get_mmap_pointer(int devmem_desc,size_t physical_address, size_t mem_size);

// read status register of CDMA
char read_cdma_status(u_int *cdma_pointer, char if_print);


int main(int argc, char *argv[])
{

//    printf("beginning\n");

    u_int mask[3][3] = {{1,2,3},{4,5,6},{7,8,9} };
    u_int match[3][18] = { {88,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,84},
                           {88,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,84},
                           {88,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,84} };

//    printf("before flat\n");

    u_int flattern_mask[3*3];
    u_int flattern_match[3*18];

    int i,j;
    for(i = 0; i < 3; ++i)
    {
        for (j = 0; j < 3; ++j)
        {
            flattern_mask[i*3+j] = mask[i][j];
        }
    }
    for (i=0; i<3; ++i)
    {
        for (j=0; j<18; ++j)
        {
            flattern_match[i*18+j] = match[i][j];
        }
    }



    g_conf config;
    config.mask_size = 3;
    config.l_match_space_wide = 18;
    config.descriptor_start_address = 0x10000000;
    config.l_match_space_start_address = 0x11000000;
    config.r_mask_start_address = 0x11100000;
    config.destination_start_address = 0x11200000;
    config.unit_start_address = 0x76000000;

//    printf("jestem przy funkcji get_best...\n");

    u_int best = get_best_from_matching(flattern_match, 3*18, flattern_mask, 3*3, &config);

    printf("best match position is %d\n", best);

    return best;
}

u_int get_best_from_matching(u_int l_match_space[], u_int left_match_cells,
        u_int r_mask[], u_int right_mask_cells, g_conf *config)
{
#define DESCRIPTOR_CELLS 9
#define UNIT_CDMA_CONFIG_ADDRESS 0x4e200000
#define CDMA_ALLOC_SIZE 28
#define MIN_DESCRIPTORS_OFFS 0x40

    u_int mask_size = config->mask_size;
    u_int l_match_space_wide = config->l_match_space_wide;
    u_int result_vector_cells = l_match_space_wide - (mask_size - 1);
    unsigned long number_of_descriptors =
        // this one for all pixells
        // two masks multiply by number of matches operations
        mask_size * mask_size * 2 * result_vector_cells
        // this one for saving results of matching operations to memory
        + result_vector_cells;

    unsigned long i;

#ifdef STATIC_DESC_ARRAY
    unsigned int desc_array[number_of_descriptors][DESCRIPTOR_CELLS];
#endif

#ifdef DYNAMIC_DESC_ARRRAY
    u_int **desc_array = malloc( sizeof(u_int*) * number_of_descriptors);
    if( desc_array == NULL)
    {
        perror("error allocating **desc_array\n");
        exit(EXIT_FAILURE);
    }
    for( i=0; i < number_of_descriptors; ++i )
    {
        desc_array[i] = malloc( sizeof(u_int) * DESCRIPTOR_CELLS);
        if( desc_array[i] == NULL )
        {
            perror("error allocating desc_array[i]");
            exit(EXIT_FAILURE);
        }
    }
#endif


    for(i=0; i<number_of_descriptors; ++i)
    {
        g_out the_descriptor = desc_generator_for_matching_unit(
                config, i, 0);
        // descriptor demand physical address
        desc_array[i][0] = the_descriptor.my_addr;
        // next descriptor (physical) pointer
        desc_array[i][1] = the_descriptor.next_desc_addr;
        desc_array[i][2] = 0;// used in 64 adress space
        // source address (physical)
        desc_array[i][3] = the_descriptor.source_addr;
        desc_array[i][4] = 0;
        // destination address
        desc_array[i][5] = the_descriptor.destiny_addr;
        desc_array[i][6] = 0;
        // bytes to write (unfortulatelly only 4)
        desc_array[i][7] = the_descriptor.bytes_to_transfer;
        // transfer status
        desc_array[i][8] = 0;
    }
// check descriptors //////////////////////////////////////////////////////////
//
//    while(1)
//    {
//        printf("pokaz deskryptor nr=\n");
//        int command;
//        scanf("%d",&command);
//        if(command >= number_of_descriptors) break;
//
//        printf("my_addr           = %x\n",desc_array[command][0]);
//        printf("next_desc_addr    = %x\n",desc_array[command][1]);
//        printf("source_addr       = %x\n",desc_array[command][3]);
//        printf("destiny_addr      = %x\n",desc_array[command][5]);
//        printf("bytes_to_transfer = %x\n",desc_array[command][7]);
//    }

// get all /dev/mem pointers ///////////////////////////////////////////////////
    int devmem_desc;
    devmem_desc = open("/dev/mem", O_RDWR | O_SYNC);
    if (devmem_desc == -1)
    {
	perror("Error opening /dev/mem for writing");
	exit(EXIT_FAILURE);
    }

    size_t match_space_alloc_size = sizeof(u_int) * mask_size * l_match_space_wide;

    u_int* match_space_mmap_pointer = (u_int*) get_mmap_pointer( devmem_desc,
            config -> l_match_space_start_address, match_space_alloc_size );
//    printf("match_space_mmap_pointer ok\n");

    size_t mask_alloc_size = sizeof(u_int) * mask_size * mask_size;

    u_int *mask_mmap_pointer        = (u_int*) get_mmap_pointer( devmem_desc,
            config -> r_mask_start_address, mask_alloc_size );
//    printf("mask_mmap_pointer ok\n");

    size_t result_alloc_size = sizeof(u_int) * result_vector_cells;

    u_int *result_mmap_pointer      = (u_int*) get_mmap_pointer( devmem_desc,
            config -> destination_start_address, result_alloc_size);
//    printf("result_mmap_pointer ok\n");

    size_t size_for_all_descriptors = sizeof(u_int) * DESCRIPTOR_CELLS
        * number_of_descriptors
        + MIN_DESCRIPTORS_OFFS * number_of_descriptors;

    u_int *descriptors_mmap_pointer = (u_int*) get_mmap_pointer( devmem_desc,
            config -> descriptor_start_address, size_for_all_descriptors );
//    printf("descriptors_mmap_pointer ok\n");

    u_int *cdma_mmap_pointer        = (u_int*) get_mmap_pointer( devmem_desc,
            UNIT_CDMA_CONFIG_ADDRESS, CDMA_ALLOC_SIZE);
//    printf("cdma_mmap_pointer ok\n");


// put arrays into physical memory /////////////////////////////////////////////
    volatile int msync_ind;
    // put left_match_space to physical memory
    for (i=0; i < left_match_cells; ++i)
        match_space_mmap_pointer[i] = l_match_space[i];

//       msync_ind = msync( match_space_mmap_pointer, match_space_alloc_size, MS_SYNC );
//    printf("match_space_2_mmap_pointer ok\n");

    // put right_mask to physical memmory
    for (i=0; i < right_mask_cells; ++i)
        mask_mmap_pointer[i] = r_mask[i];

//    msync_ind = msync( match_space_mmap_pointer, mask_alloc_size, MS_SYNC );
//    printf("mask_2_mmap_pointer ok\n");

//     write descriptors to physical memory ////////////////////////////////////
    for (i=0; i < number_of_descriptors; ++i)
    {
        u_int j;
        for (j=0; j < DESCRIPTOR_CELLS - 1 ; ++j)
        {
            u_int offs = MIN_DESCRIPTORS_OFFS / sizeof(u_int);
//            printf("index= %d\n",i*(DESCRIPTOR_CELLS+offs)+j);
            descriptors_mmap_pointer[i*offs+j] = desc_array[i][j+1];
        }
    }

//    msync_ind = msync( match_space_mmap_pointer, size_for_all_descriptors, MS_SYNC );
//    printf("descriptors_2_mmap_pointer ok\n");

// free desc array if dynamic allocated

#ifdef DYNAMIC_DESC_ARRRAY
    for( i=0; i < number_of_descriptors; ++i)
    {
        free( desc_array[i] );
    }
    free ( desc_array );
#endif


// stop program ////////////////////////////////////////////////////////////////
//    printf("finish now? (y/n) ");
//    char pause;
//    while(1)
//    {
//        scanf("%c",&pause);
//
//        if ( pause == 'y')
//        {
//            exit(EXIT_SUCCESS);
//        }
//        else if( pause == 'n' )
//        {
//            break;
//        }
//    }
// start conciguratiom cdma ////////////////////////////////////////////////////

    {
#define CDMA_OFFS1 18
#define CDMA_OFFS2 1

        u_int last_tail_descriptor = config -> descriptor_start_address;
        cdma_mmap_pointer[0] = cdma_mmap_pointer[0] | (1 << 2); // reset CDMA
//        read_cdma_status(cdma_mmap_pointer,1);
        while(cdma_mmap_pointer[0] != 0x00010002) // everythink is ok witch CDMA
        {
            usleep(1);
        }
        cdma_mmap_pointer[0] =  cdma_mmap_pointer[0] | (1 << 3);
        for ( i=0; i<l_match_space_wide - (mask_size - 1) -1; ++i)
        {
            cdma_mmap_pointer[1] =  cdma_mmap_pointer[1] | (1 << 12);// get off interrupt indicator
            cdma_mmap_pointer[2] = last_tail_descriptor;
            cdma_mmap_pointer[4] = last_tail_descriptor + MIN_DESCRIPTORS_OFFS * CDMA_OFFS1;

            while(cdma_mmap_pointer[1] != 0x0001100a)
            {
                usleep(1);
//                read_cdma_status(cdma_mmap_pointer,1);
            }

            cdma_mmap_pointer[1] = cdma_mmap_pointer[1] | (1 << 12);
            cdma_mmap_pointer[2] = last_tail_descriptor + MIN_DESCRIPTORS_OFFS * CDMA_OFFS1;
            cdma_mmap_pointer[4] = last_tail_descriptor + MIN_DESCRIPTORS_OFFS * (CDMA_OFFS1 + CDMA_OFFS2);

            last_tail_descriptor += MIN_DESCRIPTORS_OFFS * (CDMA_OFFS1 + CDMA_OFFS2);

            while (cdma_mmap_pointer[1] != 0x0001100a)
            {
                usleep(1);
//                read_cdma_status(cdma_mmap_pointer,1);
            }
        }
//        msync_ind = msync( cdma_mmap_pointer, CDMA_ALLOC_SIZE, MS_SYNC );

#undef CDMA_OFFS1
#undef CDMA_OFFS2
    }

// watch cdma snd result status ////////////////////////////////////////////////

//    {
//        char if_end;
//        do
//        {
//            if_end = read_cdma_status(cdma_mmap_pointer,1);
//            sleep(1);
//        }while( if_end == 0 );
//    }

//    read_cdma_status(cdma_mmap_pointer,1);
//    for (i=0; i < result_vector_cells; ++i)
//        printf("%d\n",result_mmap_pointer[i]);
//    read_cdma_status(cdma_mmap_pointer,1);

// get best position from matching /////////////////////////////////////////////

    u_int best_match = 0xffffffff;
    u_int best_position = 0;
    for(i=0; i < result_vector_cells; ++i)
    {
        if( result_mmap_pointer[i] < best_match )
        {
            best_match = result_mmap_pointer[i];
            best_position = i;
        }
    }


// unmap all mmap pointers and close /dev/mem //////////////////////////////////

    int munmap_ind;

    munmap_ind = munmap( match_space_mmap_pointer, match_space_alloc_size );
    if( munmap_ind == -1 )
        perror("error unmapping match_space_mmap_pointer\n");

    munmap_ind = munmap( mask_mmap_pointer, mask_alloc_size );
    if( munmap_ind == -1 )
        perror("error unmapping mask_mmap_pointer\n");

    munmap_ind = munmap( result_mmap_pointer, result_alloc_size );
    if( munmap_ind == -1 )
        perror("error unmapping result_mmap_pointer\n");

    munmap_ind = munmap( descriptors_mmap_pointer, size_for_all_descriptors );
    if( munmap_ind == -1 )
        perror("error unmapping descriptors_mmap_pointer\n");

    close(devmem_desc);

    return best_position;



#undef DESCRIPTOR_CELLS
#undef UNIT_CDMA_CONFIG_ADDRESS
}

char read_cdma_status(u_int *cdma_pointer, char if_print)
{
    u_int cdma_status = cdma_pointer[1];
    u_int a_IRQ_delay     = (cdma_status & 0xff000000) >> 24;
    u_int a_IRQ_threshold = (cdma_status & 0x00ff0000) >> 16;
    char Err_Irq   = (cdma_status & 0x4000) >> 14 ;
    char Dly_Irq   = (cdma_status & 0x2000) >> 13 ;
    char IOC_Irq   = (cdma_status & 0x1000) >> 12 ;
    char SGDecErr  = (cdma_status & 0x400) >> 10 ;
    char SGSlvErr  = (cdma_status & 0x200) >> 9 ;
    char SGIntErr  = (cdma_status & 0x100) >> 8 ;
    char DMADecErr = (cdma_status & 0x40) >> 6 ;
    char DMASlcErr = (cdma_status & 0x20) >> 5 ;
    char DMAIntErr = (cdma_status & 0x10) >> 4 ;
    char SGIncid   = (cdma_status & 0x8) >> 3 ;
    char Idle      = (cdma_status & 0x2) >> 1 ;

    if (if_print)
    {
        printf("a_IRQ_delay     = %d\n",a_IRQ_delay    );
        printf("a_IRQ_threshold = %d\n",a_IRQ_threshold);
        printf("Err_Irq   = %d\n",Err_Irq  );
        printf("Dly_Irq   = %d\n",Dly_Irq  );
        printf("IOC_Irq   = %d\n",IOC_Irq  );
        printf("SGDecErr  = %d\n",SGDecErr );
        printf("SGSlvErr  = %d\n",SGSlvErr );
        printf("SGIntErr  = %d\n",SGIntErr );
        printf("DMADecErr = %d\n",DMADecErr);
        printf("DMASlcErr = %d\n",DMASlcErr);
        printf("DMAIntErr = %d\n",DMAIntErr);
        printf("SGIncid   = %d\n",SGIncid  );
        printf("Idle      = %d\n",Idle     );

        printf("control register = %x\n",cdma_pointer[0]);
        printf("status register  = %x\n",cdma_pointer[1]);
        printf("curr descriptor  = %x\n",cdma_pointer[2]);
        printf("zero             = %x\n",cdma_pointer[3]);
        printf("tail descriptor  = %x\n",cdma_pointer[4]);
//        printf("%x\n",cdma_pointer[5]);
//        printf("%x\n",cdma_pointer[6]);
//        printf("%x\n",cdma_pointer[7]);
//        printf("%x\n",cdma_pointer[8]);
//        printf("%x\n",cdma_pointer[9]);
//        printf("%x\n",cdma_pointer[10]);

        printf("\n ************************** \n");

    }
    return Idle;

}

// designed to return one vector of matching to get one pixel of dissparition
g_out desc_generator_for_matching_unit(g_conf *config, unsigned long iteration, char last_iteration)
{
#define U_INT_S sizeof(u_int)
//    static u_int ret_pixel_counter;
    u_int mask_size = config->mask_size;

    g_out the_descriptor;

    the_descriptor.my_addr = config->descriptor_start_address + iteration * 0x40;
    // circle of descriptors addresses
    if ( last_iteration == 0 )
    {
        the_descriptor.next_desc_addr = config->descriptor_start_address + (iteration + 1) * 0x40;
    }
    else
    {
        the_descriptor.next_desc_addr = config->descriptor_start_address;
    }


    // number of reqired descriptors to get one result pixel
    u_int one_pix_desc_num = mask_size * mask_size * 2 + 1;
    // number of actual return working pixel
    u_int wrk_pxl_num = iteration / one_pix_desc_num;
    // number for actual descriptor for actual working pixel
    u_int wrk_desc_num = iteration % one_pix_desc_num;

    // thanks to this construction the function can generate anyone descriptor
    // independent of previous. all parameters are calculated directly from
    // "iteration" variable
    if(wrk_desc_num < mask_size * mask_size)
    {
        // filling first mask
        u_int row = wrk_desc_num / mask_size;

        the_descriptor.source_addr = config -> l_match_space_start_address
            + (wrk_desc_num % mask_size) * U_INT_S // col offset
            + row * (config -> l_match_space_wide) * U_INT_S // row offset
            + wrk_pxl_num * U_INT_S ; // shift all matrix right

        the_descriptor.destiny_addr = config -> unit_start_address
            + wrk_desc_num * U_INT_S;

    }
    else if (wrk_desc_num < mask_size * mask_size * 2)
    {
        // filling second mask
        u_int row = (wrk_desc_num - (mask_size * mask_size)) / mask_size;

        the_descriptor.source_addr = config -> r_mask_start_address
            + (((wrk_desc_num - mask_size * mask_size)) % mask_size) * U_INT_S // col offset
            + row * mask_size * U_INT_S; // row offset
//            + wrk_pxl_num;// this causes after matching matrix_l witch
//            matrix_r matrix_r is shift left as well as matrix_l;
//            witchout matrix_r is constans

        the_descriptor.destiny_addr = config -> unit_start_address
            + wrk_desc_num * U_INT_S;
    }
    else if (wrk_desc_num == mask_size * mask_size * 2)
    {
        // descriptor of result pixel
        the_descriptor.source_addr = config -> unit_start_address;
        the_descriptor.destiny_addr = config -> destination_start_address
            + wrk_pxl_num * U_INT_S;

    }

    the_descriptor.bytes_to_transfer = 4;

    return the_descriptor;
}


void* get_mmap_pointer(int devmem_desc,size_t physical_address, size_t mem_size)
{
    size_t alloc_mem_size, page_mask, page_size;
    void* mmap_pointer;

    page_size = sysconf( _SC_PAGESIZE );
    alloc_mem_size = (((mem_size / page_size) +1) * page_size);
    // wyrownanie mem_size do wielokrotnosci rozmiaru strony
    page_mask = ( page_size -1 );

    mmap_pointer = mmap( NULL, alloc_mem_size,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            devmem_desc, ( physical_address & ~page_mask ) );
    // if error
    if( mmap_pointer == MAP_FAILED )
    {
        close( devmem_desc );
        perror("Error mmaping /dev/mem");
        exit(EXIT_FAILURE);
    }

    return mmap_pointer;
}

void devmem_multi_place_1_file_test()
{
// nie ma problemu zeby po otwarciu /dev/mem robic wskazniki do wielu
// miejsc w pamieci jednoczesnie
    int data1[9] = {1,2,3,4,5,6,7,8,9};
    int data2[9] = {9,8,9,8,9,8,9,8,9};
    // open /dev/mem as single file
    int devmem_desc = open ("/dev/mem", O_RDWR | O_SYNC );
    if(devmem_desc == -1)
    {
        perror("error opening /dev/mem");
        exit(EXIT_FAILURE);
    }
    // get two pointers
    void *pointer_to_mmap_data1, *pointer_to_mmap_data2;

    pointer_to_mmap_data1 = get_mmap_pointer( devmem_desc, 0x10000000,
            sizeof(data1) );

    pointer_to_mmap_data2 = get_mmap_pointer( devmem_desc, 0x11000000,
            sizeof(data1) );

    // write data to physical addresses
    int i;
    for( i=0; i < (sizeof( data1 ) / sizeof(int) ); ++i)
    {
        ((int *) pointer_to_mmap_data1)[i] = data1[i];
    }

    for( i=0; i< (sizeof( data2 ) / sizeof(int) ); ++i)
    {
        ((int *) pointer_to_mmap_data2)[i] = data2[i];
    }

    // unmap pointers
    int unmap_ret;
    unmap_ret = munmap( pointer_to_mmap_data1, sizeof(data1) );
    if(unmap_ret == -1)
    {
        perror("Error unmapping dp1");
        exit(EXIT_FAILURE);
    }

    unmap_ret = munmap( pointer_to_mmap_data1, sizeof(data1) );
    if(unmap_ret == -1)
    {
        perror("Error unmapping dp2");
        exit(EXIT_FAILURE);
    }

    close(devmem_desc);
    printf("test successfulu\n");
}


void close_devmem(void* mmap_pointer, size_t alloc_mem_size, int *devmem_desc)
{
    int munmap_ret = munmap(mmap_pointer, alloc_mem_size);
    if ( munmap_ret == -1)
    {
	perror("Error un-mmapping the /dev/mem");
        exit(EXIT_FAILURE);
    }

    int close_ret = close( *devmem_desc );
    if(close_ret == -1)
    {
        perror("Error closing /dev/mem");
    }

    printf("/dev/mem unmap and close successful\n");

}

void devmem_wr_0 (unsigned int physical_address, unsigned int data[] )
{
    size_t mem_size = sizeof(data);
    // Open a file for writing.
    int devmem_desc;

    devmem_desc = open("/dev/mem", O_RDWR | O_SYNC);
    if (devmem_desc == -1)
    {
	perror("Error opening /dev/mem for writing");
	exit(EXIT_FAILURE);
    }

    size_t alloc_mem_size, page_mask, page_size;
    void *mmap_pointer, *virt_addr;

    page_size = sysconf (_SC_PAGESIZE);
    alloc_mem_size = (((mem_size / page_size) + 1) * page_size);
    page_mask = (page_size -1);

    mmap_pointer = mmap(NULL, alloc_mem_size, PROT_READ | PROT_WRITE,
            MAP_SHARED,
            devmem_desc, (physical_address & ~page_mask));

    if (mmap_pointer == MAP_FAILED)
    {
	close(devmem_desc);
	perror("Error mmapping /dev/mem");
	exit(EXIT_FAILURE);
    }

    virt_addr = (mmap_pointer + (physical_address & page_mask));

    printf(" zaalokowano na adresie %p\n",mmap_pointer);

    /* Now write int's to the file as if it were memory (an array of ints).
     */
    int i;
    for(i=0; i<mem_size; ++i)
    {
        ((int*)mmap_pointer)[i] = data[i];
    }

//    *(unsigned int*)mmap_pointer = *data;

    /* Don't forget to free the mmmap_pointerped memory
     */
    if (munmap(mmap_pointer, alloc_mem_size) == -1) {
	perror("Error un-mmapping the file");
        exit(EXIT_FAILURE);
	/* Decide here whether to close(devmem_desc) and exit() or not. Depends... */
    }

    /* Un-mmaping doesn't close the file, so we still need to do that.
     */
    close(devmem_desc);
    printf("/dev/mem operations successful\n");

}
