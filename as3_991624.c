#include <stdio.h> 
#include <sys/types.h>
#include <dirent.h> 
#include <unistd.h> 
#include <sys/stat.h> 
#include <stdlib.h> 
#include <string.h> 
#include <stdbool.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <libgen.h>
#include <ctype.h>
#include <fnmatch.h>

char cwd[PATH_MAX];
const char * getCurrentDir(){
   getcwd(cwd, sizeof(cwd));
   return cwd;
}

char cur_dir [100];

int arrayIndexes = 1;
char ** globalItemArray;
int rows = 0;
int reverse_order = 0;

//for ignore_readdir_race option
char ** ignore_readdir_table;
bool flag_ignore_readdir = false;
int counter = 0;

bool pathIsInIgnoreTable(char value []){
	int j;
	//printf("inside function pathIsInIgnoreTable:\n");
	//printf("testing string: %s\n",value);
	for (j=0;j<counter;j++){
		//printf("ignore readdir_table[%d]: %s\n",j,ignore_readdir_table[j]);
		//printf("comparing . . .:\n");
		if (strcmp(value,ignore_readdir_table[j]) == 0){
			return true;
		}
	}
	return false;
}

char absolute_path [100];
int current_depth = 0;
void printdir(char *dir, int indent) {
	DIR *dp;
	struct dirent *entry;
	struct stat statbuf;
	
	if((dp = opendir(dir)) == NULL) {
		perror(dir);    
		return;
	}
	
	chdir(dir);// change directory
	current_depth+=1;
	strcat(absolute_path,dir);
	strcat(absolute_path,"/");
	
	while((entry = readdir(dp)) != NULL) {
		
		if (lstat(entry->d_name,&statbuf) < 0){
			if (((flag_ignore_readdir==true)&&(counter==0))||((flag_ignore_readdir==true)&&(counter>0)&&(pathIsInIgnoreTable(entry->d_name))))
				;
			else
				perror("lstat error");
		}

		if (S_ISDIR(statbuf.st_mode)) {
			// Found a directory, but ignore . and .. 
			if(strcmp(".",entry->d_name) == 0 || strcmp("..",entry->d_name) == 0)
				continue;
				
			//printf("%*s%s%s/\t%d\n",indent,"",absolute_path,entry->d_name,current_depth);
			sprintf(globalItemArray[arrayIndexes-1],"%s%s/\t%d",absolute_path,entry->d_name,current_depth);
			arrayIndexes+=1;
			globalItemArray = realloc(globalItemArray, arrayIndexes*sizeof(*globalItemArray));
			globalItemArray[arrayIndexes-1] = malloc(100 * sizeof(char*));
			// Recurse using a new indent offset 
			printdir(entry->d_name,indent+4);
		}else 
			//printf("%*s%s%s\t%d\n",indent,"",absolute_path,entry->d_name,current_depth);
			sprintf(globalItemArray[arrayIndexes-1],"%s%s\t%d",absolute_path,entry->d_name,current_depth);
			arrayIndexes+=1;
			globalItemArray = realloc(globalItemArray, arrayIndexes*sizeof(*globalItemArray));
			globalItemArray[arrayIndexes-1] = malloc(100 * sizeof(char*));
	}
	
	chdir("..");
	current_depth-=1;

	char character = '/';
	char tmp [100];
	int j=0;
	while (j < strlen(absolute_path)-1){
		tmp[j] = absolute_path[j];
		j++;
	}
	tmp[j] = '\0';

    char* ptr = strrchr(tmp, character);
    int end = ptr-tmp;
	char new_path[100];
    memcpy( new_path, &tmp[0], (end+1) );
    new_path[(end+1)] = '\0';
	strcpy(absolute_path,new_path);

	closedir(dp);
}

bool isASCII(char * word){
    int i;
    bool ascii_flag = true;
    for (i=0;i<strlen(word);i++)
        if (!isascii(word[i])){
             ascii_flag = false;
             break;
        }
    return ascii_flag;
}

void removeSubstr (char *string, char *sub) {
    char *match;
    int len = strlen(sub);
    while ((match = strstr(string, sub))) {
        *match = '\0';
        strcat(string, match+len);
    }
}

