ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
    /* Стандартный адрес загрузки для Multiboot */
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        KEEP(*(.multiboot))
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }
    
    /*выравнивание по 4кб */
    . = ALIGN(4096);
    _end = .; 
    end = .;

}
