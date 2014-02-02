/**
 *  Copyright (C) 2014, David Everlöf
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "dawgminify.h"

int bit_masks[8] = {
    0x00000001, // 00000001
    0x00000003, // 00000011
    0x00000007, // 00000111
    0x0000000F, // 00001111
    0x0000001F, // 00011111
    0x0000003F, // 00111111
    0x0000007F, // 01111111
    0x000000FF, // 11111111
};

int byte_to_int_offs(const char* in, const int offset)
{
    return    ( (0xFF & in[offset + 3]) << 24) +
            ( (0xFF & in[offset + 2]) << 16) +
            ( (0xFF & in[offset + 1]) << 8)  +
            ( (0xFF & in[offset + 0]));
}

void check_ptr(void *ptr)
{
    if ( ptr == NULL )
    {
        printf ("Alloc failed.\n");
        exit (EXIT_FAILURE);
    }
}

void printBits(unsigned int num){
    unsigned int size = sizeof(unsigned int);
    unsigned int maxPow = 1 << (size * 8 - 1);
    int i=0;
    for(; i < size * 8 ;++i){
        if ( i % 8 == 0 )
        {
            printf(" ");
        }
        printf("%u", num & maxPow ? 1 : 0);
        num = num<<1;
    }
}

char* read_file(const char* filename, long* size)
{
    FILE *fp;
    char *buffer;

    fp = fopen ( filename , "rb" );
    if( !fp ) perror(filename),exit(1);

    fseek( fp , 0L , SEEK_END);
    *size = ftell( fp );
    rewind( fp );

    buffer = malloc( *size + 1 );
    check_ptr(buffer);

    if( 1 != fread( buffer , *size, 1 , fp) )
    {
        fclose(fp);
        free(buffer);
        printf("Coudlnt read file.\n");
        exit(1);
    }

    fclose(fp);
    return buffer;
}

/**
    write_bits
    
    Writes bits from an int to a char*

    val             value to read from
    nbr_bits        how many bits to write to output from val
    output          char buffer to read from
    output_pos      position in output buffer
    output_bitpos   bit position in output[*output_pos]
    
*/
void write_bits(int val, char nbr_bits, char* output, int* output_pos, int* output_bitpos)
{
    int will_be_written;
    int bits_written = 0;
    int bits_left = nbr_bits;
    
    while ( bits_written < nbr_bits )
    {
        bits_left = nbr_bits - bits_written;
        will_be_written = bits_left > (BITS_IN_BYTE - *output_bitpos) ? (BITS_IN_BYTE - *output_bitpos) : bits_left;
        output[*output_pos] |= (val >> bits_written) << *output_bitpos;
        *output_bitpos += will_be_written;
        bits_written += will_be_written;

        if ( *output_bitpos == BITS_IN_BYTE )
        {
            *output_bitpos = 0;
            (*output_pos)++;
        }
    }
}

/**
    read_bits
    
    Read bits from a buffer into val
    
    val             puts value here
    nbr_bits        how many bits to read
    input           buffer to read
    input_po        byte position in input
    input_bitpos    bit position in input[*input_pos]

*/
void read_bits(int* val, char nbr_bits, char* input, int* input_pos, int* input_bitpos)
{
    *val = 0;
    
    int will_be_read;
    int bits_read = 0;
    int bits_left = nbr_bits;
    
    while ( bits_read < nbr_bits )
    {
        bits_left = nbr_bits - bits_read;
        will_be_read = bits_left > (BITS_IN_BYTE - *input_bitpos) ? (BITS_IN_BYTE - *input_bitpos) : bits_left;
        *val |= (bit_masks[will_be_read - 1] & (input[*input_pos] >> *input_bitpos)) << bits_read;
        *input_bitpos += will_be_read;
        bits_read += will_be_read;
        
        if ( *input_bitpos == BITS_IN_BYTE )
        {
            *input_bitpos = 0;
            (*input_pos)++;
        }
    }
}

/*
    node_from_4byte
    
    Extracts letter, index and the flags from a node.
    
    node            the node to read from
    letter          the letter is written to this
    index           the index is written to this
    word_flag       the word flag is written to this
    end_flag        the end of list flag is written to this

*/
void node_from_4byte(int node, int* letter, int* index, int* word_flag, int* end_flag)
{
    *letter = KLetterMask & node;
    *index = (KChildIndexMask & node) >> 8;
    *word_flag = (node & KEndOfWordFlag) > 0;
    *end_flag = (node & KEndOfListFlag) > 0;
}