int keepFormation(char * testing_str){
	bool flag=false;
    int i;
    for (i=0;i<strlen(testing_str);i++){
        if (testing_str[i] == '\t'){
            if (isdigit(testing_str[i+1])){
                flag=true;
            }
        }
    }
    if (flag==false){
        return 0;
    }else{
        return 1;
    }	
}

void clearGlobalTable(){
	rows = 0;
	int i = 0;
	int j = 0;
	while (i<arrayIndexes){
		if (i!=arrayIndexes-1){		
			if ((globalItemArray[i][0] == '\0')||(keepFormation(globalItemArray[i]) == 0)){
				for (j=i;j<arrayIndexes-1;j++)
					globalItemArray[j] = globalItemArray[j+1];	
				arrayIndexes-=1;
				i-=1;
			}else{
				rows+=1;
			}
		}else{
			if (globalItemArray[i][0] != '\0'){
				rows+=1;
			}else{
				arrayIndexes-=1;
			}
		}
		i+=1;
	}
}

char * printRelativePath(char path []){
    static char test [100];
	char character = '/';
	int j;
	
	if (path[strlen(path)-1] == '/'){   //check if folder
	    j=0;
		while (j < strlen(path)-1){
			test[j] = path[j];
			j++;
		}
   		test[j] = '\0';
	}else{
	    strcpy(test,path);
	}
	
	//printf("test:%s\n",test);
	char* ptr = strrchr(test, character);
	int end = ptr-test;
	char dirent [100] ;
    j=0;
	while (j < (end+1)){
		dirent[j] = test[j];
		j++;
	} 
	dirent[j] = '\0';
	
	//printf("dirent:%s\n",dirent);
	removeSubstr(test, dirent);
	return test;
}

