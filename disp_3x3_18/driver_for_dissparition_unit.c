#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define MIN_DESCRIPTORS_OFFS 0x40

//struct for generate two mask descriptors

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
    u_int match_wide;
    u_int picture_wide;
    u_int picture_height;
    u_int descriptor_start_address;
    u_int l_picture_start_address;
    u_int r_picture_start_address;
    u_int destination_start_address;
    u_int unit_start_address;
    u_int mask_positions_array_start_address;
    u_int match_positions_array_start_address;

} g_conf;

g_out desc_generator_for_dissparition_unit(g_conf *config, unsigned long iteration, char last_iteration);

u_int** get_disspatition_map(u_int* l_picture, u_int* r_picture, g_conf* );

// returns pointer to memmory
void* get_mmap_pointer(int devmem_desc,size_t physical_address, size_t mem_size);

// read status register of CDMA
char read_cdma_status(u_int *cdma_pointer, char if_print);


int main (int argc, char* argv[])
{
#define PICTURE_WIDE 22
#define PICTURE_HEIGHT 10

    u_int l_picture[ PICTURE_HEIGHT ][ PICTURE_WIDE ] = {
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,2,3,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,4,5,6,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,7,8,9,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1} };

    u_int r_picture[ PICTURE_HEIGHT ][ PICTURE_WIDE ] = {
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,2,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,4,5,6,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,7,8,9,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1} };

    u_int flattern_l_picture[ PICTURE_WIDE * PICTURE_HEIGHT];
    u_int flattern_r_picture[ PICTURE_WIDE * PICTURE_HEIGHT];

    u_int i,j;
    for( i=0; i < PICTURE_HEIGHT; ++i)
    {
        for( j=0; j < PICTURE_WIDE; ++j)
        {
            flattern_l_picture[ i*PICTURE_WIDE + j ] = l_picture[i][j];
            flattern_r_picture[ i*PICTURE_WIDE + j ] = r_picture[i][j];

        }
    }

    g_conf config;
    config.mask_size                           = 3;
    config.match_wide                          = 18;
    config.picture_wide                        = PICTURE_WIDE;
    config.picture_height                      = PICTURE_HEIGHT;
    config.descriptor_start_address            = 0x10000000;
    config.l_picture_start_address             = 0x11000000;
    config.r_picture_start_address             = 0x12000000;
    config.destination_start_address           = 0x13000000;
    config.unit_start_address                  = 0x76000000;
    config.mask_positions_array_start_address  = 0x14000000;
    config.match_positions_array_start_address = 0x15000000;

    get_disspatition_map( flattern_l_picture, flattern_r_picture, &config );

    return 0;
}

