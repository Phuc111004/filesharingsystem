#include <stdio.h>


// small CLI helper functions
void print_menu() {
printf("Available commands:\n");
printf(" register <username> <password>\n");
printf(" login <username> <password>\n");
printf(" create_group <name>\n");
printf(" join_group <group_id>\n");
printf(" upload <filepath>\n");
printf(" download <file_id>\n");
}
