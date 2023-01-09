#include "zlib.h"
#include <stdio.h>  // for printf
#include <string.h> // for memset
#include <stdlib.h> // for malloc/free

// https://pyokagan.name/blog/2019-10-14-png/

#define TEST_PNG "Images/Figure_1.png"
#define PNG_SIGNATURE "\x89PNG\r\n\x1a\n"
#define ZLIB_BUF_SIZE 16 * 1024
#define MAX_CHUNKS 100
#define ERROR_RETURN -1
#define OK_RETURN 0
#define PNG_IHDR_LENGTH 13
#define PNG_BYTE_PIXEL 4

typedef struct png_chunk {
    unsigned int chunk_length;
    char* chunk_type;
    char* chunk_data;
    unsigned int chunk_crc;
} png_chunk;

typedef struct png_file {
    unsigned char* png_signature;
    png_chunk* png_chunk_data[MAX_CHUNKS];
    unsigned int png_crc;
} png_file;

typedef struct png_IHDR {
    unsigned int width;
    unsigned int height;
    unsigned char bit_depth; //profondità dei bit
    unsigned char color_type; //tipo di colore
    unsigned char compression_method; //metodo di compressione
    unsigned char filter_method; //metodo di filtraggio
    unsigned char interlace_method; //metodo di interlacciamento
} png_IHDR;

FILE* png_stream = NULL; //file stream del file png da leggere e scrivere in binario (b) in lettura (r) e scrittura (w)

png_file* png_file_data = NULL;

unsigned int byteswap(unsigned int n) //funzione per invertire l'ordine dei byte da little endian a big endian
{
    unsigned int m = (n << 24); //sposta quarto byte
    m += (n << 8)&0x00ff0000; //sposta terzo byte
    m += (n >> 8)&0x0000ff00; //sposta secondo byte
    m += (n >> 24)&0x000000ff; //sposta primo byte
    return m;
}

errno_t PNG_open_signature(png_file* png_file_data) //funzione per aprire il file e verificare la signature
{
    // read the file
    errno_t png_file_errno = fopen_s(&png_stream, TEST_PNG, "rb");

    // check if the file is a png
    if(png_file_errno != 0)
    {
        printf("Error: File is not a png: error %d", png_file_errno);
        fclose(png_stream);
        return png_file_errno;
    }

    // read the png signature
    unsigned char* png_signature = malloc(8);
    unsigned char png_signature_check[8] = PNG_SIGNATURE;

    // check if the signature is correct
    if(fread(png_signature, 1, 8, png_stream) != 8)
    {
        printf("Error on signature size");
        free(png_signature);
        fclose(png_stream);
        return png_file_errno;
    }

    // check if the signature is correct
    for(size_t i = 0; i < sizeof(png_signature_check); i++)
    {
        if(png_signature[i] != png_signature_check[i])
        {
            printf("Error on different signature: signature found %s - expected %s",png_signature, png_signature_check);
            free(png_signature);
            fclose(png_stream);
            return png_file_errno;
        }
    }

    png_file_data->png_signature = png_signature; //non uso la free perché tengo l'area di memoria puntata nella struttura

    return png_file_errno;
}

errno_t PNG_read_chunk(png_chunk* chunk_data) //funzione per leggere i chunk del file png
{
    int type_length = 4;
    
    // read the chunk length
    unsigned int chunk_length; // 4 bytes
    fread(&chunk_length, 1, 4, png_stream); // read 4 bytes
    chunk_length = byteswap(chunk_length);
    //printf("Chunk length: %d ***\n", chunk_length); // print the chunk length
    chunk_data->chunk_length = chunk_length;

    
    //read the chunk type and chunk data together for crc calculate purpose
    char* chunk_dati_for_crc = malloc(type_length + chunk_length); // 4 bytes
    fread(chunk_dati_for_crc, 1, type_length + chunk_length, png_stream); // read 4 bytes
    //printf("Chunk type: %s ***\n", chunk_type); // print the chunk type
    chunk_data->chunk_type = chunk_dati_for_crc; // chunk_type is pointed directly
    chunk_data->chunk_data = chunk_dati_for_crc + type_length; //chunk_data is four bytes ahead

    // read the chunk crc
    unsigned int chunk_crc; // 4 bytes
    fread(&chunk_crc, 1, 4, png_stream); // read 4 bytes
    chunk_crc = byteswap(chunk_crc);
    //printf("Chunk crc read: %u ***\n", chunk_crc); // print the chunk crc

    unsigned int calculated_crc = crc32(0, (const unsigned char*)chunk_dati_for_crc, chunk_length + type_length); // note the 11 as we don't want to include the trailing 0

    if(calculated_crc != chunk_crc)
    {
        printf("crc32 calculated is %u\n", calculated_crc);
        return ERROR_RETURN;
    }
    return OK_RETURN;
}

