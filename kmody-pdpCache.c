//Author: Kevin Mody


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define WORD_SIZE 32*512

#define LINES_PER_BANK 16


/* struct to help organize source and destination operand handling */
typedef struct ap{
  int mode;
  int reg;
  int addr ;  /* used only for modes 1-7 */
  int value;
} addr_phrase_t;

int mem[WORD_SIZE];
int reg[8] = {0};
int ir;
addr_phrase_t src, dst;

int halt;
int n_psw;
int z_psw;
int v_psw;
int c_psw;
int sign_bit;
int result;
int offset;
int byte_count;

int print_mode=0;
//global variables - summary exec statistics
int instructionCount = 0;
int instructionFetchCount = 0;
int dataReadCount = 0;
int dataWriteCount = 0;
int branchCount = 0;
int branchTakenCount = 0;



unsigned int
  plru_state[LINES_PER_BANK],  /* current state for each set */
  valid[4][LINES_PER_BANK],    /* valid bit for each line    */
  dirty[4][LINES_PER_BANK],    /* dirty bit for each line    */
  tag[4][LINES_PER_BANK],      /* tag bits for each line     */

  plru_bank[8] /* table for bank replacement choice based on state */

                 = { 0, 0, 1, 1, 2, 3, 2, 3 },

  next_state[32] /* table for next state based on state and bank ref */
                 /* index by 5-bit (4*state)+bank [=(state<<2)|bank] */

                                    /*  bank ref  */
                                    /* 0  1  2  3 */

                 /*         0 */  = {  6, 4, 1, 0,
                 /*         1 */       7, 5, 1, 0,
                 /*         2 */       6, 4, 3, 2,
                 /* current 3 */       7, 5, 3, 2,
                 /*  state  4 */       6, 4, 1, 0,
                 /*         5 */       7, 5, 1, 0,
                 /*         6 */       6, 4, 3, 2,
                 /*         7 */       7, 5, 3, 2  };

unsigned int
    cache_reads,  /* counter */
    cache_writes, /* counter */
    hits,         /* counter */
    misses,       /* counter */
    write_backs;  /* counter */




void cache_init( void ){
  int i;
  for( i=0; i<LINES_PER_BANK; i++ ){
    plru_state[i] = 0;
    valid[0][i] = dirty[0][i] = tag[0][i] = 0;
    valid[1][i] = dirty[1][i] = tag[1][i] = 0;
    valid[2][i] = dirty[2][i] = tag[2][i] = 0;
    valid[3][i] = dirty[3][i] = tag[3][i] = 0;
  }
  cache_reads = cache_writes = hits = misses = write_backs = 0;
}

void cache_stats( void ){
  printf( "cache statistics (in decimal):\n" );
  printf( "  cache reads       = %d\n", cache_reads );
  printf( "  cache writes      = %d\n", cache_writes );
  printf( "  cache hits        = %d\n", hits );
  printf( "  cache misses      = %d\n", misses );
  printf( "  cache write backs = %d\n", write_backs );
}

void cache_access( unsigned int address, unsigned int type ){

  unsigned int
    addr_tag,    /* tag bits of address     */
    addr_index,  /* index bits of address   */
    bank;        /* bank that hit, or bank chosen for replacement */

  if( type == 0 ){
    cache_reads++;
  }else{
    cache_writes++;
  }

//change these ????

  // addr_index = (address >> 5) & 0x1f;
  // addr_tag = address >> 10;

  addr_index = (address >> 3) & 0xf;
  addr_tag = address >> 7 & 0x7ff;

  /* check bank 0 hit */

  if( valid[0][addr_index] && (addr_tag==tag[0][addr_index]) ){
    hits++;
    bank = 0;

  /* check bank 1 hit */

  }else if( valid[1][addr_index] && (addr_tag==tag[1][addr_index]) ){
    hits++;
    bank = 1;

  /* check bank 2 hit */

  }else if( valid[2][addr_index] && (addr_tag==tag[2][addr_index]) ){
    hits++;
    bank = 2;

  /* check bank 3 hit */

  }else if( valid[3][addr_index] && (addr_tag==tag[3][addr_index]) ){
    hits++;
    bank = 3;

  /* miss - choose replacement bank */

  }else{
    misses++;

         if( !valid[0][addr_index] ) bank = 0;
    else if( !valid[1][addr_index] ) bank = 1;
    else if( !valid[2][addr_index] ) bank = 2;
    else if( !valid[3][addr_index] ) bank = 3;
    else bank = plru_bank[ plru_state[addr_index] ];

    if( valid[bank][addr_index] && dirty[bank][addr_index] ){
      write_backs++;
    }

    valid[bank][addr_index] = 1;
    dirty[bank][addr_index] = 0;
    tag[bank][addr_index] = addr_tag;
  }

  /* update replacement state for this set (i.e., index value) */

  plru_state[addr_index] = next_state[ (plru_state[addr_index]<<2) | bank ];

  /* update dirty bit on a write */

  if( type == 1 ) dirty[bank][addr_index] = 1;
}

