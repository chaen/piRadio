/*
 * isr.c:
 *	Wait for Interrupt test program - ISR method
 *
 *	How to test:
 *	  Use the SoC's pull-up and pull down resistors that are avalable
 *	on input pins. So compile & run this program (via sudo), then
 *	in another terminal:
 *		gpio mode 0 up
 *		gpio mode 0 down
 *	at which point it should trigger an interrupt. Toggle the pin
 *	up/down to generate more interrupts to test.
 *
 * Copyright (c) 2013 Gordon Henderson.
 ***********************************************************************
 * This file is part of wiringPi:
 *	https://projects.drogon.net/raspberry-pi/wiringpi/
 *
 *    wiringPi is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    wiringPi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with wiringPi.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************
 */
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


// What GPIO input are we using?
//	This is a wiringPi pin number

#define	BUTTON_PIN	0

// globalCounter:
//	Global variable to count interrupts
//	Should be declared volatile to make sure the compiler doesn't cache it.

int numberOfRadio = 0;
#define MAX_RADIO 100
//char * radioList[] = { "http://stream.funradio.sk:8000/fun128.mp3.m3u",  "http://icecast.rtl.fr/rtl-1-48-72.aac?type=.flv"};
char * radioList[MAX_RADIO];
int segment_id; //Shared Memory ID
int segment_id2; //Shared Memory ID
int segment_id3; //Shared Memory ID
bool  * mem_changeRadio; //Shared Memory Pointer
bool  * mem_turnOffRadio; //Shared Memory Pointer
bool  * mem_inChildCode; //Shared Memory Pointer

/*void executeCommand(char * exec) {
        char cmd[1000] = "sudo -u pi /usr/bin/vlc -Idummy ";
        strcat(cmd, exec);
        FILE *in;
        char buff[1024];

        if (!(in = popen(cmd, "r"))) {
                exit(1);
        }

        while (fgets(buff, sizeof(buff), in) != NULL) {
                printf("%s\n",buff);
        }
        pclose(in);

}*/


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
        //char cmd[1000] = "sudo -u pi /usr/bin/vlc -Idummy ";
        //strcat(cmd, exec);
        printf("About to execute %s\n", exec);
        execl("/usr/bin/sudo", "sudo", "-u", "pi", "/usr/bin/vlc", "-Idummy", exec, (char*)0);
        //execl("/usr/bin/vlc", "vlc", "-Idummy", exec);
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

    printf("I go out of the delay\n The childpid is %d\n",childpid);
    printf("radioCounter = %d\n", radioCounter);
    printf("numberOfRadio = %d\n", numberOfRadio);

    *mem_inChildCode = true;
    *mem_changeRadio = false;

    if (childpid){
        printf("There is a child to kill %d\n",childpid);
        kill(childpid, SIGTERM);
        childpid = 0;
    } else {
        printf("There is no child to kill\n");
    }

    if (*mem_turnOffRadio){
      radioCounter = numberOfRadio;
      *mem_turnOffRadio = false;
    }

    if (radioCounter == numberOfRadio){
        printf("End of list, back to the beginning\n");
        radioCounter = 0 ;
    } else {
        childpid = fork();
    
        if (childpid >= 0) /* fork succeeded */
        {
            if (childpid == 0) /* fork() returns 0 to the child process */
            {
                printf("CHILD: I am the child process!\n");
                printf("CHILD: Here's my PID: %d\n", getpid());
                printf("CHILD: My parent's PID is: %d\n", getppid());
                int radioChanel = radioCounter;
                printf("Radio chanel : %s\n", radioList[radioChanel]);
                executeCommand(radioList[radioChanel]);
                //exit(0);
                //executeCommand("sudo -u pi /usr/bin/vlc -Idummy http://stream.funradio.sk:8000/fun128.mp3.m3u");
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
    printf("At the end radioCounter : %d\n",radioCounter);
  }
}

void changeRadio(bool off){
    printf("I get the interrupt %d\n", getpid());
    if (!(*mem_inChildCode)){
        if (off){
          *mem_turnOffRadio = true;
        }
        *mem_changeRadio = true;
        
    }
    else
        printf("wait, I am in childCode\n");
}

void myInterrupt (void)
{
  printf("I am now in the interrupt\n");
  int buttonUp;
  delay(200);
  buttonUp  = digitalRead(8);
  if (!buttonUp){
    delay(800);
    buttonUp  = digitalRead(8);
    if (buttonUp){
      printf("We want to change radio\n");
      changeRadio(false);
    } else {
      delay(500);
      buttonUp = digitalRead(8);
      if(buttonUp){
        printf("Just a long push? lets change radio...\n");
        changeRadio(false);
      } else {
        printf("We want to shut it off\n");
        changeRadio(true);
      }
    }
  } else {
    printf("Too short. Was a glitch\n");
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
    printf("\t-f [filename] : file from which to read the list of radio\n");
}

/*
 *********************************************************************************
 * main
 *********************************************************************************
 */

int main (int argc, char ** argv)
{

  int runAsDaemon = false;
  char * radioListFile = NULL;
  int index, c; 


  while ((c = getopt (argc, argv, "df:")) != -1){
    switch (c)
       {
       case 'd':
         runAsDaemon = true;
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
  printf("radioListFile : %s\n", radioListFile);
  printf("runAsDaemon : %d\n", runAsDaemon);


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
     printf ("Je suis le fils !\n");  
     childCode(); 
  } else {  
     printf ("Je suis le père !\n");  
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
