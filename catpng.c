#include "catpng.h"

int powerFunction(int x, int y)
{
    int result = x;
    if (y == 0)
        return 1;
    for (int i = 1; i < y; i++)
    {
        result = result * x;
    }
    return result;
}
int hexToDec(unsigned char *hexVal, int hexLen)
{
    int result = 0;
    int l = hexLen - 1;
    for (int i = 0; i < hexLen; i++)
    {
        int val = hexVal[i];
        int power = powerFunction(256, l);
        result = result + val * power;
        l = l - 1;
    }

    return result;
}

int getDimensions(unsigned char *IHDR)
{
    int size = 4;
    U8 *width = malloc(size * sizeof(U8));
    U8 *height = malloc(size * sizeof(U8));

    int j = 4;
    for (int i = 0; i < size; i++)
    {
        width[i] = IHDR[i];
        height[i] = IHDR[j];
        j++;
    }

    int w = hexToDec(width, size);
    int h = hexToDec(height, size);

    // printf("data: 0x%X\n", IHDR[0]);
    // printf("data: 0x%X\n", IHDR[1]);
    // printf("data: 0x%X\n", IHDR[2]);
    // printf("data: 0x%X\n", IHDR[3]);
    // printf("data: 0x%X\n", IHDR[4]);
    // printf("data: 0x%X\n", IHDR[5]);
    // printf("data: 0x%X\n", IHDR[6]);
    // printf("data: 0x%X\n", IHDR[7]);
    // printf("data: 0x%X\n", IHDR[8]);
    // printf("data: 0x%X\n", IHDR[9]);
    // printf("data: 0x%X\n", IHDR[10]);
    // printf("data: 0x%X\n", IHDR[11]);
    // printf("data: 0x%X\n", IHDR[12]);
    // printf("\n");

    return h * (w * 4 + 1);
}

simple_PNG_p changeHeight(simple_PNG_p png1, simple_PNG_p png2)
{
    int size = 4;
    U8 *height1 = malloc(size * sizeof(U8));
    U8 *height2 = malloc(size * sizeof(U8));
    U8 *combinedHeight = malloc(size * sizeof(U8));

    int j = 4;
    for (int i = 0; i < size; i++)
    {
        height1[i] = png1->p_IHDR->p_data[j];
        height2[i] = png2->p_IHDR->p_data[j];
        j++;
    }
    U32 h1 = hexToDec(height1, size);
    U32 h2 = hexToDec(height2, size);

    U32 sum = h1 + h2;
    memcpy(combinedHeight, &sum, 4); /*Copies over in little endian format*/

    int k = 7;
    for (int i = 0; i < size; i++)
    {
        png1->p_IHDR->p_data[k] = combinedHeight[i];
        k--;
    }

    return png1;
}

simple_PNG_p inflatePNGS(simple_PNG_p png)
{
    U64 compressed_len = 0;
    U64 uncompressed_len = 0;
    U8 *compressed_buffer;
    U8 *uncompressed_buffer;

    simple_PNG_p s = malloc(sizeof(struct simple_PNG));
    chunk_p IHDR = malloc(sizeof(struct chunk));
    chunk_p IDAT = malloc(sizeof(struct chunk));
    chunk_p IEND = malloc(sizeof(struct chunk));

    compressed_len = png->p_IDAT->length;
    compressed_buffer = malloc(compressed_len * sizeof(U8));
    compressed_buffer = png->p_IDAT->p_data;

    int size = getDimensions(png->p_IHDR->p_data);
    uncompressed_buffer = malloc(size * sizeof(U8));
    int result = mem_inf(uncompressed_buffer, &uncompressed_len, compressed_buffer, compressed_len);

    if (result != 0)
    {
        printf("Mem_inf failed: value %d\n", result);
        exit(2);
    }

    IHDR = png->p_IHDR;
    IEND = png->p_IEND;
    IDAT = png->p_IDAT;
    IDAT->length = uncompressed_len;
    IDAT->p_data = uncompressed_buffer;

    s->p_IHDR = IHDR;
    s->p_IEND = IEND;
    s->p_IDAT = IDAT;

    return s;
}