void readFile(int print_mode){
  unsigned i = 0;
  int flag = scanf("%o", &ir);

  if(print_mode == 2){
    printf("\nreading words in octal from stdin:\n");
  }
  while(flag != EOF){
    if(print_mode == 2){
      //printf("  %04o: %06o\n", byte_count, ir);
      printf("  0%06o\n", ir);
    }
    mem[i] = ir;
    //cache_access(reg[7],1);
    i++;
    byte_count += 2;
    flag = scanf("%o", &ir);
  }
  if(print_mode == 2){
    printf("\ninstruction trace:\n");
  }
}

void get_operand( addr_phrase_t *phrase ){
  assert( (phrase->mode >= 0) && (phrase->mode <= 7) );
  assert( (phrase->reg  >= 0) && (phrase->reg  <= 7) );
  switch( phrase->mode ) {

    /* register */
    case 0:
      phrase->value = reg[ phrase->reg ];
      assert( phrase->value < 0200000 );
      phrase->addr = 0;
      break;

    /* register Deferred */
    case 1:
      phrase->addr = reg[ phrase->reg ];
      assert( phrase->value < 0200000 );
      cache_access(phrase->addr,0);
      phrase->value = mem[phrase->addr >> 1];
      //cache_access(phrase->addr,0);
      instructionFetchCount++;
      dataReadCount++;
      break;

    /* autoincrement (post reference) */
    case 2:
      //printf("-----GET OPERAND CASE 2-----\n");
      phrase->addr = reg[ phrase->reg ];
      assert( phrase->addr < 0200000 );
      cache_access(phrase->addr,0);
      phrase->value = mem[ phrase->addr >> 1 ];  /* adjust to word addr */
      //cache_access(phrase->addr,0);
      assert( phrase->value < 0200000 );
      reg[ phrase->reg ] = ( reg[ phrase->reg ] + 2 ) & 0177777;
      instructionFetchCount++;
      break;

    /* autoincrement Deferred */
    case 3:

      //printf("-----GET OPERAND CASE 3-----\n");
      phrase->addr = reg[ phrase->reg ];
      assert( phrase->addr < 0200000 );
      cache_access(phrase->addr,0);
      phrase->value = mem[ phrase->addr >> 1 ];  /* adjust to word addr */
      //cache_access(phrase->addr,0);
      assert( phrase->addr < 0200000 );

      reg[ phrase->reg ] = ( reg[ phrase->reg ] + 2 ) & 0177777;

      phrase->addr = reg[ phrase->reg ];
      assert( phrase->addr < 0200000 );
      cache_access(phrase->addr,0);
      phrase->value = mem[ phrase->addr ];  /* adjust to word addr */
      //cache_access(phrase->addr,0);
      assert( phrase->addr < 0200000 );

      if ( phrase->reg == 7 ) {
        instructionFetchCount++;
        dataReadCount++;
      } else {
        dataReadCount += 2;
      }
      break;

    /* autodecrement */
    case 4:
      //printf("-----GET OPERAND CASE 4-----\n");
      reg[ phrase->reg ] = ( reg[ phrase->reg ] - 2 ) & 0177777;
      phrase->addr = reg[ phrase->reg ];
      assert( phrase->addr < 0200000 );
      cache_access(phrase->addr,0);
      phrase->value = mem[ phrase->addr >> 1 ];  /* adjust to word addr */
      //cache_access(phrase->addr,0);
      assert( phrase->value < 0200000 );
      dataReadCount++;
      break;

    /* autodecrement Deferred */
    case 5:
      //printf("-----GET OPERAND CASE 5-----\n");
      reg[ phrase->reg ] = ( reg[ phrase->reg ] - 2 ) & 0177777;
      phrase->addr = reg[ phrase->reg ];
      assert( phrase->addr < 0200000 );

      if ( phrase->reg == 7 ) {
        instructionFetchCount++;
        dataReadCount++;
      } else {
        dataReadCount += 2;
      }
      break;

    /* index */
    case 6:
      //printf("-----GET OPERAND CASE 6-----\n");
      //phrase->value = operand
      //INSTRUCTION BLOCK
      phrase->addr = reg[ phrase->reg ];
      assert( phrase->value < 0200000 );

      //ADDRESS BLOCK
      phrase->value = mem[ phrase->addr ];  /* adjust to word addr */
      //cache_access(reg[7],0);
      assert( phrase->addr < 0200000 );

      //+ circle and address block
      phrase->addr = phrase->value + (( reg[ phrase->reg ] + 2 ) & 0177777);
      assert( phrase->addr < 0200000 );

      reg[7] += 2;
      dataReadCount += 2;
      break;

    /* index deferred */
    case 7:
    //printf("-----GET OPERAND CASE 7-----\n");
      //INSTRUCTION BLOCK
      phrase->addr = reg[ phrase->reg ];
      assert( phrase->value < 0200000 );

      //ADDRESS BLOCK
      cache_access(phrase->addr,0);
      phrase->value = mem[ phrase->addr ];  /* adjust to word addr */
      //cache_access(phrase->addr,0);
      assert( phrase->addr < 0200000 );

      //+ circle and address block
      phrase->addr = phrase->value + (( reg[ phrase->reg ] + 2 ) & 0177777);
      assert( phrase->addr < 0200000 );

      reg[7] += 2;
      dataReadCount += 2;
      break;

    default:
      printf("unimplemented address mode %o\n", phrase->mode);
  }
}