/**
    node_from_arb
    
    Extracts letter, index, and the flags from "compressed" node.
    
    arr                 the buffer to read from
    pos                 the bytepos in arr
    bitpos              the bitpos in arr[*pos]
    bits_for_index      how many bits used to represent the index
    letter              the letter is written to this
    index               the index is written to this
    word_flag           the word flag is written to this
    end_flag            the end flag is written to this
    
*/
void node_from_arb(char* arr, int* pos, int* bitpos, int bits_for_index, int* letter, int* index, int* word_flag, int* end_flag)
{
    read_bits(word_flag, WORD_MASK_LENGTH, arr, pos, bitpos);
    read_bits(end_flag, END_MASK_LENGTH, arr, pos, bitpos);
    read_bits(letter, CHAR_MASK_LENGTH, arr, pos, bitpos);
    read_bits(index, bits_for_index, arr, pos, bitpos);
}


void write_node(int node, char* arr, int* pos, int* bitpos, int bits_for_index)
{
    int letter;
    int index;
    int word_flag;
    int end_flag;
    
    node_from_4byte(node, &letter, &index, &word_flag, &end_flag);
    
    write_bits(word_flag, WORD_MASK_LENGTH, arr, pos, bitpos);
    write_bits(end_flag, END_MASK_LENGTH, arr, pos, bitpos);
    write_bits(letter, CHAR_MASK_LENGTH, arr, pos, bitpos);
    write_bits(index, bits_for_index, arr, pos, bitpos);
}

char* encode(char* in, size_t in_size, size_t* out_size)
{
    int node;
    int pos;
    int byte_pos;
    int bit_pos;
    
    int nbr_nodes = byte_to_int_offs(in, 0);
    int bits_for_index = (int)(ceil(log(nbr_nodes) / log(2.0)));
    int bits_per_node = WORD_MASK_LENGTH + CHAR_MASK_LENGTH + END_MASK_LENGTH + bits_for_index;
    int odd_bits = (bits_per_node * nbr_nodes) % 8;
    int total_bytes = (bits_per_node * nbr_nodes + odd_bits) / 8 + 4;
    
    printf("Nodes:               %d\n", nbr_nodes);
    printf("Bits/index:          %d\n", bits_for_index);
    printf("Bits/node:           %d\n", bits_per_node);
    printf("Odd bits:            %d\n", odd_bits);
    printf("Bytes in new file:   %d\n", total_bytes);
    printf("%% smaller:           %.3f%%\n", 1 - (total_bytes/(float)in_size));
    
    char* out = (char*) malloc( total_bytes );
    check_ptr(out);
    
    memcpy(out, in, 4);
    pos = 4; // Skip first 4 since its for nodecount
    byte_pos = 4;
    bit_pos = 0;
    
    while ( pos < in_size )
    {
        node = byte_to_int_offs(in, (int)pos);
        write_node(node, out, &byte_pos, &bit_pos, bits_for_index);
        pos += 4;
    }
    
    *out_size = total_bytes;
    return out;
}

char* decode(char* in, size_t in_size, size_t* out_size)
{
    int letter;
    int index;
    int word_flag;
    int end_flag;
    int byte_pos;
    int bit_pos;
    int node_pos;

    int nbr_nodes = byte_to_int_offs(in, 0);
    int bits_for_index = (int)(ceil(log(nbr_nodes) / log(2.0)));
    *out_size = nbr_nodes * 4 + 4;
    
    int* out = (int*) malloc(*out_size);
    check_ptr(out);
    out[0] = nbr_nodes;
    
    byte_pos = 4;
    bit_pos = 0;
    node_pos = 1;
    
    while ( byte_pos < in_size )
    {
        node_from_arb(in, &byte_pos, &bit_pos, bits_for_index, &letter, &index, &word_flag, &end_flag);
        
        out[node_pos] = index << KChildBitShift;
        out[node_pos] += KLetterMask & letter;
        if (word_flag) out[node_pos] += KEndOfWordFlag;
        if (end_flag) out[node_pos] += KEndOfListFlag;
        
        node_pos++;
    }
    
    return (char*) out;
}

