#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>     /* va_list, va_start, va_arg, va_end */


// What GPIO input are we using?
//	This is a wiringPi pin number

#define	BUTTON_PIN	0

int verbose = 0;
int runAsDaemon = 0;
int numberOfRadio = 0;
#define MAX_RADIO 100
char * radioList[MAX_RADIO];
int segment_id; //Shared Memory ID
int segment_id2; //Shared Memory ID
int segment_id3; //Shared Memory ID
bool  * mem_changeRadio; //Shared Memory Pointer
bool  * mem_turnOffRadio; //Shared Memory Pointer
bool  * mem_inChildCode; //Shared Memory Pointer

int myLog (int level, const char *format, ...)
{
    va_list args;
    va_start (args, format);
    if (runAsDaemon)
    {
        vsyslog(level, format, args);
    }   
    else
    {
        if (level == LOG_DEBUG) printf("[LOG_DEBUG] ");
        else if (level == LOG_INFO) printf("[LOG_INFO] ");
 
        vprintf(format, args);
    }
    va_end(args);
}





void readRadioList(char * filename){
   FILE *file = fopen ( filename, "r" );
   int idRadio = 0;
   if ( file != NULL )
   {
      char line [ 256 ]; /* or other suitable maximum line size */
      while ( fgets ( line, sizeof line, file ) != NULL ) /* read a line */
      {
         radioList[idRadio] = (char*)malloc(sizeof(char)*(strlen(line)+1));
         strncpy(radioList[idRadio], line, strlen(line) -1 ); 
         numberOfRadio++;
         idRadio++;
         //fputs ( line, stdout ); /* write the line */
      }
      fclose ( file );
   }
   else
   {
      perror ( filename ); /* why didn't the file open? */
      exit(1);
   }

   while(idRadio < MAX_RADIO){
       radioList[idRadio] = NULL;
       idRadio++;
   }
}



void executeCommand(char * exec) {
        myLog(LOG_DEBUG, "About to execute %s\n", exec);
        execl("/usr/bin/sudo", "sudo", "-u", "pi", "/usr/bin/vlc", "-Idummy", exec, (char*)0);
        perror("execl() failure!\n\n");
}



/*
 * myInterrupt:
 *********************************************************************************
 */

void childCode(void){

 pid_t childpid = 0;
 int radioCounter = 0 ;
 while(1){

    while(!(*mem_changeRadio)){
        delay(1000);
    }

    myLog(LOG_DEBUG, "I go out of the delay\n The childpid is %d\n",childpid);
    myLog(LOG_DEBUG, "radioCounter = %d\n", radioCounter);
    myLog(LOG_DEBUG, "numberOfRadio = %d\n", numberOfRadio);

    *mem_inChildCode = true;
    *mem_changeRadio = false;

    if (childpid){
        myLog(LOG_DEBUG, "There is a child to kill %d\n",childpid);
        kill(childpid, SIGTERM);
        childpid = 0;
    } else {
        myLog(LOG_DEBUG, "There is no child to kill\n");
    }

    if (*mem_turnOffRadio){
      radioCounter = numberOfRadio;
      *mem_turnOffRadio = false;
    }

    if (radioCounter == numberOfRadio){
        myLog(LOG_DEBUG, "End of list, back to the beginning\n");
        radioCounter = 0 ;
    } else {
        childpid = fork();
    
        if (childpid >= 0) /* fork succeeded */
        {
            if (childpid == 0) /* fork() returns 0 to the child process */
            {
                myLog(LOG_DEBUG, "CHILD: I am the child process!\n");
                myLog(LOG_DEBUG, "CHILD: Here's my PID: %d\n", getpid());
                myLog(LOG_DEBUG, "CHILD: My parent's PID is: %d\n", getppid());
                int radioChanel = radioCounter;
                myLog(LOG_INFO, "Radio chanel : %s\n", radioList[radioChanel]);
                executeCommand(radioList[radioChanel]);
            } else {
               radioCounter++;
            }
        }
        else /* fork returns -1 on failure */
        {
            perror("fork"); /* display error message */
            exit(0); 
        }
     
    }
    *mem_inChildCode = false;
    myLog(LOG_DEBUG, "At the end radioCounter : %d\n",radioCounter);
  }
}

void changeRadio(bool off){
    myLog(LOG_INFO, "I get the interrupt %d\n", getpid());
    if (!(*mem_inChildCode)){
        if (off){
          *mem_turnOffRadio = true;
        }
        *mem_changeRadio = true;
        
    }
    else
        myLog(LOG_DEBUG, "wait, I am in childCode\n");
}

