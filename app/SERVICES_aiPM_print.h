#include <stdint.h>
#include <aipm.h>

void print_run_cfg(run_profile_t *runp);
void print_off_cfg(off_profile_t *offp);

void adjust_run_cfg(run_profile_t *runp, int32_t opt1, int32_t opt2);
void adjust_off_cfg(off_profile_t *offp, int32_t opt1, int32_t opt2);

int32_t get_int_input();
int32_t print_user_menu();
int32_t print_user_submenu(int32_t choice);

int32_t print_cfg_main_menu();
int32_t print_cfg_sub_menu(int32_t choice);

int32_t print_dialog_boot_testing();
uint32_t print_dialog_boot_addr();
void print_dialog_configure_amux();
void print_dialog_read_reg();
void print_dialog_write_reg();
