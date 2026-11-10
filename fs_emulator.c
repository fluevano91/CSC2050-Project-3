#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

struct inode {
    uint32_t nodenum;
    uint32_t parent;
    char type;
    char name[32];
    char padding[3];   /* keeps each inode record 44 bytes to match the provided file. */
};



/* saves the inodes marked as used to inodes_list.
 * return 0 if saving succeeds or 1 if it fails.
 */


int saveNodes(struct inode nodearray[], int used[])
{
    FILE *nodelist = fopen("inodes_list", "wb");

    if (nodelist == NULL) {
        perror("fopen");
        return 1;
    }

    for (int k = 0; k < 1024; k++) {
        if (used[k] == 1) {
            if (fwrite(&nodearray[k], sizeof(nodearray[k]), 1, nodelist) != 1) {
                perror("fwrite");
                fclose(nodelist);
                return 1;
            }
        }
    }

    if (fclose(nodelist) != 0) {
        perror("fclose");
        return 1;
    }

    return 0;
}


/* read the inodes from the file and skip any that are invalid.
 * use each inode number as its spot in the array.
 * make sure inode 0 exists and is a directory before running commands.
 */


int load_inodes(struct inode nodearray[], int used[])
{
    FILE *nodelist = fopen("inodes_list", "rb");

    if (nodelist == NULL) {
        perror("fopen");
        return 1;
    }

    struct inode entry;

    while (fread(&entry, sizeof(entry), 1, nodelist) == 1) {
        if (entry.nodenum > 1023) {
            printf("inode out of range\n");
        }
        else {
            if (entry.type == 'd' || entry.type == 'f') {
                nodearray[entry.nodenum] = entry;
                used[entry.nodenum] = 1;
            }
            else {
                printf("invalid node type\n");
            }
        }
    }

    if (ferror(nodelist)) {
        perror("fread");
        fclose(nodelist);
        return 1;
    }

    fclose(nodelist);

    if (used[0] != 1 || nodearray[0].type != 'd') {
        printf("root inode missing or is not a directory\n");
        return 1;
    }

    return 0;
}


/* gets the command and its argument from what the user typed.
 * return 1 if there are too many arguments so main can skip the command.
 */


int parse_command(char command[], char action[], char argument[])
{
    int i = 0;
    int j = 0;

    while (command[i] != ' ' &&
           command[i] != '\0' &&
           command[i] != '\n') {
        action[i] = command[i];
        i++;
    }

    action[i] = '\0';

    while (command[i] == ' ') {
        i++;
    }

    while (command[i] != ' ' &&
           command[i] != '\0' &&
           command[i] != '\n') {
        argument[j] = command[i];
        j++;
        i++;
    }

    argument[j] = '\0';

    while (command[i] == ' ') {
    i++;
    }

    if(command[i] != '\0' && command[i] != '\n') {
        printf("too many arguments\n");
        return 1;
    }

return 0;
}




/* print the entries in the current directory. */

void ls(struct inode nodearray[], int used[], uint32_t current_dir)
{
    for (int k = 0; k < 1024; k++) {
        if (used[k] == 1 && nodearray[k].parent == current_dir) {
            printf("inode: %u, type: %c, name: %.32s\n",
                   nodearray[k].nodenum,
                   nodearray[k].type,
                   nodearray[k].name);
        }
    }
}




/* return the inode number of the  directory the user wants to go to.
 * for "..", return the parent. If the move fails, return the current inode number.
 */


uint32_t cd(struct inode nodearray[], int used[],
            uint32_t current_dir, char argument[])
{
    if (argument[0] == '\0') {
    printf("no directory entered\n");
    return current_dir;
}    

    if(strcmp(argument, "..") == 0){
    return nodearray[current_dir].parent;

}    

    int found = 0;

    for (int h = 0; h < 1024 && found != 1; h++) {
        if (used[h] == 1 &&
            nodearray[h].parent == current_dir &&
            strncmp(nodearray[h].name, argument, 32) == 0 &&
            nodearray[h].type == 'd') {
            current_dir = nodearray[h].nodenum;
            found = 1;
        }
    }

    if (found != 1) {
        printf("directory not found\n");
    }

    return current_dir;
}



/* make a directory if the name is free in the current directory.
 * write "." and ".." to the file named by its inode number.
 */


