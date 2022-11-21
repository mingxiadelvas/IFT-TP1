#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/wait.h>

#define true 1
#define false 0
#define bool int

typedef int error_code;

#define ERROR (-1)
#define HAS_ERROR(code) ((code) < 0)
#define NULL_TERMINATOR '\0'

int stop = 0;
int cd(char *);
int cd(char *path) {
    return chdir(path);
}

enum op {   //todo these are your custom shell operators. You might want to use them to represent &&, ||, & and "no operator"
    BIDON, NONE, OR, AND, ALSO    //BIDON is just to make NONE=1, BIDON is unused
};

struct command {    //todo you might want to use this to represent the different commands you find on a line
    char **call;
    enum op operator;
    struct command *next;
    int count;
    bool also;
};
//hint hint: this looks suspiciously similar to a linked list we saw in the demo. I wonder if I could use the same ideas here??

#define command struct command

//fonction qui retourne le nombre d'élément dans une liste
int element_list(command* head){
    if(head == NULL){
        return 0;
    }
    int i = 1;
    while(head->next != NULL){
        head = head->next;
        i++;
    }
    return i;
}

command* link_command(command* current, command* next){
    current->next = next;
    return next;
}

//fonction qui retourne un tableau de 2 elements
//le premier correspond au n repetion demandé quand on fait l'appel a la commande rn
//le deuxieme est case memoire du debut de la parenthese 
long* sum_char(char* str){
    int debut_parenthese = 0;
    int i = 1;
    long* n = malloc(sizeof (long) *2);
    char* paranthese_mem = malloc(sizeof (char) * strlen(str));

    while(i < strlen(str)){
        if(str[i] == '('){
            break;
        }
        paranthese_mem[debut_parenthese++] = str[i];
        i++;
    }
    //permet de savoir ou commencer a copier lorsqu'on execute la commande rn(echo qqc)
    if(str[i] != '('){
        n[0] = -1;
    }else{
        n[0] = (int) strtol(paranthese_mem, (char **)NULL, 10); 
    } 
    n[1] = i;
    paranthese_mem[debut_parenthese] = NULL_TERMINATOR;
    free(paranthese_mem);

    return n;
}

//fonction simple enleve l'espace vide que retourne la fcontion strtok()
char* delete(char *str){
    char *str2;
    //check mon premier charactere un est espace, si oui str2 aura l'adresse de l'indice 1 du charactre, sinon on laisse telquel
    if(str[0] == ' '){
        str2 = &str[1];
    }else{
      str2 = &str[0];  
    } 
    return str2;
}

//fonction utiliser dans le cas qu'on utilise rn, on retourne une commande principale
char* search_command(char* cmd){
    int size = strlen(cmd);
    int i = 0;
    while(i < size){
        if(cmd[i] == ')') {
            cmd[i] = NULL_TERMINATOR;
            break;
        }
        i++;
    }
    return cmd;
}

//fonction qui permet de creer de nouvelles commandes
command* new_command(char *call, char *operator, int val){
    int i = 0;
    char space [] = " ";
    if(call == NULL){
        return NULL;
    }
    command* cmd = malloc(sizeof(command));
    cmd->call = malloc(sizeof(char*)*1024);
    cmd->also = false;
    char* str = strtok(call, space);
    //tant que le string n'est pas null, on continue de briser le string en fonction des espaces
    while( str != NULL ) {
        cmd->call[i++] = delete(str);
        str = strtok(NULL, space);
    }
    cmd->call[i] = NULL_TERMINATOR;

    //associe la commande en fonction du signe de l'opérateur
    if(strcmp(operator, "&&") == 0){
        cmd->operator = AND;
    }else if(strcmp(operator, "&") == 0){
        cmd->operator = ALSO; 
        cmd->also = true;
    }else if(strcmp(operator, "||") == 0){
        cmd->operator = OR;
    }else if(strcmp(operator, "BIDON") == 0){
        cmd->operator = BIDON;
    }else{
        cmd->operator = NONE; 
    }

    cmd->count = val;
    cmd->next = NULL;

    return cmd;
}

