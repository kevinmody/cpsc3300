#include <stdio.h>

int main() {
    int i = 0;
    int address = 0;
    int memory[500];
    int* m = memory;


    printf("reading bytes in octal from stdin\n");

    FILE* textfile;
    char text;
    textfile = fopen("inputfile2.txt", "r");

    int j;

    while(fscanf(textfile, "%o", &j) != EOF) {
        if(i%2==1){
            memory[address] = j << 8 | memory[address];
            address++;
        }
        else{
            memory[address] = j;
        }
        ++i;
    }
    if (i % 2 == 1) {
        address++;
    }

    printf("memory contents displayed as 16-bit words\n");
    printf("in octal, little-endian format\n");
    printf("   memory  | memory\n");
    printf("  contents | address\n");
    printf("+----------|---------+\n");
    for (i = 0; i < address; i++) {
        printf("  %06o   | ", memory[i]);
        printf("  %p\n", m + i);
        printf("+----------|---------+\n");
    }


    return 0;
}