//read announcement and implement the following function 

void put_result(addr_phrase_t* phrase, int value) {
    assert((phrase->mode >= 0) && (phrase->mode <= 7));
    assert((phrase->reg >= 0) && (phrase->reg <= 7));
    switch (phrase->mode) {

        /* register */
    case 0:
        //phrase->value = reg[phrase->reg];
        //assert(phrase->value < 0200000);
        reg[phrase->reg] = value;
        phrase->addr = 0;
        break;

        /* register Deferred */
    case 1:
        phrase->addr = reg[phrase->reg];
        assert(phrase->value < 0200000);
        //phrase->value = mem[phrase->addr >> 1];
        mem[phrase->addr >> 1] = value;
        cache_access(phrase->addr, 1);
        //instructionFetchCount++;
        //dataReadCount++;
        break;

        /* autoincrement (post reference) */
    case 2:
        //printf("-----GET OPERAND CASE 2-----\n");
        phrase->addr = reg[phrase->reg];
        assert(phrase->addr < 0200000);
        //phrase->value = mem[phrase->addr >> 1];  // adjust to word addr
        mem[phrase->addr >> 1] = value;
        cache_access(phrase->addr, 1);
        assert(phrase->value < 0200000);
        reg[phrase->reg] = (reg[phrase->reg] + 2) & 0177777;
        //instructionFetchCount++;
        break;

        /* autoincrement Deferred */
    case 3:

        //printf("-----GET OPERAND CASE 3-----\n");
        phrase->addr = reg[phrase->reg];
        assert(phrase->addr < 0200000);
        //phrase->value = mem[phrase->addr >> 1];  // adjust to word addr
        mem[phrase->addr >> 1] = value;
        cache_access(phrase->addr, 1);
        assert(phrase->addr < 0200000);

        reg[phrase->reg] = (reg[phrase->reg] + 2) & 0177777;

        phrase->addr = reg[phrase->reg];
        assert(phrase->addr < 0200000);
        //phrase->value = mem[phrase->addr];  // adjust to word addr
        mem[phrase->addr] = value;
        cache_access(phrase->addr, 1);
        assert(phrase->addr < 0200000);

        if (phrase->reg == 7) {
            //instructionFetchCount++;
            //dataReadCount++;
        }
        else {
            //dataReadCount += 2;
        }
        break;

        /* autodecrement */
    case 4:
        //printf("-----GET OPERAND CASE 4-----\n");
        reg[phrase->reg] = (reg[phrase->reg] - 2) & 0177777;
        phrase->addr = reg[phrase->reg];
        assert(phrase->addr < 0200000);
        //phrase->value = mem[phrase->addr >> 1];  // adjust to word addr 
        mem[phrase->addr >> 1] = value;
        cache_access(phrase->addr, 1);
        assert(phrase->value < 0200000);
        //dataReadCount++;
        break;

        /* autodecrement Deferred */
    case 5:
        //printf("-----GET OPERAND CASE 5-----\n");
        reg[phrase->reg] = (reg[phrase->reg] - 2) & 0177777;
        phrase->addr = reg[phrase->reg];
        assert(phrase->addr < 0200000);

        if (phrase->reg == 7) {
            //instructionFetchCount++;
            //dataReadCount++;
        }
        else {
            //dataReadCount += 2;
        }
        break;

        /* index */
    case 6:
        //printf("-----GET OPERAND CASE 6-----\n");
        //phrase->value = operand
        //INSTRUCTION BLOCK
        phrase->addr = reg[phrase->reg];
        assert(phrase->value < 0200000);

        //ADDRESS BLOC
        //phrase->value = mem[phrase->addr];  // adjust to word addr
        mem[phrase->addr] = value;
        cache_access(phrase->addr, 1);
        assert(phrase->addr < 0200000);

        //+ circle and address block
        phrase->addr = phrase->value + ((reg[phrase->reg] + 2) & 0177777);
        assert(phrase->addr < 0200000);

        reg[7] += 2;
        //dataReadCount += 2;
        break;

        /* index deferred */
    case 7:
        //printf("-----GET OPERAND CASE 7-----\n");
          //INSTRUCTION BLOCK
        phrase->addr = reg[phrase->reg];
        assert(phrase->value < 0200000);

        //ADDRESS BLOCK
        //cache_access(phrase->addr, 0);
        //phrase->value = mem[phrase->addr];  // adjust to word addr

        assert(phrase->addr < 0200000);

        //+ circle and address block
        phrase->addr = phrase->value + ((reg[phrase->reg] + 2) & 0177777);
        assert(phrase->addr < 0200000);

        mem[phrase->addr] = value;
        cache_access(phrase->addr, 1);

        reg[7] += 2;
        //dataReadCount += 2;
        break;

    default:
        printf("unimplemented address mode %o\n", phrase->mode);
    }
}