void selftest()
{
    // TEST write_bits
    
    int pos = 0;
    int bit_pos = 0;
    
    char* arr = (char*) malloc(4);
    char* crr = (char*) malloc(4);
    if ( arr == NULL || crr == NULL )
    {
        printf("malloc failed\n");
        exit(1);
    }
    
    // 10001111 10101010 00000000 11111111
    int correct = 0x8FAA00FF;
    
    //                                  ..
    // 00000000 00000000 00000000 00000011
    write_bits(0xFF, 2, arr, &pos, &bit_pos);    
    
    //                            ......    
    // 00000000 00000000 00000000 11111111
    write_bits(0xFF, 6, arr, &pos, &bit_pos);    
    
    //                           
    // 00000000 00000000 00000000 11111111
    write_bits(0xFF, 0, arr, &pos, &bit_pos);
    
    //                 . ........
    // 00000000 00000010 00000000 11111111
    write_bits(0x00, 9, arr, &pos, &bit_pos);
    
    //            .....
    // 00000000 00101010 00000000 11111111
    write_bits(0x15, 5, arr, &pos, &bit_pos);
    
    //        . ..
    // 00000001 10101010 00000000 11111111
    write_bits(0x06, 3, arr, &pos, &bit_pos);

    // .......
    // 10001111 10101010 00000000 11111111
    write_bits(0x47, 7, arr, &pos, &bit_pos);
    
    assert(!(correct ^ byte_to_int_offs(arr, 0)));
    printf("OK: write_bits()\n");
    
    free(arr);
    free(crr);
    
    // END write_bits TEST
    
    // TEST Simple encode one node
    
    // Sample node "nocArr" Letter A, Index: 2, No wordflag, Yes endflag
    // Encode it and then decode it and make sure it's correct
    
    int res_letter;
    int res_index;
    int res_word_flag;
    int res_end_flag;
    int bits_for_index = 6;
    char* nocArr = (char*) malloc(4);
    char* testArr = (char*) malloc(4);
    if ( nocArr == NULL || testArr == NULL )
    {
        printf("malloc failed\n");
        exit(1);
    }
    
    nocArr[0] = 0x41; // A
    nocArr[1] = 0x02; // Index 2
    nocArr[2] = 0x00;
    nocArr[3] = 0x10; // Endflag, no wordflag
    
    // Write the node to the "testArr"
    pos = 0;
    bit_pos = 0;
    write_node(byte_to_int_offs(nocArr, 0), testArr, &pos, &bit_pos, bits_for_index);
    
    // Read the node from "testArr"
    pos = 0;
    bit_pos = 0;
    node_from_arb(testArr, &pos, &bit_pos, bits_for_index, &res_letter, &res_index, &res_word_flag, &res_end_flag);
    
    assert((char)res_letter == 'A');
    assert(res_index == 2);
    assert((0x0000000F & res_word_flag) == 0);
    assert((0x0000000F & res_end_flag) == 1);
    printf("OK: Simple encode one node\n");
    
    free(nocArr);
    free(testArr);
    
    // END Simple encode one node TEST
    
    
    // TEST Encode arr with words AR and AB
    // File contains: 04 00 00 00 00 00 00 00 41 02 00 10 52 00 00 20 42 00 00 30
    
    nocArr = (char*) malloc(12);
    testArr = (char*) malloc(12);
    if ( nocArr == NULL || testArr == NULL )
    {
        printf("malloc failed\n");
        exit(1);
    }
    
    // Node 1
    nocArr[0] = 0x41; // A
    nocArr[1] = 0x02; // Index 2
    nocArr[2] = 0x00;
    nocArr[3] = 0x10; // Endflag, no wordflag
    
    // Node 2
    nocArr[4] = 0x52; // R
    nocArr[5] = 0x00; // Index 0
    nocArr[6] = 0x00;
    nocArr[7] = 0x20; // No endflag, wordflag
    
    // Node 3
    nocArr[8] = 0x42; // B
    nocArr[9] = 0x00; // Index 0
    nocArr[10] = 0x00;
    nocArr[11] = 0x30; // Endflag and wordflag
    
    pos = 0;
    bit_pos = 0;
    write_node(byte_to_int_offs(nocArr, 0), testArr, &pos, &bit_pos, bits_for_index);
    write_node(byte_to_int_offs(nocArr, 4), testArr, &pos, &bit_pos, bits_for_index);
    write_node(byte_to_int_offs(nocArr, 8), testArr, &pos, &bit_pos, bits_for_index);
    
    
    // Read the node from "testArr"
    pos = 0;
    bit_pos = 0;
    node_from_arb(testArr, &pos, &bit_pos, bits_for_index, &res_letter, &res_index, &res_word_flag, &res_end_flag);
    assert((char)res_letter == 'A');
    assert(res_index == 2);
    assert((0x0000000F & res_word_flag) == 0);
    assert((0x0000000F & res_end_flag) == 1);
    
    node_from_arb(testArr, &pos, &bit_pos, bits_for_index, &res_letter, &res_index, &res_word_flag, &res_end_flag);
    assert((char)res_letter == 'R');
    assert(res_index == 0);
    assert((0x0000000F & res_word_flag) == 1);
    assert((0x0000000F & res_end_flag) == 0);
    
    node_from_arb(testArr, &pos, &bit_pos, bits_for_index, &res_letter, &res_index, &res_word_flag, &res_end_flag);
    assert((char)res_letter == 'B');
    assert(res_index == 0);
    assert((0x0000000F & res_word_flag) == 1);
    assert((0x0000000F & res_end_flag) == 1);
    printf("OK: Encode array with \"AR\" and \"AB\"\n");
    
    printBits(byte_to_int_offs(nocArr, 0));
    printBits(byte_to_int_offs(nocArr, 4));
    printBits(byte_to_int_offs(nocArr, 8));
    
    printf("\n");
    printBits(byte_to_int_offs(testArr, 0));
    printBits(byte_to_int_offs(testArr, 4));
    printBits(byte_to_int_offs(testArr, 8));
    
    free(nocArr);
    free(testArr);
    
    // END Encode arr with words AR and AB TEST
    
}

