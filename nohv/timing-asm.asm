.code

check_rdtscp_regs proc
  mov rax, 0FFFFFFFFFFFFFFFFh
  mov rdx, 0FFFFFFFFFFFFFFFFh
  mov rcx, 0FFFFFFFFFFFFFFFFh
  
  rdtscp
  
  shr rax, 32
  test eax, eax
  jne detected
  
  shr rdx, 32
  test edx, edx
  jne detected
  
  shr rcx, 32
  test ecx, ecx
  jne detected
  
  ; al is already 0
  ret

detected:
  mov al, 1
  ret

check_rdtscp_regs endp

end