#ifndef GROUP_HELPER_H
#define GROUP_HELPER_H

/**
 * @function get_selected_admin_group
 * Lists admin groups, prompts the user to select one, and returns the group ID.
 * 
 * @param sockfd The socket descriptor for communicating with the server.
 * @return int The selected group ID, or -1 on failure/cancellation.
 */
int get_selected_admin_group(int sockfd);

int get_selected_group_id(int sockfd);
int get_selected_folder_id(int sockfd, int group_id, const char* prompt);

#endif // GROUP_HELPER_H
