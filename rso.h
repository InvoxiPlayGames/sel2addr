#ifndef _RSO_H
#define _RSO_H

typedef struct _RSOOffsetSize {
    unsigned int offset;
    unsigned int size;
} RSOOffsetSize;

typedef struct _RSOHeader {
    unsigned int next;
    unsigned int prev;
    unsigned int section_count;
    unsigned int section_info_offset;
    RSOOffsetSize module_name;
    unsigned int module_version;
    unsigned int bss_size;
    unsigned char prolog_sect;
    unsigned char epilog_sect;
    unsigned char unresolved_sect;
    unsigned char bss_sect;
    unsigned int prolog_offset;
    unsigned int epilog_offset;
    unsigned int unresolved_function_offset;
    RSOOffsetSize int_reloc_table;
    RSOOffsetSize ext_reloc_table;
    RSOOffsetSize export_table;
    unsigned int export_names_offset;
    RSOOffsetSize import_table;
    unsigned int import_names_offset;
} __attribute__((packed)) RSOHeader;

typedef struct _RSOSymbolEntry {
    unsigned int name_offset;
    unsigned int offset;
    unsigned int section;
    unsigned int elf_hash;
} RSOSymbolEntry;

// TODO: RSORelocationEntry
// length: 0xC
// 0x0: Offset
// 0x4: Symbol ID
// 0x7: Type
// 0x8: Offset

#endif // _RSO_H