void myInterrupt (void)
{
  myLog(LOG_INFO, "I am now in the interrupt\n");
  int buttonUp;
  delay(200);
  buttonUp  = digitalRead(8);
  if (!buttonUp){
    delay(800);
    buttonUp  = digitalRead(8);
    if (buttonUp){
      myLog(LOG_INFO, "We want to change radio\n");
      changeRadio(false);
    } else {
      delay(500);
      buttonUp = digitalRead(8);
      if(buttonUp){
        myLog(LOG_INFO, "Just a long push? lets change radio...\n");
        changeRadio(false);
      } else {
        myLog(LOG_INFO, "We want to shut it off\n");
        changeRadio(true);
      }
    }
  } else {
    myLog(LOG_INFO, "Too short. Was a glitch\n");
  }
}




void daemonize(){

   /* Our process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
            exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then
 *          we can exit the parent process. */
    if (pid > 0) {
            exit(EXIT_SUCCESS);
    }

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0) {
            /* Log the failure */
            exit(EXIT_FAILURE);
    }

        /* Close out the standard file descriptors */
//      close(STDIN_FILENO);
//      //      close(STDOUT_FILENO);
//      //      close(STDERR_FILENO);
//              return 0;
//


    freopen( "/dev/null", "w", stderr );
    freopen( "/dev/null", "w", stdout );
}

void usage(){
    printf("This program is made to react to the GPIO input and stream webradio using vlc\n");
    printf("Options : \n");
    printf("\t-d : run as a daemon\n");
    printf("\t-v : verbose output\n");
    printf("\t-f [filename] : file from which to read the list of radio\n");
}

/*
 *********************************************************************************
 * main
 *********************************************************************************
 */

int main (int argc, char ** argv)
{

  char * radioListFile = NULL;
  int index, c; 



  while ((c = getopt (argc, argv, "df:")) != -1){
    switch (c)
       {
       case 'd':
         runAsDaemon = 1;
         break;
       case 'f':
         radioListFile = (char *)malloc(sizeof(char)*(strlen(optarg)+1));
         strncpy(radioListFile, optarg, strlen(optarg) + 1);
         break;
       case '?':
         if (optopt == 'f')
           fprintf (stderr, "Option -%c requires an argument.\n", optopt);
         else if (isprint (optopt))
           fprintf (stderr, "Unknown option `-%c'.\n", optopt);
         else
           fprintf (stderr,
                    "Unknown option character `\\x%x'.\n",
                    optopt);
         usage();
         return 1;
       default:
         abort ();
       }
  }

  if (!radioListFile){
      printf("Need radioListFile\n");
      usage();
      exit(1);
  }


  myLog(LOG_INFO, "Starting piRadio");
  myLog(LOG_INFO, "radioListFile : %s\n", radioListFile);
  myLog(LOG_INFO, "runAsDaemon : %d\n", runAsDaemon);


  if (wiringPiSetup () < 0)
  {
    fprintf (stderr, "Unable to setup wiringPi: %s\n", strerror (errno)) ;
    return 1 ;
  }

  if (runAsDaemon)
      daemonize();

  readRadioList(radioListFile);
  segment_id = shmget(IPC_PRIVATE, sizeof(bool), S_IRUSR|S_IWUSR);
  segment_id2 = shmget(IPC_PRIVATE, sizeof(bool), S_IRUSR|S_IWUSR);
  segment_id3 = shmget(IPC_PRIVATE, sizeof(bool), S_IRUSR|S_IWUSR);
  mem_changeRadio = (bool *) shmat(segment_id,NULL,0);
  mem_turnOffRadio = (bool *) shmat(segment_id3,NULL,0);
  mem_inChildCode = (bool *) shmat(segment_id2,NULL,0);
  *mem_changeRadio = false;
  *mem_turnOffRadio = false;
  *mem_inChildCode = false;


  pid_t pid = fork();  
  
  if (pid == 0) {  
     myLog(LOG_INFO, "Je suis le fils !\n");  
     childCode(); 
  } else {  
     myLog(LOG_INFO, "Je suis le père !\n");  
    if (wiringPiISR (8, INT_EDGE_FALLING, &myInterrupt) < 0)
    {
      fprintf (stderr, "Unable to setup ISR: %s\n", strerror (errno)) ;
      return 1 ;
    }
    while(1){
        sleep(1);
    }
  }  
  
 
    shmdt(mem_changeRadio);
    shmdt(mem_turnOffRadio);
    shmdt(mem_inChildCode);
    //remove shared memory segment
    shmctl(segment_id,IPC_RMID,NULL);
    shmctl(segment_id2,IPC_RMID,NULL);
    shmctl(segment_id3,IPC_RMID,NULL);
  return 0 ;
}
