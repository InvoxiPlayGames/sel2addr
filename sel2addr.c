#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "dol.h"
#include "rso.h"

#define MEM1_SIZE 0x1800000
#define NUM_SECTIONS 14

#ifndef _BIG_ENDIAN
#define BE(i) (((i) & 0xff) << 24 | ((i) & 0xff00) << 8 | ((i) & 0xff0000) >> 8 | ((i) >> 24) & 0xff)
#else
#define BE(i) i
#endif

#define PPC_OP(i) (i >> 26)
#define PPC_D(i) ((i >> 21) & 31)
#define PPC_A(i) ((i >> 16) & 31)
#define PPC_SIMM(i) (i & 0xFFFF)

typedef enum _FileFormat {
    FORMAT_DOL,
    FORMAT_ELF,
    FORMAT_MEM,
    FORMAT_OTHER
} FileFormat;

// function signature for RSOStaticLocateObject
static char RSOStaticLocateObject_Signature[] = {
    0x3a, 0x20, 0x00, 0x01, 0x39, 0xe0, 0x00, 0x04, 0x3a, 0x40, 0x00, 0x08, 0x90, 0xc3, 0x00, 0x40
};
// the offset into the function that the signature appears at
static unsigned int RSOStaticLocateObject_Offset = 0xC0;
// the length of the fuction to actually check
static unsigned int RSOStaticLocateObject_Header = 0x100;

// Some Platforms Don't Have Memmem... so we have to use our own!
// thank you random stackoverflow citizen
void *own_memmem(const void *haystack, size_t haystack_len, const void * const needle, const size_t needle_len)
{
    if (haystack == NULL) return NULL;
    if (haystack_len == 0) return NULL;
    if (needle == NULL) return NULL;
    if (needle_len == 0) return NULL;
    for (const char *h = haystack; haystack_len >= needle_len; ++h, --haystack_len) {
        if (!memcmp(h, needle, needle_len)) return (void *)h;
    }
    return NULL;
}

// returns the input format so we can load everything efficient and properly
FileFormat GetInputFormat(void *first_100h) {
    // elf files start with... elf!
    static char ELFHeader[] = { 0x7F, 'E', 'L', 'F' };
    if (memcmp(first_100h, ELFHeader, 4) == 0)
        return FORMAT_ELF;
    // memory dumps have this code written at 0x80000020
    static char DiseaseCode[] = { 0x0D, 0x15, 0xEA, 0x5E };
    if (memcmp(first_100h + 0x20, DiseaseCode, 4) == 0)
        return FORMAT_MEM;
    // DOL files often (always for official software) begin Text0 at 0x100
    static char StandardDOLOffset[] = { 0x00, 0x00, 0x01, 0x00 };
    if (memcmp(first_100h, StandardDOLOffset, 4) == 0)
        return FORMAT_DOL;
    // no idea
    return FORMAT_OTHER;
}

bool FindRSOSections(void *datablob, unsigned int datasize, unsigned int sections[14], unsigned int baseaddr) {
    // find the RSOStaticLocateObject function in the game
    void * find_signature = own_memmem(datablob, datasize - sizeof(RSOStaticLocateObject_Signature), RSOStaticLocateObject_Signature, sizeof(RSOStaticLocateObject_Signature));
    // if we can't find it, don't bother
    if (find_signature == NULL)
        return false;
    void * RSOStaticLocateObject_addr = find_signature - RSOStaticLocateObject_Offset;
    printf("RSOStaticLocateObject: 0x%08x\n", (unsigned int)(RSOStaticLocateObject_addr - datablob + baseaddr));
    // iterate through each instruction and parse the result
    for (void * i = RSOStaticLocateObject_addr; i < RSOStaticLocateObject_addr + RSOStaticLocateObject_Header; i += 4) {
        // read the instruction
        unsigned int inst = BE(*(unsigned int *)i);
        // get the target section from the destination register
        int target = 0;
        if (PPC_D(inst) == 22) target = 1;
        if (PPC_D(inst) == 23) target = 2;
        if (PPC_D(inst) == 24) target = 5;
        if (PPC_D(inst) == 25) target = 6;
        if (PPC_D(inst) == 26) target = 7;
        if (PPC_D(inst) == 27) target = 8;
        if (PPC_D(inst) == 29) target = 9;
        if (PPC_D(inst) == 28) target = 11;
        if (PPC_D(inst) == 30) target = 12;
        // if we don't have a target section, leave
        if (target == 0) continue;
        // if its a lis, its the higher 16 bits of our value
        if (PPC_OP(inst) == 15 && PPC_A(inst) == 0) {
            sections[target] = PPC_SIMM(inst) << 16;
        // if its an addi/subi, its the lower 16 bits of our value
        } else if (PPC_OP(inst) == 14 && PPC_A(inst) == PPC_D(inst)) {
            if (PPC_SIMM(inst) <= 0x7FFF) { // andi
                sections[target] += PPC_SIMM(inst);
            } else { // subi
                sections[target] -= (0x10000 - PPC_SIMM(inst));
            }
            //printf("RSO Section %i: 0x%08x\n", target, sections[target]);
        }
    }
    return (sections[1] > 0);
}