//check si un operateur a ete ecrit dans la commande, si oui on retourne sa valeur
command* operator_list(enum op operator, command* head){
    command* answer = head;
    int size = element_list(head);
    int i = 0;
    while(i < size){
        if(answer->operator == operator){
            return answer;
        }
        if(i == size-1) {
            return answer;
        }
        answer = answer->next;
        i++;
    }
    return answer;
}

void freeStringArray(char **arr) {  //todo probably add this to free the "call" parameter inside of command
    if (arr != NULL) {
        for (int i = 0; arr[i] != NULL; i++) {
            free(arr[i]);
        }
    }
    free(arr);
}

//libere la memoire allouee de chaque commande
void free_command(command* cmd){
    while (cmd != NULL){
        command* temp = cmd->next;
        free(cmd->call);
        free(cmd);
        cmd = temp;
    }
}

error_code readline(char **out) {   //todo this is pretty barebones, you must complete it
    size_t size = 1024;                       // size of the char array
    char *line = malloc(sizeof(char) * size);       // initialize a ten-char line
    if (line == NULL) return ERROR;   // if we can't, terminate because of a memory issue
    char ch;
    for (int at = 0; at<size; at++) { //todo 10 is clearly too small, make this bigger
        scanf("%c", &ch); // todo this is bad form, fix this
        if (ch == '\n') {        // if we get a newline
            line[at] = NULL_TERMINATOR;    // finish the line with return 0
            break;
        }
        line[at] = ch; // sets ch at the current index and increments the index
    }

    out[0] = line;
    return 0;
}

//fonction permettant de creer une liste de commande a partir de la ligne
command* parse_line(char *line){
    long val = 1;
    long parent;
    int indice = 0;
    int indice_op = 0;
    int indice_temp = 0;
    char space [] = " ";
    char special_char[] = "&&||";

    //condition principale, dans le cas ou la ligne est vide
    if (strcmp(line,"")==0){
        return NULL;
    }
    bool not_also = false;
    if(strcmp(&line[strlen(line) - 1], "&") == 0){
        not_also = true;
    }

    command* cmd;
    command* now;
    char* op_mem = malloc(sizeof (char)*50);
    memcpy(op_mem, line, strlen(line));
    char* cmd2[256];
    char* temp2[256];
    char* op2[256];
    char* str = strtok(line, special_char);

    //decoupe chaque chaque mot de la phare en fonction du string donne 
    while( str != NULL ) {
        cmd2[indice++] = delete(str);
        str = strtok(NULL, special_char);
    }
    cmd2[indice] = NULL_TERMINATOR;

    long rn [sizeof(cmd2)];
    char* execute_line;
    //on check si on a des rn dans une des commandes ecrite
    for(int i=0; i<indice; i++){
        if(cmd2[i][0] == 'r' && strlen(cmd2[i]) > 1){
            long* parent2 = sum_char(cmd2[i]);
            if(parent2){
                val = parent2[0];
                parent = parent2[1];
                free(parent2);
            }
            if(val != -1){ //check si on est dans rn ou non
                execute_line = search_command(&cmd2[i][parent+1]);
                if(execute_line){
                    cmd2[i] = execute_line;
                }
            }else{
                val = 1;
            }
        }else{
            val = 1;
        }
        rn[i] = val;
    }
    if(indice == 1 && not_also == false){
        cmd = new_command(cmd2[0], "NONE", rn[0]);
        free(op_mem);
        return cmd;
    }else if(indice == 1 && not_also == true){ 
        cmd = new_command(cmd2[0], "&", rn[0]);
        free(op_mem);
        return cmd;
    }else{
        char* str_op = strtok(op_mem, space);
        while (str_op != NULL) {
            temp2[indice_temp++] = str_op;
            str_op = strtok(NULL, space);
        }
        temp2[indice_temp] = NULL_TERMINATOR;

        for (int i = 0; i < indice_temp; i++){
            if(strcmp(temp2[i], "||") == 0 || strcmp(temp2[i], "&&") == 0 || strcmp(temp2[i], "&")  == 0){
                op2[indice_op++] = temp2[i];
            }
        }
        op2[indice_op] = NULL_TERMINATOR;
        //on fait appel au premier element dans la liste
        cmd = new_command(cmd2[0], op2[0], rn[0]);
        now = cmd;
        command* next;

        for(int i = 1; i < indice; i++){
            if(not_also == true){
                next = new_command(cmd2[i], op2[i], rn[i]);
            }else{
                if(i == indice-1){
                    next = new_command(cmd2[i], "NONE", rn[i]);
                }else{
                    next = new_command(cmd2[i], op2[i], rn[i]);
                } 
            }
            link_command(cmd, next);
            cmd = next;
        }
    }
    free(op_mem);

    return now;
}