simple_PNG_p deflatePNGS(simple_PNG_p png)
{
    U64 compressed_len = 0;
    U64 uncompressed_len = 0;
    U8 *compressed_buffer;
    U8 *uncompressed_buffer;

    simple_PNG_p s = malloc(sizeof(struct simple_PNG));
    chunk_p IHDR = malloc(sizeof(struct chunk));
    chunk_p IDAT = malloc(sizeof(struct chunk));
    chunk_p IEND = malloc(sizeof(struct chunk));

    uncompressed_len = png->p_IDAT->length;
    uncompressed_buffer = malloc(uncompressed_len * sizeof(U8));
    uncompressed_buffer = png->p_IDAT->p_data;

    int size = getDimensions(png->p_IHDR->p_data);

    // printf("Value of size is: %d\n", size);
    compressed_buffer = malloc(size * sizeof(U8));
    int result = mem_def(compressed_buffer, &compressed_len, uncompressed_buffer, uncompressed_len, Z_DEFAULT_COMPRESSION);

    if (result != 0)
    {
        printf("Mem_def failed: value %d\n", result);
        exit(2);
    }

    IHDR = png->p_IHDR;
    IEND = png->p_IEND;
    IDAT = png->p_IDAT;
    IDAT->length = compressed_len;
    IDAT->p_data = compressed_buffer;

    s->p_IHDR = IHDR;
    s->p_IEND = IEND;
    s->p_IDAT = IDAT;

    return s;
}
simple_PNG_p combinePNGS(simple_PNG_p pngs[], int numPNGS)
{
    // simple_PNG_p uncompressedPNGS[numPNGS];

    // /*Get uncompressed IDAT values for all PNGS*/
    // for (int i = 0; i < numPNGS; i++)
    // {
    //     uncompressedPNGS[i] = inflatePNGS(pngs[i]);
    // }

    simple_PNG_p uncompressed_result = malloc(sizeof(struct simple_PNG));
    uncompressed_result = pngs[0];

    for (int i = 1; i < numPNGS; i++)
    {
        if (numPNGS == 1)
        {
            break;
        }
        /*Give storage enough space for two PNG IDAT values*/
        U32 data_length = uncompressed_result->p_IDAT->length + pngs[i]->p_IDAT->length;
        U8 *data = malloc(data_length * sizeof(U8));
        memcpy(data, uncompressed_result->p_IDAT->p_data, uncompressed_result->p_IDAT->length);
        memcpy(data + uncompressed_result->p_IDAT->length, pngs[i]->p_IDAT->p_data, pngs[i]->p_IDAT->length);

        uncompressed_result->p_IDAT->p_data = data;
        uncompressed_result->p_IDAT->length = data_length;
        
        uncompressed_result = changeHeight(uncompressed_result, pngs[i]);
    }

    simple_PNG_p compressed_result = malloc(sizeof(struct simple_PNG));

    compressed_result = deflatePNGS(uncompressed_result);

    /*Recalculate CRC value for IHDR*/
    U32 crc_val_IHDR = 0;
    U32 size_IHDR = compressed_result->p_IHDR->length + 4;
    U8 *buffer_IHDR = malloc(size_IHDR * sizeof(U8));
    memcpy(buffer_IHDR, compressed_result->p_IHDR->type, 4);
    memcpy(buffer_IHDR + 4, compressed_result->p_IHDR->p_data, compressed_result->p_IHDR->length);
    crc_val_IHDR = crc(buffer_IHDR, size_IHDR);
    compressed_result->p_IHDR->crc = crc_val_IHDR;

    /*Recalculate CRC value for IDAT*/
    U32 crc_val_IDAT = 0;
    U32 size_IDAT = compressed_result->p_IDAT->length + 4;
    U8 *buffer_IDAT = malloc(size_IDAT * sizeof(U8));
    memcpy(buffer_IDAT, compressed_result->p_IDAT->type, 4);
    memcpy(buffer_IDAT + 4, compressed_result->p_IDAT->p_data, compressed_result->p_IDAT->length);
    crc_val_IDAT = crc(buffer_IDAT, size_IDAT);
    compressed_result->p_IDAT->crc = crc_val_IDAT;

    /*Recalculate CRC value for IEND*/
    U32 crc_val_IEND = 0;
    U32 size_IEND = compressed_result->p_IEND->length + 4;
    U8 *buffer_IEND = malloc(size_IEND * sizeof(U8));
    memcpy(buffer_IEND, compressed_result->p_IEND->type, 4);
    memcpy(buffer_IEND + 4, compressed_result->p_IEND->p_data, compressed_result->p_IEND->length);
    crc_val_IEND = crc(buffer_IEND, size_IEND);
    compressed_result->p_IEND->crc = crc_val_IEND;

    compressed_result->magicNumBuffer = pngs[0]->magicNumBuffer;

    return compressed_result;
}