int main(int argc, char** argv) {
    unsigned int sections[NUM_SECTIONS] = { 0 };
    char first_100h[0x100] = { 0 };

    // make sure we have all the arguments we need
    if (argc != 3) {
        printf("usage: %s /path/to/game.bin /path/to/game.sel\n", argv[0]);
        return -1;
    }

    // load the game file, and detect the format
    FILE *fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        printf("failed to open '%s'\n", argv[1]);
        return -1;
    }
    fread(first_100h, 1, sizeof(first_100h), fp);
    FileFormat fmt = GetInputFormat(first_100h);

    bool found_sections = false;
    if (fmt == FORMAT_MEM) {
        // allocate a 24MB buffer and load the entire file into it
        void *membuffer = malloc(MEM1_SIZE);
        fseek(fp, 0, SEEK_SET);
        fread(membuffer, 1, MEM1_SIZE, fp);
        // get the RSO section offsets 
        found_sections = FindRSOSections(membuffer, MEM1_SIZE, sections, 0x80000000);
        // no leaky leaky, 24MB is big!
        free(membuffer);
    } else if (fmt == FORMAT_DOL) {
        DOL *dol_header = (DOL *)first_100h;
        // iterate through each text segment and find RSO sections there
        // if we find one, don't continue searching!
        for (int i = 0; i < DOL_TEXT_SECTIONS && !found_sections; i++) {
            // if load address is null, just skip over it, its unsafe to do this
            if (BE(dol_header->textloading[i]) == 0) continue;
            // allocate a buffer for the DOL segment
            void *secbuffer = malloc(BE(dol_header->textsizes[i]));
            if (secbuffer == NULL) {
                // whooups...
                fclose(fp);
                printf("input game file too big, or out of memory\n");
                return -1;
            }
            // seek to where the DOL segment begins and read through it
            fseek(fp, BE(dol_header->textoffset[i]), SEEK_SET);
            fread(secbuffer, 1, BE(dol_header->textsizes[i]), fp);
            // get the RSO section offsets
            found_sections = FindRSOSections(secbuffer, BE(dol_header->textsizes[i]), sections, BE(dol_header->textloading[i]));
            free(secbuffer);
        }
    } else { // ELF isn't implemented, this will work right?
        // get the filesize
        fseek(fp, 0, SEEK_END);
        int filesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        // load the entire file into a buffer
        void *filebuffer = malloc(filesize);
        if (filebuffer == NULL) {
            // whooups...
            fclose(fp);
            printf("input game file too big, or out of memory\n");
            return -1;
        }
        fread(filebuffer, 1, filesize, fp);
        // get the RSO section offsets, no base sorry
        found_sections = FindRSOSections(filebuffer, filesize, sections, 0);
        free(filebuffer);
    }

    // close the first file to be nice and neat
    fclose(fp);
    // we haven't been able to find the section addresses, we're ngmi
    if (!found_sections) {
        printf("failed to find RSO section addresses, quitting\n");
        return -1;
    }

    // open the RSO file, and read its header
    fp = fopen(argv[2], "rb");
    if (fp == NULL) {
        printf("failed to open '%s'\n", argv[1]);
        return -1;
    }
    RSOHeader rso;
    fread(&rso, sizeof(RSOHeader), 1, fp);
    unsigned int table_size = BE(rso.export_table.size);
    unsigned int table_offset = BE(rso.export_table.offset);
    unsigned int names_offset = BE(rso.export_names_offset);
    // allocate the size of the export table
    RSOSymbolEntry *table = malloc(table_size);
    if (table == NULL) {
        // whooups...
        fclose(fp);
        printf("input SEL file too big, or out of memory\n");
        return -1;
    }
    int num_entries = table_size / sizeof(RSOSymbolEntry);
    // load the export table into the array
    fseek(fp, table_offset, SEEK_SET);
    fread(table, num_entries, sizeof(RSOSymbolEntry), fp);

    // iterate through each entry
    for (int i = 0; i < num_entries; i++) {
        char entry_name[0x80];
        RSOSymbolEntry entry = table[i];
        // if the section is too big, ignore it
        if (BE(entry.section) > NUM_SECTIONS) continue;
        unsigned int entry_sect = BE(entry.section);
        unsigned int entry_offset = BE(entry.offset);
        // get the address of the entry from the table
        unsigned int entry_addr = sections[entry_sect] + entry_offset;
        // if the address is zero somehow, ignore it
        if (entry_addr == 0) continue;
        // read the entry's name from the file
        unsigned int name_offset = BE(entry.name_offset);
        fseek(fp, names_offset + name_offset, SEEK_SET);
        fread(entry_name, 1, sizeof(entry_name), fp);
        // print out the info
        printf("0x%08x = %s\n", entry_addr, entry_name);
    }

    // clean up after ourselves
    free(table);
    fclose(fp);

    return 0;
}