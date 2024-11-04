#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void convert_8bit_to_4bit(const char* input_file, const char* output_file) {
    FILE *fin, *fout;
    uint8_t input_byte, output_byte;
    int bytes_read;
    int is_first_nibble = 1;
    
    // Open input file in binary read mode
    fin = fopen(input_file, "rb");
    if (fin == NULL) {
        fprintf(stderr, "Error: Cannot open input file %s\n", input_file);
        exit(1);
    }
    
    // Open output file in binary write mode
    fout = fopen(output_file, "wb");
    if (fout == NULL) {
        fprintf(stderr, "Error: Cannot open output file %s\n", output_file);
        fclose(fin);
        exit(1);
    }
    
    output_byte = 0;
    
    // Read input file byte by byte
    while ((bytes_read = fread(&input_byte, 1, 1, fin)) > 0) {
        // Convert 8-bit value to 4-bit by dividing by 16
        uint8_t nibble = input_byte >> 4;
        
        if (is_first_nibble) {
            // Store in high nibble
            output_byte = nibble << 4;
            is_first_nibble = 0;
        } else {
            // Combine with previous high nibble and write
            output_byte |= nibble;
            fwrite(&output_byte, 1, 1, fout);
            is_first_nibble = 1;
        }
    }
    
    // If we ended with a high nibble, write the final byte
    if (!is_first_nibble) {
        fwrite(&output_byte, 1, 1, fout);
    }
    
    fclose(fin);
    fclose(fout);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        fprintf(stderr, "Converts 8-bit raw audio to 4-bit raw format\n");
        return 1;
    }
    
    convert_8bit_to_4bit(argv[1], argv[2]);
    printf("Conversion complete\n");
    
    return 0;
}