simple_PNG_p getPNGInfo(U8* buffer)
{
    simple_PNG_p s = malloc(sizeof(struct simple_PNG));
    chunk_p IHDR = malloc(sizeof(struct chunk));
    chunk_p IDAT = malloc(sizeof(struct chunk));
    chunk_p IEND = malloc(sizeof(struct chunk));
    U32 totalLength = 0;

    int magicNumLen = 8; /*First 8 bytes of a png file*/
    U8 *magicNum_buffer = malloc(magicNumLen * sizeof(U8));
    memcpy(magicNum_buffer, buffer, magicNumLen); /*Read magic number*/
    s->magicNumBuffer = magicNum_buffer;

    totalLength += magicNumLen;

    U32 IHDR_length = 4; /*Next 4 bytes tell data byte size*/
    U8 *IHDR_len_buffer = malloc(IHDR_length * sizeof(U8));
    memcpy(IHDR_len_buffer, buffer + totalLength, IHDR_length); /*Read IHDR chunk*/
    U32 IHDR_data_length = hexToDec(IHDR_len_buffer, IHDR_length);
    IHDR->length = IHDR_data_length;
    
    totalLength += IHDR_length;

    int IHDR_type = 4; /*Next 4 bytes tell type*/
    U8 *IHDR_type_buffer = malloc(IHDR_type * sizeof(U8));
    memcpy(IHDR_type_buffer, buffer + totalLength, IHDR_type); /*Read IHDR chunk*/
    memcpy(IHDR->type, IHDR_type_buffer, 4);     /*Copy data over to struct*/

    totalLength += IHDR_type;

    U8 *IHDR_data_buffer = malloc(IHDR_data_length * sizeof(U8)); /*Read IHDR chunk*/
    memcpy(IHDR_data_buffer, buffer + totalLength, IHDR_data_length);
    IHDR->p_data = IHDR_data_buffer; /*Point struct to data*/

    totalLength += IHDR_data_length;

    U32 IHDR_CRC = 4; /*Last 4 bytes tell CRC*/
    U8 *IHDR_CRC_buffer = malloc(IHDR_CRC * sizeof(U8));        
    memcpy(IHDR_CRC_buffer, buffer + totalLength, IHDR_CRC);/*Read IHDR chunk*/
    U32 CRC_Val = hexToDec(IHDR_CRC_buffer, IHDR_CRC); /*Gets Base 10 val of CRC*/
    IHDR->crc = CRC_Val;

    totalLength += IHDR_CRC;

    U32 lengthSize = 4;                                    /* First four bytes of IDAT to find byte size of data */
    U8 *IDAT_len_buffer = malloc(lengthSize * sizeof(U8)); /*Store value in buffer*/
    memcpy(IDAT_len_buffer, buffer + totalLength, lengthSize);
    U32 IDAT_data_length = hexToDec(IDAT_len_buffer, lengthSize);
    IDAT->length = IDAT_data_length;

    totalLength += lengthSize;

    U32 typeSize = 4;                                     /* Next four bytes of IDAT to find type of data */
    U8 *IDAT_type_buffer = malloc(typeSize * sizeof(U8)); /*Store value in buffer*/
    memcpy(IDAT_type_buffer, buffer + totalLength, typeSize);
    memcpy(IDAT->type, IDAT_type_buffer, 4); /*Copy data over to struct*/

    totalLength += typeSize;

    U32 IDAT_data_size = hexToDec(IDAT_len_buffer, lengthSize);
    U8 *IDAT_data_buffer = malloc(IDAT_data_size * sizeof(U8));
    memcpy(IDAT_data_buffer, buffer + totalLength, IDAT_data_size);/*Read IDAT data*/
    IDAT->p_data = IDAT_data_buffer;

    totalLength += IDAT_data_size;

    U32 IDAT_CRC = 4; /*Last 4 bytes tell CRC*/
    U8 *IDAT_CRC_buffer = malloc(IDAT_CRC * sizeof(U8));
    memcpy(IDAT_CRC_buffer, buffer + totalLength, IDAT_CRC); /*Read IHDR chunk*/
    U32 CRC_Val_IDAT = hexToDec(IDAT_CRC_buffer, IDAT_CRC); /*Gets Base 10 val of CRC*/
    IDAT->crc = CRC_Val_IDAT;

    totalLength += IDAT_CRC;

    U32 lengthSize_IEND = 4;                                    /* First four bytes of IEND to find byte size of data */
    U8 *IEND_len_buffer = malloc(lengthSize_IEND * sizeof(U8)); /*Store value in buffer*/
    memcpy(IEND_len_buffer, buffer + totalLength, lengthSize_IEND);
    U32 IEND_data_length = hexToDec(IEND_len_buffer, lengthSize_IEND);
    IEND->length = IEND_data_length;

    totalLength += lengthSize_IEND;

    U32 typeSize_IEND = 4;                                     /* Next four bytes of IEND to find type of data */
    U8 *IEND_type_buffer = malloc(typeSize_IEND * sizeof(U8)); /*Store value in buffer*/
    memcpy(IEND_type_buffer, buffer + totalLength, typeSize_IEND);
    memcpy(IEND->type, IEND_type_buffer, 4); /*Copy data over to struct*/

    totalLength += typeSize_IEND;

    /*IEND has no data*/
    U32 IEND_CRC = 4; /*Last 4 bytes tell CRC*/
    U8 *IEND_CRC_buffer = malloc(IEND_CRC * sizeof(U8));
    memcpy(IEND_CRC_buffer, buffer + totalLength, IEND_CRC); /*Read IHDR chunk*/
    U32 CRC_Val_IEND = hexToDec(IEND_CRC_buffer, IEND_CRC); /*Gets Base 10 val of CRC*/
    IEND->crc = CRC_Val_IEND;

    totalLength += IEND_CRC;

    s->p_IHDR = IHDR;
    s->p_IDAT = IDAT;
    s->p_IEND = IEND;
    return s;
}