void printRegisters() {
  printf("  R0:%07o  R2:%07o  R4:%07o  R6:%07o\n", reg[0], reg[2], reg[4], reg[6]);
  printf("  R1:%07o  R3:%07o  R5:%07o  R7:%07o\n", reg[1], reg[3], reg[5], reg[7]);
}

void update_operand( addr_phrase_t *phrase, int result ) {
    get_operand(phrase);
  if ( phrase->mode == 0 ) {
    reg[phrase->reg] = result;
  } else {
    mem[phrase->addr] = result;
    //cache_access(reg[7],1);
  }
}

void fetch(int printmode) {
  /* fetch – note that reg[7] in PDP-11 is the PC */
if(printmode > 0){
  printf("at 0%04o, ", reg[7]);
}
  cache_access(reg[7],0);
  ir = mem[ reg[7] >> 1 ];  /* adjust for word address */
  //cache_access(reg[7],0);
  assert( ir < 0200000 );
  reg[7] = ( reg[7] + 2 ) & 0177777;

  /* extract the fields for the addressing modes          */
  /*   (done whether src and dst are used or not; could   */
  /*   be moved instead to indiv. inst. execution blocks) */

  src.mode = (ir >> 9) & 07;  /* three one bits in the mask */
  src.reg = (ir >> 6) & 07;   /* decimal 7 would also work  */
  dst.mode = (ir >> 3) & 07;
  dst.reg = ir & 07;

  instructionFetchCount++;
}