u_int** get_disspatition_map(u_int* l_picture, u_int* r_picture, g_conf* config)
{
#define DESCRIPTOR_CELLS 9
#define UNIT_CDMA_CONFIG_ADDRESS 0x4e200000
#define CDMA_ALLOC_SIZE 28
#define MIN_DESCRIPTORS_OFFS 0x40

    u_int mask_size = config -> mask_size;
    u_int match_wide = config -> match_wide;
    u_int result_row_cells = config -> picture_wide - (mask_size -1);
    unsigned long result_cells = result_row_cells * config -> picture_height;


    unsigned long number_of_descriptors =
        (mask_size * mask_size + mask_size * match_wide + 3) // descriptors needed to calc one pixel
        * result_row_cells * config -> picture_height; // number of result pixells

    u_int **desc_array = malloc( sizeof(u_int*) * number_of_descriptors);
    if( desc_array == NULL )
    {
        perror("error_allocating **desc_array");
        exit(EXIT_FAILURE);
    }
    { // allloceting array for all descriptors /////////////////////////////////
        unsigned long i;
        for( i=0; i < number_of_descriptors; ++i)
        {
            desc_array[i] = malloc( sizeof(u_int) * DESCRIPTOR_CELLS );
            if ( desc_array[i] == NULL)
            {
                perror("error allocating desc_array[i]");
                exit(EXIT_FAILURE);
            }
        }
    }

    { // filling array of all descriptors
        unsigned long i;
        for( i=0; i < number_of_descriptors; ++i)
        {
            g_out the_descriptor = desc_generator_for_dissparition_unit(
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
    }

// check descriptors //////////////////////////////////////////////////////////

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

// get all /dev/mem pointers //////////////////////////////////////////////////

    int devmem_desc;
    devmem_desc = open( "/dev/mem", O_RDWR | O_SYNC );
    if( devmem_desc == -1 )
    {
        perror("Error opening /dev/mem for matching");
        exit(EXIT_FAILURE);
    }

    size_t pictures_alloc_size = sizeof(u_int) * config -> picture_wide * config -> picture_height;

    u_int* l_picture_mmap_pointer = (u_int*) get_mmap_pointer( devmem_desc,
            config -> l_picture_start_address, pictures_alloc_size );

    u_int* r_picture_mmap_pointer = (u_int*) get_mmap_pointer( devmem_desc,
            config -> r_picture_start_address, pictures_alloc_size );

    size_t result_alloc_size = sizeof(u_int) * result_row_cells * config -> picture_height;

    u_int* result_mmap_pointer = (u_int*) get_mmap_pointer( devmem_desc,
            config -> destination_start_address, result_alloc_size );

    size_t size_for_all_descriptors = sizeof(u_int) * DESCRIPTOR_CELLS * number_of_descriptors
        + MIN_DESCRIPTORS_OFFS * number_of_descriptors;

    u_int* descriptors_mmap_pointer = get_mmap_pointer( devmem_desc,
            config -> descriptor_start_address, size_for_all_descriptors );

    u_int* cdma_mmap_pointer = (u_int*) get_mmap_pointer( devmem_desc,
            UNIT_CDMA_CONFIG_ADDRESS, CDMA_ALLOC_SIZE );

    size_t position_arrays_alloc_size = sizeof(u_int) * result_cells;

    u_int* mask_positions_mmap_pointer = (u_int*) get_mmap_pointer( devmem_desc,
            config -> mask_positions_array_start_address, position_arrays_alloc_size );

    u_int* match_positions_mmap_pointer = (u_int*) get_mmap_pointer( devmem_desc,
            config -> match_positions_array_start_address, position_arrays_alloc_size );

// put arrays into physical memory ////////////////////////////////////////////

    {
        unsigned long i;
        unsigned long picture_cells = config -> picture_wide * config -> picture_height;
// left mask and right mask
        for( i=0; i < picture_cells; ++i)
            l_picture_mmap_pointer[i] = l_picture[i];

        for( i=0; i < picture_cells; ++i)
            r_picture_mmap_pointer[i] = r_picture[i];
// descriptors and free desc array
        for( i=0; i < number_of_descriptors; ++i )
        {
            u_int j;
            for( j=0; j < DESCRIPTOR_CELLS -1 ; ++j)
            {
                u_int offs = MIN_DESCRIPTORS_OFFS / sizeof(u_int);
                descriptors_mmap_pointer[ i*offs + j ] = desc_array[i][j+1];
            }
        }

        for( i=0; i < number_of_descriptors; ++i)
            free( desc_array[i] );
        free( desc_array );
// position arrays
        for( i=0; i < result_cells; ++i )
        {
            u_int act_mask_position = i % result_row_cells;
            mask_positions_mmap_pointer[i] = act_mask_position;

            if( act_mask_position <= match_wide)
                match_positions_mmap_pointer[i] = 0;
            else
                match_positions_mmap_pointer[i] = act_mask_position - match_wide;
        }


    }


// debug

//    {
//        unsigned long i;
//        u_int offs = 0x40 / sizeof(u_int);
//        for ( i=0; i < number_of_descriptors; ++i)
//        {
//            printf("hehe %lu %x\n",i,descriptors_mmap_pointer[i*offs + 4]);
//        }
//    }

// start configuration cdma ///////////////////////////////////////////////////

    {
#define CDMA_OFFS1 ( mask_size * mask_size + mask_size * match_wide + 2  )
#define CDMA_OFFS2 1

        u_int last_tail_descriptor = config -> descriptor_start_address;
        cdma_mmap_pointer[0] = cdma_mmap_pointer[0] | (1 << 2); // reset CDMA

        while( cdma_mmap_pointer[0] != 0x00010002 )
        {
            usleep(1);
        }
        cdma_mmap_pointer[0] = cdma_mmap_pointer[0] | (1 << 3); // enable SG mode
        unsigned long i;
        // all return pixells multiply by 2 minus 1
        for( i=0; i < result_cells -1; ++i )
        {
            cdma_mmap_pointer[1] = cdma_mmap_pointer[1] | (1 << 12); // get off interrupt indicator
            cdma_mmap_pointer[2] = last_tail_descriptor;
            cdma_mmap_pointer[4] = last_tail_descriptor + MIN_DESCRIPTORS_OFFS * CDMA_OFFS1;
            printf("hehe %x\n",last_tail_descriptor + MIN_DESCRIPTORS_OFFS * (CDMA_OFFS1 ) );


            while( cdma_mmap_pointer[1] != 0x0001100a )
            {
//                read_cdma_status(cdma_mmap_pointer,1);
                usleep(1);
            }

            cdma_mmap_pointer[1] = cdma_mmap_pointer[1] | (1 << 12);
            cdma_mmap_pointer[2] = last_tail_descriptor + MIN_DESCRIPTORS_OFFS * CDMA_OFFS1;
            cdma_mmap_pointer[4] = last_tail_descriptor + MIN_DESCRIPTORS_OFFS * ( CDMA_OFFS1 + CDMA_OFFS2);

            printf("hehehe %x\n",last_tail_descriptor + MIN_DESCRIPTORS_OFFS * (CDMA_OFFS1 + CDMA_OFFS2));

            last_tail_descriptor += MIN_DESCRIPTORS_OFFS * ( CDMA_OFFS1 + CDMA_OFFS2 ) ;

            while( cdma_mmap_pointer[1] != 0x0001100a )
            {
//                read_cdma_status(cdma_mmap_pointer,1);
                usleep(1);
            }
        }
#undef CDMA_OFFS1
#undef CDMA_OFFS2
    }

// print result for debug /////////////////////////////////////////////////////

    {
        unsigned long i;
        for( i=0; i < result_row_cells * config -> picture_height; ++i )
        {
            u_int row = i % result_row_cells;
            if( row == 0)
                printf("\n");
            printf( "[%d]",result_mmap_pointer[i] );
        }
    }

// unmap all mmap pointers and close /dev/mem

    int munmap_ind;

    munmap_ind = munmap( l_picture_mmap_pointer, pictures_alloc_size );
    if( munmap_ind == -1 )
        perror("error unmapping match_space_mmap_pointer\n");

    munmap_ind = munmap( r_picture_mmap_pointer, pictures_alloc_size );
    if( munmap_ind == -1 )
        perror("error unmapping mask_mmap_pointer\n");

    munmap_ind = munmap( result_mmap_pointer, result_alloc_size );
    if( munmap_ind == -1 )
        perror("error unmapping result_mmap_pointer\n");

    munmap_ind = munmap( descriptors_mmap_pointer, size_for_all_descriptors );
    if( munmap_ind == -1 )
        perror("error unmapping descriptors_mmap_pointer\n");

    munmap_ind = munmap( cdma_mmap_pointer, CDMA_ALLOC_SIZE );
    if( munmap_ind == -1 )
        perror("error unmapping cdma_mmap_pointer\n");

    munmap_ind = munmap( mask_positions_mmap_pointer, position_arrays_alloc_size );
    if( munmap_ind == -1 )
        perror("error unmapping mask_positions_mmap_pointer\n");

    munmap_ind = munmap( match_positions_mmap_pointer, position_arrays_alloc_size );
    if( munmap_ind == -1 )
        perror("error unmapping match_positions_mmap_pointer\n");

    close(devmem_desc);

    exit(EXIT_SUCCESS);



}



g_out desc_generator_for_dissparition_unit(g_conf *config, unsigned long iteration, char last_iteration)
{

#define U_INT_S sizeof(u_int);
    u_int mask_size = config -> mask_size;
    u_int match_wide = config -> match_wide;
    u_int result_row_cells = config -> picture_wide - (mask_size - 1);

    g_out the_descriptor;

    the_descriptor.my_addr = config -> descriptor_start_address + iteration * 0x40;
// circle of descriptors addresses
    if ( last_iteration == 0 )
    {
        the_descriptor.next_desc_addr = config -> descriptor_start_address + (iteration + 1) * 0x40;
    }
    else
    {
        the_descriptor.next_desc_addr = config -> descriptor_start_address;
    }

    // number of reqired descriptors to get one result pixel
    u_int one_pix_desc_num = mask_size * mask_size
        + mask_size * match_wide
        +3; // mask_position, match_position, result;

    // number of actual return working pixel
    u_int wrk_pxl_num = iteration / one_pix_desc_num;
    // number for actual descriptor for actual working pixel
    u_int wrk_desc_num = iteration % one_pix_desc_num;

    // thanks to this construction the function can generate anyone descriptor
    // independent of previous. all parameters are calculated directly from
    // "iteration" variable
    if( wrk_desc_num < mask_size * mask_size )
    {
        // filling  mask
        u_int mask_row = wrk_desc_num / mask_size;

        the_descriptor.source_addr = config -> l_picture_start_address
            +( (wrk_desc_num % mask_size) // mask col offset
            + mask_row * (config -> picture_wide) // mask row offset
            + wrk_pxl_num % result_row_cells // mask position col offset
            + (wrk_pxl_num / result_row_cells) * config -> picture_wide )// mask_position row offset
            * U_INT_S ;

        the_descriptor.destiny_addr = config -> unit_start_address
            + wrk_desc_num * U_INT_S;

    }
    else if ( wrk_desc_num < mask_size * mask_size + mask_size * match_wide )
    {
        // filling match space
        // match position starts moving when mask reaches last of match possibillity
        if ( (wrk_pxl_num % result_row_cells) <= (match_wide - mask_size) )
        {
            u_int row = (wrk_desc_num - (mask_size * mask_size)) / match_wide;

            the_descriptor.source_addr = config -> r_picture_start_address
                +( (wrk_desc_num - mask_size * mask_size) % match_wide // match coll offset
                + row * (config -> picture_wide) // match row offset
                + (wrk_pxl_num / result_row_cells) * config -> picture_wide ) // match position row offset
                * U_INT_S;
        }
        else
        {
            u_int row = (wrk_desc_num - (mask_size * mask_size)) / match_wide;

            the_descriptor.source_addr = config -> r_picture_start_address
                +( (wrk_desc_num - mask_size * mask_size) % match_wide
                + row * (config -> picture_wide)
                + (wrk_pxl_num % result_row_cells) - (match_wide - mask_size)
                + (wrk_pxl_num / result_row_cells) * config -> picture_wide )
                * U_INT_S;
        }

        the_descriptor.destiny_addr = config -> unit_start_address
            + wrk_desc_num * U_INT_S;
    }
    else if (wrk_desc_num == mask_size * mask_size + mask_size * match_wide)
    {
        // descriptor of mask_position
        // this function requires addictional array of mask_positions
        //
        // mask_position is: "wrk_pxl_num % picture_wide"
        // return picture is: "picture_wide - (mask_size-1)/2"
        // last pixells (number: "picture_wide - (mask_size-1)/2) are random
        // becouse of jumping to another row of picture since: "(mask_size-1)/2
        // pixells

        the_descriptor.source_addr = config -> mask_positions_array_start_address
            + wrk_pxl_num * U_INT_S;
        the_descriptor.destiny_addr = config -> unit_start_address
            + wrk_desc_num * U_INT_S;

    }
    else if (wrk_desc_num == mask_size * mask_size + mask_size * match_wide + 1)
    {
        // descriptor of match_position
        // this function requires addictional array of match_positions
        //

        the_descriptor.source_addr = config -> match_positions_array_start_address
            + wrk_pxl_num * U_INT_S;
        the_descriptor.destiny_addr = config -> unit_start_address
            + wrk_desc_num * U_INT_S;
    }
    else if (wrk_desc_num == mask_size * mask_size + mask_size * match_wide + 2)
    {
        // descriptor of result
        the_descriptor.source_addr = config -> unit_start_address;
        the_descriptor.destiny_addr = config -> destination_start_address
            + wrk_pxl_num * U_INT_S;
    }

    the_descriptor.bytes_to_transfer = 4;

    return the_descriptor;

#undef MIN_DESCRIPTORS_OFFS
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

