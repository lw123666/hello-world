global my_print


section .data
red: db 1Bh, '[31;1m', 0    ;红色  警告信息颜色，对应flag为2
.len: equ $-red
blue: db 1Bh, '[34;1m', 0     ;蓝色   目录颜色，对应flag为1
.len: equ $-blue
white: db 1Bh, '[37;1m', 0   
.len: equ $-white

restore: db 1Bh, '[37;0m', 0     ;默认色
.len: equ $-restore

section .bss
out: resb 1     ;一个字节一个字节输出
flag: resq 1    ;颜色flag

section .text
my_print:
    mov rax, rdi
    mov byte[out], al
    mov rbx, rsi
    mov qword[flag], rbx
    cmp qword[flag], 0
    je _white
    cmp qword[flag], 1
    je _blue
    cmp qword[flag], 2
    je _red

_printChar:
    mov eax, 4
    mov ebx, 1
    mov ecx, out
    mov edx, 1
    int 80h

    mov eax, 4
    mov ebx, 1
    mov ecx, restore
    mov edx, restore.len
    int 80h
    ret

_white:
   mov eax, 4
   mov ebx, 1
   mov ecx, white
   mov edx, white.len
   int 80h
   jmp _printChar


_blue:
   mov eax, 4
   mov ebx, 1
   mov ecx, blue
   mov edx, blue.len
   int 80h
   jmp _printChar


_red:
   mov eax, 4
   mov ebx, 1
   mov ecx, red
   mov edx, red.len
   int 80h
   jmp _printChar