//------------------------------------------------------------
//  main


int main(int argc, char* argv[]) {
//int main() {

    //int argc = 2;
    //char* argv[] = { "./a.out", "-o" };

    cache_init();

    if (argc == 2) {
        if (strcmp(argv[1], "-v") == 0) {
            print_mode = 2;

        }
        else if (strcmp(argv[1], "-t") == 0) {
            printf("\ninstruction trace:\n");
            print_mode = 1;
        }
        else {
            print_mode = 0;
        }
    }
    else {
        print_mode = 0;
    }

    readFile(print_mode);

    while (!halt) {

        fetch(print_mode);

        /* decode using a series of dependent if statements */
        /*   and execute the identified instruction         */

        if (ir == 0) {
            if (print_mode > 0) {
                printf("halt instruction\n");
                if (print_mode == 2) {
                    printRegisters();
                }
            }

            //printRegisters();
            halt = 1;
            instructionCount++;

        }
        else if ((ir >> 12) == 01) {   /* LSI-11 manual ref 4-25 */
        //MOV
        //printf("mov instruction ");


            int tempReg = reg[dst.reg];
            //if (dst.mode == 2) {
            if (dst.mode > 2) {
                get_operand(&src);
                int tempMem = tempReg >> 1;
                //cache_access(reg[7],1);
                mem[tempMem] = src.value;
                //cache_access(reg[7],1);
                put_result(&dst, src.value);
                //dataWriteCount++;
                //instructionFetchCount--;
            }
            else {
                get_operand(&src);
                put_result(&dst, src.value);
                //dataWriteCount++;
                dst.value = src.value;
                result = dst.value;
                //instructionFetchCount-=2;
                //update_operand(&dst, result);
            }

            if (dst.mode > 0) {
                dataWriteCount++;
            }

            result = result & 0177777;
            sign_bit = result & 0100000;

            n_psw = 0; if (sign_bit) n_psw = 1;
            z_psw = 0; if (src.value == 0) z_psw = 1;
            v_psw = 0;
            c_psw = 0;

            if (print_mode > 0) {
                printf("mov instruction sm %d, sr %d dm %d dr %d\n", src.mode, src.reg, dst.mode, dst.reg);
                if (print_mode == 2) {
                    printf("  src.value = 0%06o\n", src.value);
                    printf("  nzvc bits = 4'b%o%o%o%o\n", n_psw, z_psw, v_psw, c_psw);
                    //printRegisters();
                }
            }

            // printf("sm %o, sr %o dm %o dr %o\n", src.mode, src.reg, dst.mode, dst.reg);


            if (dst.mode == 2) {
                if (print_mode > 1) {
                    printf("  value 0%06o is written to 0%06o\n", src.value, tempReg);
                }

                //dataWriteCount++;
            }
            if (print_mode == 2) {
                printRegisters();
            }
            // printRegisters();
            instructionCount++;

        }
        else if ((ir >> 12) == 02) {   /* LSI-11 manual ref 4-26 */
        //CMP

            get_operand(&src);
            get_operand(&dst);

            result = src.value - dst.value;

            c_psw = (result & 0200000) >> 16;
            result = result & 0177777;

            sign_bit = result & 0100000;
            n_psw = 0; if (sign_bit) n_psw = 1;
            z_psw = 0; if (result == 0) z_psw = 1;
            v_psw = 0;
            if (((src.value & 0100000) != (dst.value & 0100000)) && ((dst.value & 0100000) == (result & 0100000))) {
                v_psw = 1;
            }

            //found 3 ===== 12 
          //reg[1]  = 0000003;
        //   if (print_mode == 2) {
        //             printf("  src.value = 0%06o\n", src.value);
        //             printf("  dst.value = 0%06o\n", dst.value);
        //            // printf("  result    = 0%06o\n", result);
        //             printf("  nzvc bits = 4'b%1d%1d%1d%1d\n", n_psw, z_psw, v_psw, c_psw);

        //             printRegisters();
        // }

            instructionCount++;

        }
        else if ((ir >> 12) == 06) {   /* LSI-11 manual ref 4-27 */

      // printf("add instruction ");

            get_operand(&src);
            get_operand(&dst);

            result = dst.value + src.value;

            c_psw = (result & 0200000) >> 16;
            result = result & 0177777;

            sign_bit = result & 0100000;
            n_psw = 0; if (sign_bit) n_psw = 1;
            z_psw = 0; if (result == 0) z_psw = 1;
            v_psw = 0;
            if (((src.value & 0100000) == (dst.value & 0100000)) && ((src.value & 0100000) != (result & 0100000))) {
                v_psw = 1;
            }



            if (print_mode > 0) {
                printf("add instruction sm %d, sr %d dm %d dr %d\n", src.mode, src.reg, dst.mode, dst.reg);
            }
            //  printf("sm %o, sr %o dm %o dr %o\n", src.mode, src.reg, dst.mode, dst.reg);

                //found 2 
                // if (print_mode == 2) {
                //         printf("  src.value = 0%06o\n", src.value);
                //         printf("  dst.value = 0%06o\n", dst.value);
                //         printf("  result    = 0%06o\n", result);
                //         printf("  nzvc bits = 4'b%1d%1d%1d%1d\n", n_psw, z_psw, v_psw, c_psw);
                //         //printf("printing register for test\n");
                //         printRegisters();
                // }

            update_operand(&dst, result);
            //  printRegisters();

            instructionCount++;

        }
        else if ((ir >> 12) == 016) {  /* LSI-11 manual ref 4-28 */
     //SUB

            get_operand(&src);
            get_operand(&dst);

            result = dst.value - src.value;

            c_psw = (result & 0200000) >> 16;
            result = result & 0177777;

            sign_bit = result & 0100000;
            n_psw = 0; if (sign_bit) n_psw = 1;
            z_psw = 0; if (result == 0) z_psw = 1;
            v_psw = 0;
            if (((src.value & 0100000) != (dst.value & 0100000)) && ((src.value & 0100000) == (result & 0100000))) {
                v_psw = 1;
            }



            if (print_mode > 0) {
                printf("sub instruction sm %d, sr %d dm %d dr %d\n", src.mode, src.reg, dst.mode, dst.reg);
            }


            //found 1 at 12
            reg[1] = result;
            if (print_mode == 2) {
                printf("  src.value = 0%06o\n", src.value);
                printf("  dst.value = 0%06o\n", dst.value);
                printf("  result    = 0%06o\n", result);
                printf("  nzvc bits = 4'b%1d%1d%1d%1d\n", n_psw, z_psw, v_psw, c_psw);
                // printf("printing register for test\n");
                printRegisters();
            }

            update_operand(&dst, result);
            // printRegisters();

            instructionCount++;

        }
        else if ((ir >> 9) == 077) {  /* LSI-11 manual ref 4-61 */
        //SOB


            offset = ir & 037;
            get_operand(&dst);


            reg[src.reg] -= 1;
            result = reg[src.reg];

            if (print_mode > 0) {
                //printf("sob instruction reg %d with offset %03o\n", dst.reg, offset);
                printf("sob instruction reg %d with offset %03o\n", src.reg, offset);
            }


            offset = offset << 24;       /* sign extend to 32 bits */
            offset = offset >> 24;

            if (result != 0) {
                reg[7] = (reg[7] - (offset << 1)) & 0177777;
                branchTakenCount++;
            }
            branchCount++;
            instructionCount++;

            if (print_mode == 2) {
                printRegisters();
            }
            //printRegisters();

        }
        else if ((ir >> 8) == 001) {  /* LSI-11 manual ref 4-35 */
         //BR

            offset = ir & 0377;          /* 8-bit signed offset */

            if (print_mode > 0) {
                printf("br instruction with offset 0%03o\n", offset);
            }
            // printf("with offset 0%03o\n", offset);
            offset = offset << 24;       /* sign extend to 32 bits */
            offset = offset >> 24;
            reg[7] = (reg[7] + (offset << 1)) & 0177777;

            branchTakenCount++;
            branchCount++;
            instructionCount++;


        }
        else if ((ir >> 8) == 002) {   /* LSI-11 manual ref 4-36 */

            offset = ir & 0377;          /* 8-bit signed offset */

            if (print_mode > 0) {
                printf("bne instruction with offset 0%03o\n", offset);


            }

            offset = offset << 24;       /* sign extend to 32 bits */
            offset = offset >> 24;

            if (!z_psw) {
                reg[7] = (reg[7] + (offset << 1)) & 0177777;
                branchTakenCount++;
            }
            if (print_mode == 2) {
                printRegisters();
            }
            branchCount++;
            instructionCount++;

            //printRegisters();

        }
        else if ((ir >> 8) == 003) {  /* LSI-11 manual ref 4-37 */
     //BEQ

            offset = ir & 0377;          /* 8-bit signed offset */

            if (print_mode > 0) {
                printf("beq instruction with offset 0%03o\n", offset);
            }


            offset = offset << 24;       /* sign extend to 32 bits */
            offset = offset >> 24;

            if (z_psw == 1) {
                reg[7] = (reg[7] + (offset << 1)) & 0177777;
                branchTakenCount++;
            }
            branchCount++;
            instructionCount++;
            //printRegisters();

        }
        else if ((ir >> 6) == 0062) {  /* LSI-11 manual ref 4-13 */
     //ASR


            get_operand(&dst);

            c_psw = dst.value & 1;
            result = dst.value << 16;
            result = result >> 16;
            result = result >> 1;
            result = result & 0177777;
            sign_bit = result & 0100000;
            n_psw = 0; if (sign_bit) n_psw = 1;
            z_psw = 0; if (result == 0) z_psw = 1;
            v_psw = 0;
            if (n_psw ^ c_psw) {
                v_psw = 1;
            }

            if (print_mode > 0) {
                printf("asr instruction dm %d dr %d\n", dst.mode, dst.reg);
            }

            //found  4 dont have src value
            if (print_mode == 2) {
                printf("  dst.value = 0%06o\n", dst.value);
                printf("  result    = 0%06o\n", result);
                printf("  nzvc bits = 4'b%1d%1d%1d%1d\n", n_psw, z_psw, v_psw, c_psw);

                //printRegisters();
            }


            update_operand(&dst, result);
            // printRegisters();
            instructionCount++;

        }
        else if ((ir >> 6) == 0063) {  /* LSI-11 manual ref 4-14 */
         //ASL

            get_operand(&dst);

            result = dst.value << 1;

            c_psw = (result & 0200000) >> 16;
            result = result & 0177777;

            sign_bit = result & 0100000;
            n_psw = 0; if (sign_bit) n_psw = 1;
            z_psw = 0; if (result == 0) z_psw = 1;
            v_psw = 0;
            if (n_psw ^ c_psw) {
                v_psw = 1;
            }
            if (print_mode > 0) {
                printf("asl instruction dm %d dr %d\n", dst.mode, dst.reg);
            }

            //found  5 dont have src value
            if (print_mode == 2) {
                printf("  dst.value = 0%06o\n", dst.value);
                printf("  result    = 0%06o\n", result);
                printf("  nzvc bits = 4'b%1d%1d%1d%1d\n", n_psw, z_psw, v_psw, c_psw);
                // printRegisters();
            }

            update_operand(&dst, result);


            instructionCount++;
        }
        else {
            dataReadCount++;
            //instructionFetchCount++;
        }

    }

    //Summary execution statistics of program
    printf("\nexecution statistics (in decimal):\n");
    printf("  instructions executed     = %d\n", instructionCount);
    printf("  instruction words fetched = %d\n", instructionFetchCount - dataReadCount);
    printf("  data words read           = %d\n", dataReadCount);
    printf("  data words written        = %d\n", dataWriteCount);
    printf("  branches executed         = %d\n", branchCount);
    printf("  branches taken            = %d", branchTakenCount);
    if (branchCount > 0) {
        printf(" (%.1f%%)\n", 100.0 * ((float)branchTakenCount) / ((float)branchCount));
    }
    else {
        printf("\n");
    }
    if (print_mode > 1) {
        printf("\nfirst 20 words of memory after execution halts:\n");

        for (int i = 0; i < 20; i++) {
            printf("  0%04o: %06o\n", i << 1, mem[i]);
            byte_count += 2;
        }
    }
    cache_stats();

    return 0;

}