// elexe symbolic links -P,-H,-L
void optionExecuteWithValue(char symlink_option [],char option [],char value []){
	struct stat buf;
	struct passwd * p;
	struct group * g;
	int modif_time;
	int i;

	if ((strcmp(option,"-group")==0)&&(isdigit(value[0]) == 0)){ //if isDigit is zero means argument is NOT numeric
		if ((g = getgrnam(value)) == NULL){
			perror("getgrnam() error");
		}else{
			sprintf(value,"%d",g->gr_gid);
			//printf("group id : %d\n", g->gr_gid);
		}
	}

	if ((strcmp(option,"-user")==0)&&(isdigit(value[0]) == 0)){	//if isDigit is zero means argument is NOT numeric
		if ((p = getpwnam(value)) == NULL){
			perror("getpwnam() error");
		}else{
			sprintf(value,"%d",(int)(p->pw_uid));
			//printf("value = %s\n",value);
		}
	}

	struct stat newer_buf;
	if (strcmp(option,"-newer")==0){
		if (strcmp(symlink_option,"-P")==0){
			if (lstat(value,&newer_buf) < 0){
				bool abc = pathIsInIgnoreTable(value);
				if (((flag_ignore_readdir==true)&&(counter==0))||((flag_ignore_readdir==true)&&(counter>0)&&(pathIsInIgnoreTable(value))))
					;
				else
					perror("lstat error");
			}
		}

		if ((strcmp(symlink_option,"-L")==0)||(strcmp(symlink_option,"-H")==0)){
			if (stat(value,&newer_buf) < 0){
				if (((flag_ignore_readdir==true)&&(counter==0))||((flag_ignore_readdir==true)&&(counter>0)&&(pathIsInIgnoreTable(value))))
					;
				else
					perror("stat error");
			}
		}

		//get modified date
		modif_time = newer_buf.st_mtime;
	}

	for (i=0;i<rows;i++){
		char * path = strtok(globalItemArray[i], "\t");
		char * depth_str = strtok(NULL,"\t");
		int path_depth = atoi(depth_str);
		bool flag_delete = true;

		if (strcmp(symlink_option,"-P")==0){
			if (lstat(path,&buf) < 0){
				if (((flag_ignore_readdir==true)&&(counter==0))||((flag_ignore_readdir==true)&&(counter>0)&&(pathIsInIgnoreTable(globalItemArray[i]))))
					;
				else
					perror("lstat error");
				continue;
			}
		}else if(strcmp(symlink_option,"-L")==0){
			if (stat(path,&buf) < 0){
				if (((flag_ignore_readdir==true)&&(counter==0))||((flag_ignore_readdir==true)&&(counter>0)&&(pathIsInIgnoreTable(globalItemArray[i]))))
					;
				else
					perror("stat error");				
				continue;
			}
		}else{	//option H
			if (path_depth == 0){	//command-line argument
				if (stat(path,&buf) < 0){
					if (((flag_ignore_readdir==true)&&(counter==0))||((flag_ignore_readdir==true)&&(counter>0)&&(pathIsInIgnoreTable(globalItemArray[i]))))
						;
					else
						perror("stat error");
					continue;
				}
			}else{					//NOT a command-line argument
				if (lstat(path,&buf) < 0){
					if (((flag_ignore_readdir==true)&&(counter==0))||((flag_ignore_readdir==true)&&(counter>0)&&(pathIsInIgnoreTable(globalItemArray[i]))))
						;
					else
						perror("lstat error");
					continue;
				}
			}
		}
		//printf("Array[%d]: %s with inode: %d\n", i, path,buf.st_ino);

		//code for -type option
		if (strcmp(option,"-type")==0){	
			if ((strcmp(value,"b")==0)&&(S_ISBLK(buf.st_mode))) 
				flag_delete = false;
			else if ((strcmp(value,"c")==0)&&(S_ISCHR(buf.st_mode))) 
				flag_delete = false;
			else if ((strcmp(value,"d")==0)&&(S_ISDIR(buf.st_mode))) 
				flag_delete = false;
			else if ((strcmp(value,"p")==0)&&(S_ISFIFO(buf.st_mode))) 
				flag_delete = false;
			else if ((strcmp(value,"f")==0)&&(S_ISREG(buf.st_mode))) 
				flag_delete = false;
			else if ((strcmp(value,"l")==0)&&(S_ISLNK(buf.st_mode))) 
				flag_delete = false;
			else if ((strcmp(value,"s")==0)&&(S_ISSOCK(buf.st_mode))) 
				flag_delete = false;
		}

		//code for -inum option
		if (strcmp(option,"-inum")==0){	
			int inode_number = atoi(value);
			if (inode_number == buf.st_ino)
				flag_delete = false;
		}

		//code for -uid and -user option
		if ((strcmp(option,"-uid")==0) || (strcmp(option,"-user")==0)){	
			int uid_number = atoi(value);
			if (uid_number == buf.st_uid)
				flag_delete = false;
		}

		//code for -gid and -group option
		if ((strcmp(option,"-gid")==0) || (strcmp(option,"-group")==0)){	
			int gid_number = atoi(value);
			if (gid_number == buf.st_gid)
				flag_delete = false;
		}

		//code for -maxdepth option
		if (strcmp(option,"-maxdepth")==0){	
			int limit = atoi(value);
			if (path_depth <= limit)
				flag_delete = false;
		}

		//code for -mindepth option
		if (strcmp(option,"-mindepth")==0){	
			int limit = atoi(value);
			if (path_depth >= limit)
				flag_delete = false;
		}

		//code for -newer option
		if (strcmp(option,"-newer")==0){	
			if (buf.st_mtime > modif_time)
				flag_delete = false;
		}

		//code for -name option
		if (strcmp(option,"-name")==0){	
			char pattern [100];
			int j;
			if ((value[0] == '"') && (value[strlen(value)-1] == '"')){
				for (j=1;j<strlen(value)-1;j++)
        			pattern[j-1] = value[j];
				pattern[j-1] = '\0';
			}else{
				strcpy(pattern,value);
			}
			char * str = printRelativePath(globalItemArray[i]);

			//printf("pattern:%s\n",pattern);
			//printf("string:%s\n",str);
			if( fnmatch( pattern, str, 0 ) == 0 ) {      
          		flag_delete = false;
        	}else{
				flag_delete = true;
			}
		}

		//code for -iname option
		if (strcmp(option,"-iname")==0){	
			char pattern [100];
			int j;
			if ((value[0] == '"') && (value[strlen(value)-1] == '"')){
				for (j=1;j<strlen(value)-1;j++)
        			pattern[j-1] = value[j];
				pattern[j-1] = '\0';
			}else{
				strcpy(pattern,value);
			}

			char * str = printRelativePath(globalItemArray[i]);

			//printf("pattern:%s\n",pattern);
			//printf("string:%s\n",str);			

			//lowercase both pattern and string
			for(j = 0; pattern[j]; j++){
  				pattern[j] = tolower(pattern[j]);
			}
			for(j = 0; str[j]; j++){
  				str[j] = tolower(str[j]);
			}

			if( fnmatch( pattern, str, 0 ) == 0 ) {      
          		flag_delete = false;
        	}else{
				flag_delete = true;
			}
		}

		//desicion about the current file/folder
		if (flag_delete == true){
			globalItemArray[i] = "\0";
		}else{
			sprintf(globalItemArray[i],"%s\t%d",path,path_depth);
		}
	}
}