errno_t PNG_IHDR_check(png_IHDR* IHDR_data, png_chunk* png_IHDR_chunk) //funzione per verificare la correttezza del chunk IHDR
{
    unsigned int data_to_swap; //variabile per salvare i dati da invertire
    unsigned int* ptr; //puntatore per salvare l'indirizzo di memoria di data_to_swap

    if(png_IHDR_chunk->chunk_length != PNG_IHDR_LENGTH)
    {
        return ERROR_RETURN;
    }

    IHDR_data->bit_depth = png_IHDR_chunk->chunk_data[8];
    IHDR_data->color_type = png_IHDR_chunk->chunk_data[9]; 
    IHDR_data->compression_method = png_IHDR_chunk->chunk_data[10];
    IHDR_data->filter_method = png_IHDR_chunk->chunk_data[11];
    IHDR_data->interlace_method = png_IHDR_chunk->chunk_data[12];

    ptr = (unsigned int*) png_IHDR_chunk->chunk_data; //salvo l'indirizzo di memoria del chunk data in ptr
    data_to_swap = *ptr; //salvo il valore puntato da ptr in data_to_swap (in questo caso il valore di width)
    IHDR_data->width  = byteswap(data_to_swap);

    ptr = ptr + 1; //incremento di 4 bytes l'indirizzo di memoria di ptr (in questo caso il valore di height)
    data_to_swap = *ptr; //salvo il valore puntato da ptr in data_to_swap (in questo caso il valore di height)
    IHDR_data->height  = byteswap(data_to_swap);

    if (IHDR_data->compression_method != 0)
    {
        //raise Exception('invalid compression method')
        return ERROR_RETURN;
    }
    if (IHDR_data->filter_method != 0)
    {
        //raise Exception('invalid filter method')
        return ERROR_RETURN;
    }

    if (IHDR_data->color_type != 6)
    {
        //raise Exception('we only support truecolor with alpha')
        return ERROR_RETURN;
    }
    if (IHDR_data->bit_depth != 8)
    {
        //raise Exception('we only support a bit depth of 8')
        return ERROR_RETURN;
    }
    if (IHDR_data->interlace_method != 0)
    {
        //raise Exception('we only support no interlacing')
        return ERROR_RETURN;
    }

    return OK_RETURN;
}

char* PNG_IDAT_enqueue(char* png_image, png_chunk* png_IDAT_chunk, int chunk_displacement) //funzione per aggiungere i chunk IDAT alla struttura dati
{
    if(png_image == NULL || chunk_displacement == 0)
    {
        png_image = (char*)malloc(png_IDAT_chunk->chunk_length); //alloco la memoria per il chunk IDAT e lo salvo in png_image
        if(png_image == NULL)
        {
            return NULL;
        }
        png_image = (char*)memcpy(png_image, png_IDAT_chunk->chunk_data, png_IDAT_chunk->chunk_length);
        if(png_image == NULL)
        {
            return NULL;
        }
        chunk_displacement = png_IDAT_chunk->chunk_length;
    }
    else
    {
        png_image = (char*)realloc(png_image, png_IDAT_chunk->chunk_length); //alloco la memoria per il chunk IDAT e lo salvo in png_image
        if(png_image == NULL)
        {
            return NULL;
        }
        png_image = (char*)memcpy(png_image + chunk_displacement, png_IDAT_chunk->chunk_data, png_IDAT_chunk->chunk_length);
        if(png_image == NULL)
        {
            return NULL;
        }
        chunk_displacement += png_IDAT_chunk->chunk_length;
    }
    return png_image;
}

int paeth_predictor(int a, int b, int c) //funzione per il calcolo del predictor Paeth
{
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    int pr;

    if (pa <= pb && pa <= pc) 
    { 
        pr = a;
    }
    else if(pb <= pc)
    {
        pr = b;
    }
    else
    {
        pr = c;
    }
    return pr;
}

int recon_a(int r, int c, int stride, char* recon) //funzioni per il calcolo dei valori di riferimento per il predictor
{
    if (c < PNG_BYTE_PIXEL) 
    {
        return 0;
    }

    return recon[r * stride + c - PNG_BYTE_PIXEL];
}

int recon_b(int r, int c, int stride, char* recon) //funzioni per il calcolo dei valori di riferimento per il predictor
{
    if (r == 0) 
    {
        return 0;
    }

    return recon[(r-1) * stride + c - PNG_BYTE_PIXEL];
}

int recon_c(int r, int c, int stride, char* recon) //funzioni per il calcolo dei valori di riferimento per il predictor
{
    if (r == 0 || c < PNG_BYTE_PIXEL) 
    {
        return 0;
    }

    return recon[(r-1) * stride + c - PNG_BYTE_PIXEL];
}