void createPNGFile(simple_PNG_p result)
{
    U8* magicNum_buffer;
    magicNum_buffer = malloc(8*sizeof(U8));
    memcpy(magicNum_buffer, result->magicNumBuffer, 8);

    FILE *file;
    file = fopen("all.png", "w"); /*Create new file called all.png*/

    /*Write the 8 bytes that define a png file*/
    fwrite(magicNum_buffer, 8, 1, file);

    /*Write IHDR length*/
    U8 *IHDR_length = malloc(4 * sizeof(U8));
    U8 *IHDR_length_r = malloc(4 * sizeof(U8));
    memcpy(IHDR_length_r, &result->p_IHDR->length, 4);
    int k = 3;
    /*IDHR_length_r is in reverse(little endian)*/
    for (int i = 0; i < 4; i++)
    {
        IHDR_length[i] = IHDR_length_r[k];
        k--;
    }
    fwrite(IHDR_length, 4, 1, file);

    /*Write IHDR type*/
    fwrite(result->p_IHDR->type, 4, 1, file);

    /*Write IHDR data*/
    fwrite(result->p_IHDR->p_data, result->p_IHDR->length, 1, file);

    /*Write IHDR CRC*/
    U8 *IHDR_CRC = malloc(4 * sizeof(U8));
    U8 *IHDR_CRC_r = malloc(4 * sizeof(U8));
    memcpy(IHDR_CRC_r, &result->p_IHDR->crc, 4);
    k = 3;
    for (int i = 0; i < 4; i++)
    {
        IHDR_CRC[i] = IHDR_CRC_r[k];
        k--;
    }
    fwrite(IHDR_CRC, 4, 1, file);

    /*Write IDAT length*/
    U8 *IDAT_length = malloc(4 * sizeof(U8));
    U8 *IDAT_length_r = malloc(4 * sizeof(U8));
    memcpy(IDAT_length_r, &result->p_IDAT->length, 4);
    k = 3;
    for (int i = 0; i < 4; i++)
    {
        IDAT_length[i] = IDAT_length_r[k];
        k--;
    }
    fwrite(IDAT_length, 4, 1, file);

    /*Write IDAT type*/
    fwrite(result->p_IDAT->type, 4, 1, file);

    /*Write IDAT data*/
    fwrite(result->p_IDAT->p_data, result->p_IDAT->length, 1, file);

    /*Write IDAT CRC*/
    U8 *IDAT_CRC = malloc(4 * sizeof(U8));
    U8 *IDAT_CRC_r = malloc(4 * sizeof(U8));
    memcpy(IDAT_CRC_r, &result->p_IDAT->crc, 4);
    k = 3;
    for (int i = 0; i < 4; i++)
    {
        IDAT_CRC[i] = IDAT_CRC_r[k];
        k--;
    }
    fwrite(IDAT_CRC, 4, 1, file);

    /*Write IEND length*/
    U8 *IEND_length = malloc(4 * sizeof(U8));
    U8 *IEND_length_r = malloc(4 * sizeof(U8));
    memcpy(IEND_length_r, &result->p_IEND->length, 4);
    k = 3;
    for (int i = 0; i < 4; i++)
    {
        IEND_length[i] = IEND_length_r[k];
        k--;
    }
    fwrite(IEND_length, 4, 1, file);

    /*Write IEND type*/
    fwrite(result->p_IEND->type, 4, 1, file);

    /*Write IDAT CRC*/
    U8 *IEND_CRC = malloc(4 * sizeof(U8));
    U8 *IEND_CRC_r = malloc(4 * sizeof(U8));
    memcpy(IEND_CRC_r, &result->p_IEND->crc, 4);
    k = 3;
    for (int i = 0; i < 4; i++)
    {
        IEND_CRC[i] = IEND_CRC_r[k];
        k--;
    }

    fwrite(IEND_CRC, 4, 1, file);

    free(result->p_IDAT->p_data);
    free(result->p_IHDR->p_data);
    free(result);

    fclose(file);
}