// elexe symbolic links -P,-H,-L
void optionExecute(char symlink_option [],char option []){
	struct stat buf;
	struct passwd * p;
	struct group * g;
	int modif_time;
	int i;

	//code for -depth option
	if (strcmp(option,"-depth")==0){	
		reverse_order = 1;
	}

	//code for -version and --version option
	if ((strcmp(option,"-version")==0) || (strcmp(option,"--version")==0)){	
		reverse_order = -1;
		printf("VERSION of FIND: find (GNU findutils) 4.5.11\n");
	}

	//code for -help and --help option
	if ((strcmp(option,"-help")==0) || (strcmp(option,"--help")==0)){	
		reverse_order = -1;
		printf("HELP with FIND:\n");
		printf("Usage: find [-H] [-L] [-P] [-Olevel] [-D help|tree|search|stat|rates|opt|exec] [path...] [expression]\n");
		printf("For further information do the following steps:\n");
		printf("STEP 1: open a LINUX terminal\n");
		printf("STEP 2: type <<man find>> and press enter\n");
	}

	//code for -mount and -xdev option
	if ((strcmp(option,"-mount")==0) || (strcmp(option,"-xdev")==0)){
		bool flag_delete = false;
		bool delete_previous = false;	
		for (i=0;i<rows;i++){
			char * path = strtok(globalItemArray[i], "\t");
			char * depth_str = strtok(NULL,"\t");
			int path_depth = atoi(depth_str);
			char * path_dup = strdup(path);

			struct stat file_stat;
			struct stat parent_stat;

			/* get the parent directory  of the file */
  			char * parent_name = dirname(path);

			/* get the file's stat info */
			if( -1 == stat(path, &file_stat) ) {
				if (((flag_ignore_readdir==true)&&(counter==0))||((flag_ignore_readdir==true)&&(counter>0)&&(pathIsInIgnoreTable(path))))
					;
				else
					perror("stat error");
				exit(-1);
			}

			/* determine whether the supplied file is a directory
    		if it isn't, then it can't be a mountpoint. */
			if( !(file_stat.st_mode & S_IFDIR) ) {
				flag_delete = delete_previous;
			}else{
				/* get the parent's stat info */
				if( -1 == stat(parent_name, &parent_stat) ) {
					if (((flag_ignore_readdir==true)&&(counter==0))||((flag_ignore_readdir==true)&&(counter>0)&&(pathIsInIgnoreTable(parent_name))))
						;
					else
						perror("stat error");
					exit(-1);
				}

				/* if file and parent have different device ids,
				then the file is a mount point
				or, if they refer to the same file,
				then it's probably the root directory '/'
				and therefore a mountpoint */
				if( file_stat.st_dev != parent_stat.st_dev || ( file_stat.st_dev == parent_stat.st_dev &&file_stat.st_ino == parent_stat.st_ino )) {
					//is a mountpoint (keep the directory and the files inside)
					delete_previous = false;
					flag_delete = false;
				} else {
					//is NOT a mountpoint (delete the directory and the files inside);
					delete_previous = true;
					flag_delete = false;
				}
			}

			//desicion about the current file/folder
			if (flag_delete == true){
				globalItemArray[i] = "\0";
			}else{

				sprintf(globalItemArray[i],"%s\t%d",path_dup,path_depth);
			}
		}
	}
}

void printTable(){
	int i;
	char pattern [100];
	strcpy(pattern,cur_dir);
	strcat(pattern,"/");
	//printf("\n++++++++++++++++++++++++++++++++++result+++++++++++++++++++++++++++++++++++++++++\n");
	if (reverse_order == 0){
		for (i=0;i<rows;i++){
			removeSubstr(globalItemArray[i],pattern);
			char * path = strtok(globalItemArray[i], "\t");
			printf("%s\n",path);
			//printf("Array[%d]: %s\n", i, globalItemArray[i]);
		}
	}else if (reverse_order == 1){
		for (i=rows-1;i>=0;i--){
			removeSubstr(globalItemArray[i],pattern);
			char * path = strtok(globalItemArray[i], "\t");
			printf("%s\n",path);
			//printf("Array[%d]: %s\n", i, globalItemArray[i]);
		}		
	}
	//printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
}