//fonction qui execute la commande envoye
void execute_command(command* cmd) {
    bool not_and = false;
    bool not_or = false;
    bool not_or2 = true;
    
    while (cmd != NULL) {
        not_or = false;
        not_and = false;
        //appel a pour la commande cd
        if (strcmp(*cmd->call, "cd") == 0) {
            if (cd(cmd->call[1]) < 0) {
                perror(cmd->call[1]);
            }
            cmd = cmd->next;
            continue;
        }
        enum op operators = cmd->operator;
        if (not_or2 == false){
            if (operators == OR){
                cmd = cmd->next;
                continue;
            }else{
                not_or2 = true;
                cmd = cmd->next;
                continue;
            }
        }
        for (int i = 1; i <=(cmd->count); i++) {
            pid_t pid;
            pid = fork();

            if(pid < 0){
                fprintf(stderr, "Le premier fork a échoué");
            }else if(pid == 0){ 
                int num = execvp(*cmd->call, &cmd->call[0]);
                if(num == -1){
                    printf("%s: command not found\n", *cmd->call);
                    exit(60);
                }
            }else{ //processus du parent
            //on attend que l'enfant est fini de faire sa commande
                int stat;
                waitpid(pid, &stat, 0);
                if (WEXITSTATUS(stat) == 60) {
                    if (operators == AND) {
                        not_and = true;
                    }
                }
                else {
                    if (operators == OR) {
                        not_or2 = false;
                        not_or = true;
                    }
                }
            }
        }
        if(not_and == true) break;
        if(not_or == true) continue;

        cmd = cmd->next;
    }
    free_command(cmd);
}

//fonction qui permet d'execute le shell
void execute(){
    pid_t pid2;
    while(stop == 0){
        char *line;
        readline(&line);
        //creation de la liste chainnee
        command *cmd = parse_line(line);
        if(cmd == NULL){
            free_command(cmd);
            free(line);
            continue;
        }
        //exit qui a ete donne dans le main
        if(strcmp(line, "exit") == 0){
            free(line);
            exit(0);
        }
        command* tempo = operator_list(ALSO, cmd);
        //dans le cas ou on a un background
        if(tempo->also == 1){ 
            pid2 = fork();
            if(pid2 <0){
                fprintf(stderr, "Le deuxième fork a échoué");
            }else if(pid2 == 0){ 
                execute_command(cmd);
                free_command(cmd);
                free(line);

                exit(0);
            }else{
                free_command(cmd);
                free(line);
                continue;
            }
        }else{ 
            //dans le cas ou on n'a pas de backgroung 
            execute_command(cmd);
            free_command(cmd);
            free(line);
        }
    }
}

int main (void) {    
    //todo
    /*lchar *line;
    readline(&line);    //todo what about error_code?
    if(strcmp(line, "exit") == 0) {
        free(line);
        exit(0);
    }
    exit(-1);*/

    //todo probably add other functions for different parts of the homework...

    execute();
}