void write_buff_to_file(const char* filename, char* buff, size_t buff_size)
{
    size_t b_written = 0;
    FILE* fp = fopen (filename, "ab");
    while ( b_written < buff_size )
    {
        fputc (buff[(int)b_written], fp);
        b_written++;
    }
    fclose (fp);
}

void trim(char *line){
    size_t line_len = strlen(line);
    if ( line[line_len - 2] == '\r' ) line[line_len - 2] = '\0';
    else if ( line[line_len - 1] == '\n' ) line[line_len - 1] = '\0';
}

void debug_dawg_to_file(char* dict, long size)
{
    int letter;
    int index;
    int word_flag;
    int end_flag;
    int node;
    int byte_pos = 8;
    int current_node = 1;
    FILE * fp;

    fp = fopen ("debug.out.txt", "w");
    
    while ( byte_pos < size )
    {
        node = byte_to_int_offs(dict, byte_pos);
        node_from_4byte(node, &letter, &index, &word_flag, &end_flag);
        fprintf (fp, "%d: Letter: %c (%d), is_word: %d, is_end: %d, Index: %d\n", current_node, (char)letter, letter, word_flag, end_flag, index);
        current_node++;
        byte_pos += 4;
    }
    
    fclose (fp);
}

/*
int main (int argc, char *argv[])
{
    
    if ( argc > 1 && strlen(argv[1]) == 1 && argv[1][0] == 't' )
    {
        selftest();
        exit(0);
    }
    
    long size;
    char* buffer = read_file(argv[1], &size);
    
    int letter;
    int index;
    int word_flag;
    int end_flag;
    
    size_t encoded_size;
    char* encoded = encode(buffer, (size_t) size, &encoded_size);
    
    size_t decoded_size;
    char* decoded = decode(encoded, encoded_size, &decoded_size);
    
    
    char *text = calloc(1,1), inbuffer[20];
    while( fgets(inbuffer, 20 , stdin) ) // break with ^D or ^Z
    {
        text = realloc( text, strlen(text)+1+strlen(inbuffer) );
        strcat( text, inbuffer ); // note a '\n' is appended here everytime
        trim(inbuffer);
        int result = lookup((int*) decoded, inbuffer);
        printf("%s, is word? %d\n", inbuffer, result);
        if ( !result )
        {
            printf("%s, is word? %d\n", inbuffer, result);
            exit(1);
        }
    }
    
    
    // write_buff_to_file("out_decoded", decoded, decoded_size);
    
    int n = memcmp(buffer, decoded, size);
    assert(n == 0);
    printf("memcmp ok: decode was same as input");
    
    free(buffer);
    free(encoded);
    free(decoded);
}*/