char* PNG_recon_array(png_IHDR *header, char* png_data) //funzione per il calcolo dell'array di pixel
{ 
    int i = 0;
    int array_index = 0;
    int stride = PNG_BYTE_PIXEL * header->width;
    int filter_type, r, c;
    unsigned char filt_x, recon_x;
    char* recon = malloc(header->height * stride);

    for (r = 0; r < header->height; r++) //: # for each scanline
    {
        filter_type = png_data[i++]; //# first byte of scanline is filter type
        for (c = 0; c < stride; c++)
        {
            filt_x = png_data[i++];
            if (filter_type == 0)
            {
                recon_x = filt_x;
            } //: # None

            else if(filter_type == 1)
            {
                recon_x = filt_x + recon_a(r, c, stride, recon);
            }  //: # Sub

            else if(filter_type == 2)
            {
                recon_x = filt_x + recon_b(r, c, stride, recon);
            } //: # Up

            else if(filter_type == 3)
            {
                recon_x = filt_x + (recon_a(r, c, stride, recon) + recon_b(r,c, stride, recon)) / 2;
            } //: # Average

            else if(filter_type == 4)
            {
                recon_x = filt_x + paeth_predictor(recon_a(r, c, stride, recon),recon_b(r,c, stride, recon), recon_c(r,c, stride, recon));
            } //: # Paeth
            else
            {
                free(recon);
                return NULL;
            }
            //Recon.append(Recon_x & 0xff) # calculation to truncated to single byte (Mandatory only in Python)
            recon[array_index++] = recon_x;
        } //: # for each byte in scanline
       
    }
    return recon;
}

int main(int argc, char **argv)
{
    char* png_image = NULL;
    int png_image_length = 0;
    int i,j;
    png_IHDR *png_IHDR_data;
    png_chunk *pippo;
    png_file_data = (png_file*) malloc(sizeof(png_file)); //alloco la memoria per la struttura png_file che conterrà tutti i dati del file png da convertire
    errno_t png_stream_errno = PNG_open_signature(png_file_data);
    char* recon = NULL;

    if(png_stream_errno != 0)
    {
        printf("PNG conversion in Error");
        free(png_file_data);
        return png_stream_errno;
    }
    
    for(i = 0; i < MAX_CHUNKS; i++)
    {
        pippo = malloc(sizeof(png_chunk));
        
        if(PNG_read_chunk(pippo) == ERROR_RETURN)
        {
            printf("Error Chunk type: %s number %d invalid crc: %u***\n", pippo->chunk_type, i, pippo->chunk_crc);
            for(j = 0; j < i; j++)
            {
                //TODO: fix free file oriented structs i,poddible - non local to main()
                //free(png_file_data->png_chunk_data[j]->chunk_type);
                //free(png_file_data->png_chunk_data[j]);
                if(j == 0)
                {
                    free(png_IHDR_data);
                }
            }
            free(pippo);
            free(png_file_data);
            return ERROR_RETURN;
        }
        
        //printf("Chunk type: %s ***\n", pippo->chunk_type); // print the chunk type
        //printf("Chunk length: %d ***\n", pippo->chunk_length); // print the chunk length

        if(strstr(pippo->chunk_type, "IHDR") != NULL)
        {            
            png_IHDR_data = malloc(sizeof(png_IHDR));
            errno_t png_IHDR_errno = PNG_IHDR_check(png_IHDR_data,pippo);
            if(png_IHDR_errno != 0)
            {
                printf("Error PNG IHDR not valid: %d", png_IHDR_errno);
                free(pippo);
                free(png_file_data);
                free(png_IHDR_data);
                return ERROR_RETURN;
            }

        }

        if(strstr(pippo->chunk_type, "IEND") != NULL)
        {
            break;
        }

        if(strstr(pippo->chunk_type, "IDAT") != NULL)
        {
            png_image = malloc(pippo->chunk_length);
            png_image_length = pippo->chunk_length;
            memcpy(png_image, pippo->chunk_data, pippo->chunk_length);
        }
        png_file_data->png_chunk_data[i] = pippo;
    }
    
    //TODO: fix free file oriented structs i,poddible - non local to main()
    for(j = 0; j <= i; j++)
    {
        //free(png_file_data->png_chunk_data[j]->chunk_type);
        //free(png_file_data->png_chunk_data[j]);
        if(j == 0)
        {
            free(png_IHDR_data);
        }
    }
    free(png_file_data);

    //uncompress PNG
    unsigned long uncompressed_size = 600000;
    unsigned char *uncompressed_data = malloc(uncompressed_size);
    int result = uncompress(uncompressed_data, &uncompressed_size, png_image, png_image_length);
    if(result != Z_OK)
    {
        free(uncompressed_data);
        free(png_image);
        printf("Error uncompressing the png file");
        return ERROR_RETURN;
    }
    else
    {
        free(png_image);
    }
    recon = PNG_recon_array(png_IHDR_data, uncompressed_data);
    
    //TODO: display png image
    //

    free(uncompressed_data);
    free(recon);
    return OK_RETURN; 
    
    //TODO: (OPTIONAL) specialize error codes
    //TODO: (OPTIONAL) Create .h file with function
}