void mkdir(struct inode nodearray[], int used[],
           uint32_t current_dir, char argument[])
{
    if (argument[0] == '\0') {
        printf("mkdir needs a directory name\n");
        return;
    }

    char name[33];
    strncpy(name, argument, 32);
    name[32] = '\0';

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        printf("entry with that name already exists\n");
        return;
    }

    for (int k = 0; k < 1024; k++) {
        if (used[k] == 1 &&
            nodearray[k].parent == current_dir &&
            strncmp(nodearray[k].name, name, 32) == 0) {
            printf("entry with that name already exists\n");
            return;
        }
    }

    int free_inode = -1;

    for (int k = 1; k < 1024; k++) {
        if (used[k] == 0) {
            free_inode = k;
            break;
        }
    }

    if (free_inode == -1) {
        printf("no free inode slots\n");
        return;
    }

    char filename[16];
    snprintf(filename, sizeof(filename), "%d", free_inode);

    FILE *directory = fopen(filename, "w");
    if (directory == NULL) {
        perror("mkdir");
        return;
    }

    if (fprintf(directory, "%d .\n%u ..\n", free_inode, current_dir) < 0) {
        perror("mkdir write");
        fclose(directory);
        remove(filename);
        return;
    }

    if (fclose(directory) != 0) {
        perror("mkdir close");
        remove(filename);
        return;
    }

    nodearray[free_inode].nodenum = free_inode;
    nodearray[free_inode].parent = current_dir;
    nodearray[free_inode].type = 'd';
    strncpy(nodearray[free_inode].name, name, 32);
    for (int p = 0; p < 3; p++) {
        nodearray[free_inode].padding[p] = 0;
    }


    
    used[free_inode] = 1;
}

/* make a file if the name is free in the current directory.
 * write its name into the file named by its inode number.
 */

void touch(struct inode nodearray[], int used[],
           uint32_t current_dir, char argument[])
{
    if (argument[0] == '\0') {
        printf("touch requires a file name\n");
        return;
    }

    char name[33];
    strncpy(name, argument, 32);
    name[32] = '\0';

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return;
    }

    for (int k = 0; k < 1024; k++) {
        if (used[k] == 1 &&
            nodearray[k].parent == current_dir &&
            strncmp(nodearray[k].name, name, 32) == 0) {
            return;
        }
    }

    int free_inode = -1;

    for (int k = 1; k < 1024; k++) {
        if (used[k] == 0) {
            free_inode = k;
            break;
        }
    }

    if (free_inode == -1) {
        printf("no free inode slots\n");
        return;
    }

    char filename[16];
    snprintf(filename, sizeof(filename), "%d", free_inode);

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("touch");
        return;
    }

    if (fprintf(file, "%s", name) < 0) {
        perror("touch write");
        fclose(file);
        remove(filename);
        return;
    }

    if (fclose(file) != 0) {
        perror("touch close");
        remove(filename);
        return;
    }

    nodearray[free_inode].nodenum = free_inode;
    nodearray[free_inode].parent = current_dir;
    nodearray[free_inode].type = 'f';
    strncpy(nodearray[free_inode].name, name, 32);
    for (int p = 0; p < 3; p++) {
        nodearray[free_inode].padding[p] = 0;
    }



    used[free_inode] = 1;
}



int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("error\n");
        return 1;
    }

    if (chdir(argv[1]) == -1) {
        perror("chdir");
        return 1;
    }

    struct inode nodearray[1024];
    int used[1024] = {0};

    if (load_inodes(nodearray, used) != 0) {
        return 1;
    }

    uint32_t current_dir = 0;
    char command[256];

    while (fgets(command, sizeof(command), stdin) != NULL) {
        char action[256];
        char argument[256];

        if (parse_command(command, action, argument) == 0) {

            if (strcmp(action, "ls") == 0) {
                if(argument[0] == '\0') {
                     ls(nodearray, used, current_dir);
                }
                else {
                    printf("no argument should be provided when using ls\n");
                }
            }

            
            else if (strcmp(action, "cd") == 0) {
                current_dir = cd(nodearray, used, current_dir, argument);
            }

            else if (strcmp(action, "exit") == 0) {
                 if (argument[0] == '\0') {
                     break;
                  }
            else {
                printf("exit takes no arguments\n");
            }

            }

            else if (strcmp(action, "mkdir") == 0) {
                 mkdir(nodearray, used, current_dir, argument);
            }

            else if (strcmp(action, "touch") == 0) {
               touch(nodearray, used, current_dir, argument);
            }
            

            else{
                printf("unkown command\n");
            }

        }
    }

    return saveNodes(nodearray, used);
}