int main(int argc, char *argv[]){
	strcpy(cur_dir,getCurrentDir());
	char from_table [19][30] = {{"-newer"},{"-type"},{"-uid"},{"-user"},{"-gid"},{"-group"},{"-name"},{"-iname"},{"-inum"},{"-depth"},{"-help"},{"--help"},{"-ignore_readdir_race"},{"-maxdepth"},{"-mindepth"},{"-mount"},{"-version"},{"--version"},{"-xdev"}};
	
	int i,j,p=0;
	int pointer_first_expression = argc;
	int pointer_last_symlink = 0;


	//STEP 1: find the positions of the first expression option and the last symlink option
	for (i=argc-1;i>=0;i--)
		for (j=0;j<19;j++)
			if (strcmp(argv[i],from_table[j]) == 0)
				pointer_first_expression = i;			
			
	for (i=0;i<argc;i++)
		if ((strcmp(argv[i],"-P") == 0) || (strcmp(argv[i],"-L") == 0) || (strcmp(argv[i],"-H") == 0))
			pointer_last_symlink = i;			
		
	bool flag_def_path = false;
	int num_of_paths = 0 ;
	num_of_paths = (pointer_first_expression - 1) - (pointer_last_symlink + 1) + 1;
	if (num_of_paths == 0){
		flag_def_path = true;
		num_of_paths = 1;
	}

	char path_table [num_of_paths][100];
	if (flag_def_path)
		strcpy(path_table[0],".");
 	else
		for(i=pointer_last_symlink+1;i<=pointer_first_expression-1;i++){
			strcpy(path_table[p],argv[i]);
			p=p+1;
		}

	for(i=0;i<num_of_paths;i++){

		if (strcmp(path_table[i],"..") == 0){
			char base [100];	
			strcpy(base,getCurrentDir());
			char character = '/';
  
    			char* ptr = strrchr(base, character);
    			int end = ptr-base;
    
    			char new_path[100];
    			memcpy( new_path, &base[0], (end+1) );
    			new_path[(end+1)] = '\0';
    
			strcpy(path_table[i],new_path);
		}

		else if (strcmp(path_table[i],".") == 0)
			strcpy(path_table[i],getCurrentDir());

		else if (strcmp(&path_table[i][0],"/") != 0){
			char base [100];
			strcpy(base,getCurrentDir());
			strcat(base,"/");
			strcat(base,path_table[i]);
			if (base[strlen(base)-1] != '/')
				strcat(base,"/");
			strcpy(path_table[i],base);
		}
	}		
/*
	printf("++++++++++++++++++++++++++++++++++testing+++++++++++++++++++++++++++++++++++++++++\n");
	printf("pointer_first_expression = %d   loading . . . \n",pointer_first_expression);
	for(i=pointer_first_expression;i<argc;i++)
		printf("%s \n",argv[i]);
	printf("---------------------------------------------------------------------------------\n");
	printf("pointer_last_symlink = %d 	loading . . . \n",pointer_last_symlink);
	for(i=1;i<=pointer_last_symlink;i++)
		printf("%s \n",argv[i]);
	printf("---------------------------------------------------------------------------------\n");
	printf("num_of_paths = %d   		loading . . . \n",num_of_paths);
	for(i=0;i<num_of_paths;i++)
		printf("%d ) %s\n",i,path_table[i]);
	printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
*/
	//STEP 2: crawl all files based on each path in path_table
	globalItemArray = malloc(arrayIndexes * sizeof(*globalItemArray));
	globalItemArray[0] = malloc(254 * sizeof(char));

	for(i=0;i<num_of_paths;i++){		
		//path_table[i] ends with '/'
		strcpy(absolute_path,"");

		sprintf(globalItemArray[arrayIndexes-1],"%s\t%d",path_table[i],current_depth);
		arrayIndexes+=1;
		globalItemArray = realloc(globalItemArray, arrayIndexes*sizeof(*globalItemArray));
		globalItemArray[arrayIndexes-1] = malloc(100 * sizeof(char*));

		char subpath [100];
		if (path_table[i][strlen(path_table[i])-1] == '/'){
			int j=0;
			while (j < strlen(path_table[i])-1){
				subpath[j] = path_table[i][j];
				j++;
			}
   			subpath[j] = '\0';
		}else{
			strcpy(subpath,path_table[i]);
		}

		//printf(">> crawling of [[%s]] . . . \n",subpath); 
		printdir(subpath,0);	
	}
	arrayIndexes-=1;

/*	
	see all items inside the array (be careful: non ascii characters need to be removed)
	printf("be careful: non ascii characters need to be removed\n");
	printf("arrayIndexes:%d\n",arrayIndexes);
	for (i=0;i<arrayIndexes;i++)
		printf("Array[%d]: %s\n", i, globalItemArray[i]);
*/

	for (i=0;i<arrayIndexes;i++){
		bool ascii_flag = isASCII(globalItemArray[i]);
		if (ascii_flag == false)
			globalItemArray[i] = "\0";
	}

	clearGlobalTable();
/*	
	printf("check ------------> \n");
	printf("rows:%d\n",rows);
	printf("arrayIndexes:%d\n",arrayIndexes);
	for (i=0;i<rows;i++)
		printf("Array[%d]: %s\n", i, globalItemArray[i]);
*/
	//check option -ignore_readdir_race
//	printf("Sleeping for 5 seconds.\n");
//   sleep(5);
	   
	//STEP 3: read and execute each option
	char symlink_option [] = "-P";
	for(i=1;i<=pointer_last_symlink;i++)
		strcpy(symlink_option,argv[i]);
//	printf("symlink option is : %s \n",symlink_option);

	//give priority to option "-ignore_readdir_race"
	i=pointer_first_expression;
	while (i<argc){
		if (strcmp(argv[i],"-ignore_readdir_race") == 0){
			flag_ignore_readdir = true;
			int c = i+1;
			while((c<argc)&&(argv[c][0]!='-')){
				counter+=1;
				c+=1;
			}

			int j;
			ignore_readdir_table = (char **)malloc(counter * sizeof(char *));
			//printf("making the ignore_readdir_table . . . \n");
			for (j=0;j<counter;j++){
				char tmp [100];
				strcpy(tmp,cur_dir);
				strcat(tmp,"/");
				strcat(tmp,argv[i+1+j]);
				ignore_readdir_table[j] = (char *)malloc(100 * sizeof(char));
				strcpy(ignore_readdir_table[j],tmp);
				//printf("ignore_readdir_table[%d]:%s\n",j,ignore_readdir_table[j]);
			}
			i = c;
		}
		i+=1;
	}
	//printf("Flag_ignore_readdir:%d\n",flag_ignore_readdir);	//true = 1

	i=pointer_first_expression;
	while (i<argc){
		if ((strcmp(argv[i],"-newer") == 0) || (strcmp(argv[i],"-type") == 0) || (strcmp(argv[i],"-uid") == 0) || (strcmp(argv[i],"-user") == 0) || (strcmp(argv[i],"-gid") == 0) || (strcmp(argv[i],"-group") == 0) || (strcmp(argv[i],"-name") == 0) || (strcmp(argv[i],"-iname") == 0) || (strcmp(argv[i],"-inum") == 0) || (strcmp(argv[i],"-maxdepth") == 0) || (strcmp(argv[i],"-mindepth") == 0)){
			optionExecuteWithValue(symlink_option,argv[i],argv[i+1]);
			clearGlobalTable();
			i+=2;
		}else if ((strcmp(argv[i],"-depth") == 0) || (strcmp(argv[i],"-help") == 0) || (strcmp(argv[i],"--help") == 0) || (strcmp(argv[i],"-version") == 0) || (strcmp(argv[i],"--version") == 0) || (strcmp(argv[i],"-mount") == 0) || (strcmp(argv[i],"-xdev") == 0)){	
			optionExecute(symlink_option,argv[i]);
			clearGlobalTable();
			i+=1;
		}else{		//option "-ignore_readdir_race"
			i+=1;
		}
	}

	printTable();

	if (flag_ignore_readdir == true){
		for (j = 0; j < counter ; j++)
       			free(ignore_readdir_table[j]);      
    		free(ignore_readdir_table);
	}

	for (i = 0; i < rows ; i++)
       free(globalItemArray[i]);      
    free(globalItemArray);
	return 0;
}