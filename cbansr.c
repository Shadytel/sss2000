
// For C99 support

#if __STDC_VERSION__ >= 199901L
# define _XOPEN_SOURCE 600
#else
# define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

/**
 ** System Header Files
 **/

#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>

/**
 **  Dialogic header files 
 **/


#include <srllib.h>
#include <dxxxlib.h>
#include <dtilib.h>
#include <sctools.h>

/**
 **  Application include files
 **/

#include "cbansrx.h" // Dialogic specific function prototype and defines
#include "dispx.h"   // Display function prototype and defines

/**
 ** Global Variables
 **/
 
// FILE* debugfile;
short connchan[MAXCHANS + 1];
char	tmpbuff[ 256 ];		/* Temporary Buffer */
DX_INFO		dxinfox[ MAXCHANS+1 ];

/**
 **  Display routine defines and variables
 **/

#define MAX_STR_LEN 255
#define CT_NTTEST   0x06 /* Robbed bit FXS/FXO signaling test */
#define GC_USER_BUSY 0x11 // Q.850 cause code 17
extern __sighandler_t sigset(int __sig, __sighandler_t __disp) __THROW;

static char version[] = "13.37";
static char dialogic[] = "ThoughtPhreaker's Sooper Dooper SSS-2000 Channel Bank Hatbowl"; 

/**
 ** Variable Definitions
 **/

// Dialogic function variables
int end = 0;

/**
 ** Global data
 **/
 
 short startchan;
 short isdnmax;
 short isdnbdnum;

 #define EXTENSIONS 24

/*
 * File Descriptors for VOX Files
 */
int introfd;
int invalidfd;
int goodbyefd;

short maxchans = EXTENSIONS;		/* Default Number of D/4x Channels to use */
int d4xbdnum = 1;		/* Default D/4x Board Number to Start use */
int dtibdnum = 1;		/* Default DTI Board Number to Start use */
int frontend = CT_NTTEST;	/* Default network frontend is ANALOG */
int scbus    = TRUE;		/* Default Bus mode is SCbus */
int routeag  = FALSE;		/* Route analog frontend to resource ??? */
int boardtag = TRUE;		/* Default mode CCM */

/*
 * Variables and whatnot for the SSS-2000 code
 */
 unsigned char dignum[ MAXCHANS+1 ];
 char modifier[ MAXCHANS+1 ] = {0};
 char ownies[ MAXCHANS+1 ] = {0};
 unsigned long playoffset[ MAXCHANS+1 ];
 unsigned long playoffset2[ MAXCHANS+1 ]; // I think we can do with just one. The original code for this pre-dates me knowing my shit, or anything close to it.
 int anncnum[ MAXCHANS+1 ];
 int anncnum2[ MAXCHANS+1 ];
 int minannc[ MAXCHANS+1 ];
 int maxannc[ MAXCHANS+1 ];
 unsigned char errcnt[ MAXCHANS+1 ];  /* Error counter */
 int currentlength[ MAXCHANS ];
 int file[25]; // For readback function.
 
 char defaultcpn[15]; // Default calling party number for ISDN trunks
 char login[15]; // DISA external destination
 char ivrtest[11]; // Destination for test IVR from external trunks
 char password[16]; // DISA password
 char operator[12]; // Operator extension
 char dntable[ EXTENSIONS ][5];
 char name[ EXTENSIONS ][17] = {0};
 unsigned char dnlookup[1024] = {0}; // For our inefficient lookup thing.
 
 //time_t hosttimer[MAXCHANS+1];
 struct timespec hosttimer[MAXCHANS+1];
 struct timespec hosttimer2[MAXCHANS+1];
 //time_t hosttimer2[MAXCHANS+1];
 char fxo[MAXCHANS + 1] = { 0 };
 static char disapasscode[7] = { 0x32, 0x31, 0x33, 0x32, 0x31, 0x33, 0x00 }; // Just a pre-defined hatbowl for the moment. Later, it'll be something in a config file.
 struct stat sts;
 DTCAS_CREATE_TRAIN_MSG reversaltest;
 DTCAS_REPLY_MSG reversal_response;
 DTCAS_TRANSMIT_MSG reversalxmit;
 DTCAS_TRANSMIT_MSG reversalxmit_response;
 DTCAS_ENA_DIS_TEMPLATE_MSG reversalenable;
 DTCAS_REPLY_MSG reversal_enbresponse;
 DTCAS_CLEAR_ALL_TEMPLATE_MSG clearmsg;
 DTCAS_REPLY_MSG clearreply;

 void isdn_prep();
 bool set_corruptprog(short channum, char msgtype);
 int playtone_rep( int channum, int toneval, int toneval2, int amp1, int amp2, int ontime, int pausetime, int cycles);
 int isdn_hkstate(short channum, int state);
 
 void isdn_origtest( short channum, char * dest, char * cpn ) {
      if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_RINGS | DM_DIGOFF ) == -1 ) {
           sprintf( tmpbuff, "Cannot set CST events for %s",
           ATDV_NAMEP( dxinfox[ channum ].chdev ) );
           disp_status(channum, tmpbuff );
    }
    connchan[channum] = 25;
    while ((dxinfox[connchan[channum]].state != ST_WTRING) && (connchan[channum] <= MAXCHANS)) {
        connchan[channum]++;
    }

    sprintf(tmpbuff, "Dest. channel is %d", connchan[channum]);
    disp_msg(tmpbuff);

    if (connchan[ channum ] > MAXCHANS) {  // Error handling for all circuits being busy; make sure we're not trying to dial out on the D channel
        // Error handling for all circuits being busy
        connchan[channum] = 0;
        /*
        dxinfox[channum].state = ST_REORDER;
        playtone_rep(channum, 480, 620, -24, -26, 25, 25, 40);
        */

        // This comes later; when announcement support is created.
        dxinfox[ channum ].state = ST_ACBANNC;
        dxinfox[ channum ].msg_fd = open("sounds/error/acb2.pcm", O_RDONLY);
        if ( ATDX_STATE( dxinfox[channum].chdev ) != CS_IDLE ) {
            dx_stopch( dxinfox[channum].chdev, (EV_ASYNC | EV_NOSTOP) );
        }
        else if (play(channum, dxinfox[ channum ].msg_fd, 0x81, 0)  == -1) {
            dxinfox[channum].state = ST_REORDER;
            playtone_rep(channum, 480, 620, -25, -27, 25, 25, 40);
        }
        

    disp_msg("Error: all circuits are busy");
    return;
    }

    connchan[connchan[channum]] = channum;
    dxinfox[channum].state = ST_ROUTEDISDN;
    dxinfox[connchan[channum]].state = ST_ROUTEDISDN2;

    if ((name[channum][0] != 0x00) && (!(modifier[channum] & 1))) set_cpname(connchan[channum], name[channum]);
    makecall(connchan[channum], dest, cpn);   // Call the number in the digit buffer

    return;
}

void confparse() {
    FILE *configfile;
    char line[1500]; // Max string size is 1500 characters
    char *parsedstr;
    int index;
    unsigned char extension;

    if ((configfile = fopen("main.conf", "r")) == NULL) {
        disp_msg("Unable to find main.conf. Using default configuration options...");
        sprintf(ivrtest, "3974");
        sprintf(defaultcpn, "3975");
        sprintf(password, "31337");
        sprintf(login, "3975");
        sprintf(operator, "3000");
    }

    else {

        disp_msg("Opening configuration file");

        while (fgets(line, 1500, configfile)) {
            index = (strlen(line) - 1);

            while (index && (line[index] <= ' ')) {
                // Why does it do this? Is anything below 0x21 considered a null character?
                disp_msg("Condition 1 met");
                line[index] = '\0';
                index--;
            }

            /* ignore blank lines */
            if (index == 0) {
                disp_msg("Condition 2 met");
                continue;
            }

            // If the first character isn't alpha-numeric, it's a comment. Or a typo. Or ignorable anyway.

            if ((line[0] < 'A') || (line[0] > 'z')) { /* simplifed non-alpha check */
                if( (line[0] < 0x40) && (line[0] > 0x29) ) {
                    parsedstr = strtok(line, " ");
                    extension = (unsigned char) atoi(parsedstr);
                    if ( (extension > 0) && (extension < 25) ) {
                        parsedstr = strtok(NULL, " ");
                        if (strcmp("=>", parsedstr) == 0) {
                            parsedstr = strtok(NULL, " ");
                            if (strlen(parsedstr) < 11) {
                                strcpy(dntable[extension], parsedstr);
                                // This is fuuuuuugly, but for a single hundred block, it works fine
                                dnlookup[( (unsigned short) atoi(parsedstr) & 0x3FF)] = extension;
                            }
                        }
                    }
                }
                continue; /* Ignore the line and read next */
            }
            
            if (line[0] == 'f') {   // This is either default or dialer CPN
                parsedstr = strtok(line, " ");

                if ( (parsedstr[1] == 'x') &&
                     (parsedstr[2] == 'o') ) {
                    disp_msg("Parsing FXO");
                    extension = atoi(parsedstr + 3);
                    if ( (extension > 0) && (extension < 25 ) ) {

                        parsedstr = strtok(NULL, " ");

                        if (strcmp("=>", parsedstr) != 0) {
                            // Process error and keep going
                            disp_msg("Error processing login numer");
                            continue;
                        }

                        parsedstr = strtok(NULL, " ");

                        if (parsedstr != NULL) {
                            // Parse FXO setting
                            fxo[extension] = atoi(parsedstr);
                        } else {
                            disp_msg("FXO entry empty. Skipping...");
                        }
                        
                    }

                    continue;
                }

                else {
                    disp_msg("Unable to identify configuration parameter starting with l. Skipping...");
                }

                continue;
            }
            
            if (line[0] == 'n') {   // This is either default or dialer CPN
                char *ptr;
                parsedstr = strtok_r(line, " ", &ptr);

                if ( (parsedstr[1] == 'a') &&
                     (parsedstr[2] == 'm') ) {
                    disp_msg("Parsing name");
                    extension = atoi(parsedstr + 4);
                    if ( (extension > 0) && (extension < 25 ) ) {

                        parsedstr = strtok_r(NULL, " ", &ptr);

                        if (strcmp("=>", parsedstr) != 0) {
                            // Process error and keep going
                            disp_msg("Error processing login numer");
                            continue;
                        }

                        //parsedstr = strtok_r(NULL, " ", &ptr);

                        if (parsedstr != NULL) {
                            // Parse name setting
                            size_t length;
                            length = strlen(ptr);
                            sprintf(tmpbuff, "DEBUG: Length is %d", length);
                            disp_msg(tmpbuff);
                            length--;
                            if (length < 15) {
                                strcpy(name[extension], ptr);
                                parsedstr = strtok(NULL, " ");
                                // Get rid of the trailing newline
                                if (name[extension][length] == 0x0A) name[extension][length] = 0x00;
                                sprintf(tmpbuff, "DEBUG: Name is %s", name[extension]);
                                disp_msg(tmpbuff);
                            }
                            
                            else {
                                disp_msg("Name entry exceeds limit. Skipping...");
                            }
                        } else {
                            disp_msg("Name entry empty. Skipping...");
                        }
                        
                    }

                    continue;
                }

                else {
                    disp_msg("Unable to identify configuration parameter starting with n. Skipping...");
                }

                continue;
            }

            if (line[0] == 'l') {   // This is either default or dialer CPN
                parsedstr = strtok(line, " ");

                if (strcmp("login", parsedstr) == 0) {
                    disp_msg("Parsing login");
                    parsedstr = strtok(NULL, " ");

                    if (strcmp("=>", parsedstr) != 0) {
                        // Process error and keep going
                        disp_msg("Error processing login numer");
                        continue;
                    }

                    parsedstr = strtok(NULL, " ");

                    if (parsedstr != NULL) {
                        // Parse login destination
                        snprintf(login, 14, "%s", parsedstr);
                        // Just to make sure we don't overflow...
                        login[14] = '\0';
                    } else {
                        disp_msg("Login entry empty. Skipping...");
                    }

                    continue;
                }

                else {
                    disp_msg("Unable to identify configuration parameter starting with l. Skipping...");
                }

                continue;
            }

            if (line[0] == 'd') {   // This is either default or dialer CPN
                parsedstr = strtok(line, " ");

                if (strcmp("defaultcpn", parsedstr) == 0) {
                    disp_msg("Parsing defaultcpn");
                    parsedstr = strtok(NULL, " ");

                    if (strcmp("=>", parsedstr) != 0) {
                        // Process error and keep going
                        disp_msg("Error processing default CPN");
                        continue;
                    }

                    parsedstr = strtok(NULL, " ");

                    if (parsedstr != NULL) {
                        // Parse bridge number
                        snprintf(defaultcpn, 14, "%s", parsedstr);
                        // Just to make sure we don't overflow...
                        defaultcpn[14] = '\0';
                    }

                    else {
                        disp_msg("Default CPN entry empty. Skipping...");
                    }

                    continue;
                }

            }

            if (line[0] == 'i') {
                parsedstr = strtok(line, " ");
                if (strcmp("ivrtest", parsedstr) == 0) {
                    disp_msg("Parsing ivrtest");
                    parsedstr = strtok(NULL, " ");

                    if (strcmp("=>", parsedstr) != 0) {
                        // Process error and keep going
                        disp_msg("Error processing ivrtest number");
                        continue;
                    }

                    parsedstr = strtok(NULL, " ");

                    if (parsedstr != NULL) {
                        // Parse ivrtest
                        snprintf(ivrtest, 10, "%s", parsedstr);
                        // Just to make sure we don't overflow...
                        password[10] = '\0';
                    }

                    else {
                        disp_msg("ivrtest empty. Skipping...");
                    }

                    continue;
                }

            }

            if (line[0] == 'p') {
                parsedstr = strtok(line, " ");

                if (strcmp("password", parsedstr) == 0) {
                    disp_msg("Parsing password...");
                    parsedstr = strtok(NULL, " ");

                    if (strcmp("=>", parsedstr) != 0) {
                        // Process error and keep going
                        disp_msg("Error processing password number");
                        continue;
                    }

                    parsedstr = strtok(NULL, " ");

                    if (parsedstr != NULL) {
                        // Parse password
                        snprintf(password, 15, "%s", parsedstr);
                        // Just to make sure we don't overflow...
                        password[15] = '\0';
                    }

                    else {
                        disp_msg("password empty. Skipping...");
                    }

                    continue;
                }

            }
            
            if (line[0] == 'o') {
                parsedstr = strtok(line, " ");
                //disp_msg("Parsing operator...");

                if (strcmp("operator", parsedstr) == 0) {
                    disp_msg("Parsing operator...");
                    parsedstr = strtok(NULL, " ");

                    if (strcmp("=>", parsedstr) != 0) {
                        // Process error and keep going
                        disp_msg("Error processing operator number");
                        continue;
                    }

                    parsedstr = strtok(NULL, " ");

                    if (parsedstr != NULL) {
                        // Parse operator
                        snprintf(operator, 11, "%s", parsedstr);
                        // Just to make sure we don't overflow...
                        operator[11] = '\0';
                    }

                    else {
                        disp_msg("operator empty. Skipping...");
                    }

                    continue;
                }

            }

            disp_msg("Reached bottom of string processing loop.");
        }

        disp_msg("String processing finished.");

        // If the user didn't fill out any of the required destinations, make them default.

        if (ivrtest[0] == '\0') {
            sprintf(ivrtest, "3974");
        }

        if (defaultcpn[0] == '\0') {
            sprintf(defaultcpn, "3975");
        }

        if (login[0] == '\0') {
            sprintf(login, "3975");
        }

        if (password[0] == '\0') {
            sprintf(password, "31337");
        }
        
        if (operator[0] == '\0') {
            sprintf(operator, "3000");
        }

        sprintf(tmpbuff, "Login is %s", login);
        disp_msg(tmpbuff);

        if (configfile != NULL) {
            fclose(configfile);
        }

    }

    return;
}
 
/***************************************************************************
 *        NAME: void intr_hdlr()
 * DESCRIPTION: Handler called when one of the following signals is
 *		received: SIGHUP, SIGINT, SIGQUIT, SIGTERM.
 *		This function stops I/O activity on all channels and
 *		closes all the channels.
 *       INPUT: None
 *      OUTPUT: None.
 *     RETURNS: None.
 *    CAUTIONS: None.
 ***************************************************************************/
void intr_hdlr()
{
   disp_msg( "Process Terminating ...." );
   end = 1;
}


 /*********************************************************************
 *        NAME : main()
 * DESCRIPTION : Point of entry; makes shit happen.
 *    CAUTIONS : none.
 *********************************************************************/

 
int main(){

	// debugfile = fopen("debug.log", "a+");

    printf("%s - version %s\n", dialogic, version);
    disp_msg("FXS Driver/TDM Switch Development Module starting...");

    int mode;

    disp_msg("Starting Application...");
    chkargs();

    sigset( SIGHUP, (void (*)()) intr_hdlr );
    sigset( SIGINT, (void (*)()) intr_hdlr );
    sigset( SIGQUIT, (void (*)()) intr_hdlr );
    sigset( SIGTERM, (void (*)()) intr_hdlr );

   /*
    * Initialize System
    */
#ifndef SIGNAL
    /*
     * Set the Device to Polled Mode
     */
    mode = SR_POLLMODE;

    if (sr_setparm(SRL_DEVICE, SR_MODEID, &mode) == -1) {
        disp_msg("Unable to set to Polled Mode");
        exit(1);
    }

#endif

    /*
     * Set-up the fall-back handler
     */
    if (sr_enbhdlr(EV_ANYDEV, EV_ANYEVT, (EVTHDLRTYP)fallback_hdlr) == -1) {
        disp_msg("Unable to set-up the fall back handler");
        exit(1);
    }

    /*
     * Initialize System
     */
    sysinit();

    /**
     **   Main Loop
     **/
    while (1)  {
#ifndef SIGNAL
        sr_waitevt(1000);
#else
        sleep(1);
#endif

        if (end) {
            sys_quit();
            break;
        }
    }

    return (0);
}

int isdn_inroute( int channum ) {
    // This function exists to route inbound ISDN traffic.
    dignum[channum] = dnlookup[(unsigned short) atoi(isdninfo[channum].dnis) & 0x3FF];
    
    if (dignum[channum] != 0 ) {
        dxinfox[ channum ].state = ST_RINGPHONE1;
        ringphone( channum, dignum[channum] );
        return 0;
    }

    if (strcmp(isdninfo[channum].dnis, "8378") == 0) {
        disp_msg("DEBUG: About to send corruptprog!");
        set_corruptprog(channum, 2);
        set_corruptprog(channum, 2);
    }

    if (strcmp(isdninfo[channum].dnis, "9760002") == 0) {
        // This is a quick hack to check IMG2020 functionality
        disp_msg("DEBUG: The IMG2020 is stupid.");
        dignum[channum] = dnlookup[3978 & 0x3FF];
        dxinfox[ channum ].state = ST_RINGPHONE1;
        ringphone( channum, dignum[ channum ] );
    }
    
    // Function to reject all traffic.
    connchan[channum] = 0;
    dxinfox[channum].state = ST_REORDER;
    
    dxinfox[ channum ].msg_fd = open("sounds/error/toorcamp_nwn.pcm", O_RDONLY);

    if (play(channum, dxinfox[ channum ].msg_fd, 0x81, 0)  == -1) {
        set_hkstate(channum, DX_ONHOOK);
    }
    
    return 0;
}
 
 /***************************************************************************
 *        NAME: disp_status(chnum, stringp)
 *      INPUTS: chnum - channel number (1 - 12)
 *              stringp - pointer to string to display 
 * DESCRIPTION: display the current activity on the channel in window 2
 *              (the string pointed to by stringp) using chno as a Y offset
 *************************************************************************/
void disp_status (int chnum, char *stringp)
{
 
  
printf("[STATUS] Channel %d: %s\n", chnum, stringp);
  
}
 
/***************************************************************************
 *        NAME: disp_msg(hwnd, stringp)
 *      INPUTS: stringp - pointer to string to display.
 * DESCRIPTION: wrapper for display messages via printf
 *************************************************************************/
void disp_msg (char *stringp)
{
  
printf("[MSG] %s\n", stringp);
  return;

}


/***************************************************************************
 *        NAME: disp_err(chan_num, chfd, error )
 * DESCRIPTION: This routine prints error information.
 *      INPUTS: chan_num - channel number
 *		chfd - device descriptor
 *      error - error message
 *     OUTPUTS: The error code and error message are displayed via printf
 *    wrapper
 *    CAUTIONS: none. 
 ************************************************************************/
void disp_err( int chan_num, int chfd, char *error )
{
       	char *lasterr = ATDV_ERRMSGP( chfd );
	char *dev_name =  ATDV_NAMEP( chfd ) ;
    	
	if ( *lasterr == EDX_SYSTEM ) {
            printf("[ERROR] Channel: %d %s errno: %d, Device: %s\n",
            chan_num, error, errno, dev_name );
	} else {
            printf("[ERROR] Channel: %d %s lasterr: %s, Device: %s\n", chan_num, error, lasterr, dev_name );
	}
}

int casxmit_hdlr() {
    
    int chdev = sr_getevtdev();
    int channum = get_channum(chdev);
    if ( channum == -1) return 0; // FAKE channel?! ICCKK!!!!
    dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON);
    // Do permanent signal annoucement here
    //playtone_cad( channum, 480, 0, -1 );
    dxinfox[channum].state = ST_PERMANNC;
    dxinfox[ channum ].msg_fd = open( "sounds/error/psrec.pcm", O_RDONLY );
    play( channum, dxinfox[ channum ].msg_fd, 0x81, 0 );
    return 0;
    
}

/***************************************************************************
 *        NAME: void sys_quit()
 * DESCRIPTION: Handler called when one of the following signals is
 *		received: SIGHUP, SIGINT, SIGQUIT, SIGTERM.
 *		This function stops I/O activity on all channels and
 *		closes all the channels.
 *       INPUT: None
 *      OUTPUT: None.
 *     RETURNS: None.
 *    CAUTIONS: None.
 ***************************************************************************/
void sys_quit()
{
   int channum;
   long hEvent;  

   disp_msg( "Stopping Channel(s)...." );

   /*
    * Close all the channels opened after stopping all I/O activity.
    * It is okay to stop the I/O on a channel as the program is
    * being terminated.
    */
    
	  /*
	   * Disabling Voice Event Handlers
	   */
	  
	  if (sr_dishdlr(EV_ANYDEV, TDX_CST, (EVTHDLRTYP) cst_hdlr)
                                                                    == -1 ) {
		 disp_status(channum, "Unable to disable the CST handler" );
		 exit(-1);
	  } // end if 

      if (sr_dishdlr(EV_ANYDEV, TDX_PLAY, (EVTHDLRTYP) play_hdlr)
                                                                    == -1 ) {
		disp_status(0, "Unable to disable the PLAY handler" );
		exit(-1);
		 
      }// end if 

      if (sr_dishdlr(EV_ANYDEV, TDX_RECORD, (EVTHDLRTYP) record_hdlr)
                                                                    == -1 ) {
		 disp_status(0, "Unable to disable the RECORD handler" );
		 exit(-1);

		 
      } // end if 

      if (sr_dishdlr(EV_ANYDEV, TDX_GETDIG, (EVTHDLRTYP) getdig_hdlr)
                                                                    == -1 ) {
		 disp_status(0, "Unable to disable the GETDIG handler" );
		 exit(-1);		 
      } // end if 

	  if (sr_dishdlr(EV_ANYDEV, TDX_PLAYTONE, (EVTHDLRTYP) playtone_hdlr)
                                                                    == -1 ) {
		 disp_status(0, "Unable to disable the PLAYTONE handler" );
		 exit(-1);
	  } // end if 

	  if (sr_dishdlr(EV_ANYDEV, TDX_DIAL, (EVTHDLRTYP) dial_hdlr)
                                                                    == -1 ) {
		 disp_status(channum, "Unable to disable the DIAL handler" );
		 exit(-1);
	  } // end if 
	  if ( frontend != CT_NTTEST ) {
      if (sr_dishdlr(EV_ANYDEV ,TDX_SETHOOK, (EVTHDLRTYP) sethook_hdlr)
                                                                    == -1 ) {
		 disp_status(0, "Unable to disable the SETHOOK handler" );
		 exit(-1);
	  }
      } // end if 

      if (sr_dishdlr(EV_ANYDEV, TDX_ERROR, (EVTHDLRTYP) error_hdlr)
                                                                    == -1 ) {
		 disp_status(0, "Unable to disable the ERROR handler" );
		 exit(-1);		 
     } // end if 

   for ( channum = 1; channum <= maxchans; channum++ ) {
      set_hkstate( channum, DX_ONHOOK );
      if (frontend != CT_NTANALOG) {
          
          if (dt_castmgmt(dxinfox[channum].tsdev, &clearmsg, &clearreply) == -1) {
	          disp_err(channum, dxinfox[channum].chdev, "FUUUUUUUUUUUUUUCK");
          }
        
         /* Digital Frontend */
         /*
          * Unroute the digital timeslots from their resource channels.
          */
          
          
         if (scbus == TRUE) {

            nr_scunroute( dxinfox[ channum ].tsdev, SC_DTI,
                          dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP );
         }

		 /*
		  * Disable DTI Handlers
		  */
          
         if (sr_dishdlr(dxinfox[channum].tsdev, DTEV_CASSENDENDEVT, (EVTHDLRTYP) casxmit_hdlr) == -1 ) {
			disp_status( channum, "Unable to disable the DTI CAS transmission handler" );
			sys_quit();
         } // end if 

	if (sr_dishdlr(dxinfox[channum].tsdev, DTEV_SIG, (EVTHDLRTYP) sig_hdlr)
                                                                    == -1 ) {
			disp_status(channum, "Unable to disable the DTI signalling handler" );
			exit(-1);
         } // end if 

         if (sr_dishdlr(dxinfox[channum].tsdev, DTEV_ERREVT, (EVTHDLRTYP) dtierr_hdlr) == -1 ) {
			disp_status(channum, "Unable to disable the DTI error handler" );
			exit(-1);
         } // end if 


		 dt_close( dxinfox[ channum ].tsdev );
		 
      } else {
           //
           // Analog frontend, we just went onhook, so wait for 
           // the event
           sr_waitevtEx( (long int *) &dxinfox[channum].chdev, 1, 2000, &hEvent);
           // int, long int, same thing...
      }

	  // Attempting to avoid spurious ring events

	  if ( dx_setevtmsk( dxinfox[ channum ].chdev,0 ) == -1 ) {
		 sprintf( tmpbuff, "Cannot set CST events for %s",
			ATDV_NAMEP( dxinfox[ channum ].chdev ) );
		 disp_status(channum, tmpbuff );
		 sys_quit();
      } // end if 

	  	  
      dx_close( dxinfox[ channum ].chdev );
	  disp_status(channum,"Stopped");
   }

   if ( sr_dishdlr(EV_ANYDEV,EV_ANYEVT, (EVTHDLRTYP) fallback_hdlr) == -1 ){

	   disp_msg("Error disabling the fallback handler");
	   exit(-1);
   }
  
}


/***************************************************************************
 *        NAME: int get_channum( chtsdev )
 * DESCRIPTION: Get the index into the dxinfox[] for the channel or timeslot
 *		device descriptor, chtsdev.
 *       INPUT: int chtsdev;	- Channel/Timeslot Device Descriptor
 *      OUTPUT: None.
 *     RETURNS: Returns the index into dxinfox[]
 *    CAUTIONS: None.
 ***************************************************************************/
int get_channum( int chtsdev )
{
   int channum = 1;

   while ( channum <= MAXCHANS ) {
      if ( ( dxinfox[ channum ].chdev == chtsdev ) ||
           ( dxinfox[ channum ].tsdev == chtsdev ) ) {
	 return( channum );
      } else {
	 channum++;
      }
   }

   /*
    * Not Found in the Array, print error and return -1
    */

   sprintf( tmpbuff, "Unknown Event for Device %d - Ignored ", chtsdev );
   disp_status(channum, tmpbuff );

   return( -1 );
}


/***************************************************************************
 *        NAME: int play( channum, filedesc, format, offset )
 * DESCRIPTION: Set up IOTT and TPT's and Initiate the Play-Back
 *       INPUT: int channum;	- Index into dxinfox structure
 *		int filedesc;	- File Descriptor of VOX file to Play-Back
 *      int format; 0 for 6khz/ADPCM, 1 for 8khz/mu-law. This should be more.
 *      unsigned long offset; number of bytes to advance in the file before
 *      playback.
 *      OUTPUT: Starts the play-back
 *     RETURNS: -1 = Error
 *		 0 = Success
 *    CAUTIONS: None
 ***************************************************************************/
int play( int channum, int filedesc, int format, unsigned long offset )
{
   int		errcode;
   DV_TPT	tpt[ 2 ];
   short playmode = EV_ASYNC;

   /*
    * Rewind the file
   sprintf( tmpbuff, "Seeking to %ld", offset);
   disp_msg(tmpbuff);
    */
    
    if (filedesc == -1) {
        if ( ( filedesc = open( ERROR_VOX, O_RDONLY ) ) == -1 ) return -1;
        else {
            dxinfox[channum].msg_fd = filedesc; // Workaround so we aren't trying to close file descriptor -1
            format = 0x80;
        }
    }
    
   if ( lseek( filedesc, 0, SEEK_SET ) == -1 ) {
	   /*
      sprintf( tmpbuff, "Cannot seek to the beginning of the VOX file",
		ATDV_NAMEP( dxinfox[ channum ].chdev ) );
		*/
      disp_status(channum, "Cannot seek to the beginning of the VOX file" );
   }

   /*
    * Clear and Set-Up the IOTT strcuture
    */
   memset( dxinfox[ channum ].iott, 0, sizeof( DX_IOTT ) );

   dxinfox[ channum ].iott[ 0 ].io_type = IO_DEV | IO_EOT;
   dxinfox[ channum ].iott[ 0 ].io_fhandle = filedesc;
   dxinfox[ channum ].iott[ 0 ].io_length = -1;
   dxinfox[ channum ].iott[ 0 ].io_offset = offset;

   /*
    * Clear and then Set the DV_TPT structures
    */
   memset( tpt, 0, (sizeof( DV_TPT ) * 2) );

   /* Terminate Play on Loop Current Drop */
   if (format & 0x80) tpt[ 0 ].tp_type = IO_EOT;
   else {
       tpt[ 0 ].tp_type = IO_CONT;
       
       /* Terminate Play on Receiving any DTMF tone */
       
       tpt[ 1 ].tp_type = IO_EOT;
       tpt[ 1 ].tp_termno = DX_MAXDTMF;
       tpt[ 1 ].tp_length = 1;
       tpt[ 1 ].tp_flags = TF_MAXDTMF;
       
   }
   tpt[ 0 ].tp_termno = DX_LCOFF;
   tpt[ 0 ].tp_length = 1;
   tpt[ 0 ].tp_flags = TF_LCOFF;
   

   /*
    * Play VOX File on D/4x Channel, Normal Play Back
    */
   if (format & 0x1) playmode |= MD_PCM | PM_SR8;
   if ( ( errcode = dx_play( dxinfox[ channum ].chdev, dxinfox[ channum ].iott,
			tpt, playmode ) ) == -1 ) {
     disp_err(channum,dxinfox[ channum ].chdev,"PLAY");
   }

   return( errcode );
}

/***************************************************************************
 *        NAME: char countup()
 * DESCRIPTION: Simple function that counts upward by a single digit and
 *              performs carrying within a string.
 *       INPUT: numstring (pointer to string)
 *      OUTPUT: None.
 *     RETURNS: TRUE on success, FALSE on failure.
 *    CAUTIONS: None.
 ***************************************************************************/
 // I'd LIKE to have this return a bool. That's not happening, though. Fucking C89.
 char countup( char *numstring )
 {
     char strlength;
     strlength = strlen( numstring );
     if (strlength < 5 ) { // For testing
         disp_msg( "countup() failed!");
         return(-1);
     }
     
    if (numstring[(strlength - 1)] < 0x39 ) numstring[(strlength - 1)]++;
   
   else {
       
       if (numstring[(strlength - 2)] < 0x39 ) {
           numstring[(strlength - 2)]++;
           numstring[(strlength - 1)] = 0x30;
       }
       
       else {
           if ( numstring[(strlength - 3)] < 0x39 ) {
                numstring[(strlength - 3)]++;
                numstring[(strlength - 2)] = 0x30;
                numstring[(strlength - 1)] = 0x30;
           }
     
           else {
               if ( numstring[(strlength - 4)] < 0x39 ) {
                    numstring[(strlength - 4)]++;
                    numstring[(strlength - 3)] = 0x30;
                    numstring[(strlength - 2)] = 0x30;
                    numstring[(strlength - 1)] = 0x30;
                }
                
               else {
                   if ( numstring[(strlength - 5)] < 0x39 ) {
                        numstring[(strlength - 5)]++;
                        numstring[(strlength - 4)] = 0x30;
                        numstring[(strlength - 3)] = 0x30;
                        numstring[(strlength - 2)] = 0x30;
                        numstring[(strlength - 1)] = 0x30;
                    }
                    
                    else {
                        if ( numstring[(strlength - 6)] < 0x39 ) {
                             numstring[(strlength - 6)]++;
                             numstring[(strlength - 5)] = 0x30;
                             numstring[(strlength - 4)] = 0x30;
                             numstring[(strlength - 3)] = 0x30;
                             numstring[(strlength - 2)] = 0x30;
                             numstring[(strlength - 1)] = 0x30;
                        }
                        
                        else {
                            if ( numstring[(strlength - 7)] < 0x39 ) {
                                 numstring[(strlength - 7)]++;
                                 numstring[(strlength - 6)] = 0x30;
                                 numstring[(strlength - 5)] = 0x30;
                                 numstring[(strlength - 4)] = 0x30;
                                 numstring[(strlength - 3)] = 0x30;
                                 numstring[(strlength - 2)] = 0x30;
                                 numstring[(strlength - 1)] = 0x30;
                                }
                            else {
                                if ( numstring[(strlength - 8)] < 0x39 ) {
                                     numstring[(strlength - 8)]++;
                                     numstring[(strlength - 7)] = 0x30;
                                     numstring[(strlength - 6)] = 0x30;
                                     numstring[(strlength - 5)] = 0x30;
                                     numstring[(strlength - 4)] = 0x30;
                                     numstring[(strlength - 3)] = 0x30;
                                     numstring[(strlength - 2)] = 0x30;
                                     numstring[(strlength - 1)] = 0x30;
                                    }
                                }
                            }
                        }
                    }
     
                }
        
            }
   
        }
    
    return( 0 );
    
 }


/***************************************************************************
 *        NAME: int record( channum, filedesc )
 * DESCRIPTION: Set up IOTT and TPT's and Initiate the record
 *       INPUT: int channum;	- Index into dxinfox structure
 *		int filedesc;	- File Descriptor of VOX file to Record to
 *      OUTPUT: Starts the Recording
 *     RETURNS: -1 = Error
 *		 0 = Success
 *    CAUTIONS: None
 ***************************************************************************/
int record( int channum, int filedesc )
{
   int		errcode;
   DV_TPT	tpt[ 4 ];

   /*
    * Clear and Set-Up the IOTT strcuture
    */
   memset( dxinfox[ channum ].iott, 0, sizeof( DX_IOTT ) );

   dxinfox[ channum ].iott[ 0 ].io_type = IO_DEV | IO_EOT;
   dxinfox[ channum ].iott[ 0 ].io_fhandle = filedesc;
   dxinfox[ channum ].iott[ 0 ].io_length = -1;

   /*
    * Clear and then Set the DV_TPT structures
    */
   memset( tpt, 0, (sizeof( DV_TPT ) * 4) );

   /* Terminate Record on Receiving any DTMF tone */
   tpt[ 0 ].tp_type = IO_CONT;
   tpt[ 0 ].tp_termno = DX_MAXDTMF;
   tpt[ 0 ].tp_length = 1;
   tpt[ 0 ].tp_flags = TF_MAXDTMF;

   /* Terminate Record on Loop Current Drop */
   tpt[ 1 ].tp_type = IO_CONT;
   tpt[ 1 ].tp_termno = DX_LCOFF;
   tpt[ 1 ].tp_length = 1;
   tpt[ 1 ].tp_flags = TF_LCOFF;

   /* Terminate Record on 5 Seconds of Silence */
   tpt[ 2 ].tp_type = IO_CONT;
   tpt[ 2 ].tp_termno = DX_MAXSIL;
   tpt[ 2 ].tp_length = 50;
   tpt[ 2 ].tp_flags = TF_MAXSIL;

   /* Terminate Record After 10 Seconds of Recording */
   tpt[ 3 ].tp_type = IO_EOT;
   tpt[ 3 ].tp_termno = DX_MAXTIME;
   tpt[ 3 ].tp_length = 100;
   tpt[ 3 ].tp_flags = TF_MAXTIME;

   /*
    * Record VOX File on D/4x Channel
    */
   if ( ( errcode = dx_rec( dxinfox[ channum ].chdev, dxinfox[ channum ].iott,
			tpt, RM_TONE | EV_ASYNC ) ) == -1 ) {
       disp_err(channum, dxinfox[ channum ].chdev,"RECORD");
   }

   return( errcode );
}

/***************************************************************************
*       NAME: int send_bell202
*       DESCRIPTION: Sends an ADSI-formatted file using Bell 202 FSK
*       INPUT: channum - channel number
*              filedesc - File descriptor of data to be sent
*       OUTPUT: BYAH-TIFUL EFF ESS KAY!!!!!1111
*       RETURNS: -1 on error, 0 on success
*
***************************************************************************/
int send_bell202( int channum, int filedesc )

{

DV_TPT tpt[1];
/*
if (dx_clrtpt(&tpt[1], 1) == -1 ) {
    disp_msg("Couldn't clear TPT for ADSI sender!");
}
*/

DX_IOTT iott[1];
ADSI_XFERSTRUC adsimode;
memset( tpt, 0, (sizeof( DV_TPT ) ));

tpt[0].tp_type = IO_EOT;
tpt[0].tp_termno = DX_MAXTIME;
tpt[0].tp_length = 2400;
tpt[0].tp_flags = TF_MAXTIME;

iott[0].io_fhandle = filedesc;
iott[0].io_type = IO_DEV|IO_EOT;
iott[0].io_bufp = 0;
iott[0].io_offset = 0;
iott[0].io_length = -1;

adsimode.cbSize = sizeof(adsimode);
adsimode.dwTxDataMode = ADSI_ALERT; // Set to ADSI_NOALERT for no CAS tone

if (dx_TxIottData( dxinfox[ channum ].chdev, iott, tpt, DT_ADSI, &adsimode, EV_ASYNC ) < 0 )
{
    disp_msg("dx_TxIottData returned an error!");
    disp_err(channum, dxinfox[ channum ].chdev,"SEND FSK");
    return(-1);
}

return(0);

}

/***************************************************************************
*       NAME: int digread
*       DESCRIPTION: Plays back a string of numbers. This is both to
*       encourage CPU efficiency and to stop DM3 cards from poking around
*       at the speed of molasses.
*       INPUT: channum - Channel number
*            digstring - String of digits to play
*       OUTPUT: Your digits - just add water.
*       RETURNS: -1 on WHAT THA FAHCK!?, number of FDs to close on success.
*
***************************************************************************/
int digread( int channum, char * digstring )
{
	DV_TPT  tpt_table[1];
	DX_XPB  xpb_table[21];
	DX_IOTT dig_table[21];
    unsigned char numdigs; // Is a maximum of 255 digits going to be a problem? I hope not...
    unsigned char numdigs2; // I'd rather do this with just one variable, but I think we need two to close all the FDs
	char digfile[21]; // Keep in mind, we only allocated enough characters for the filename in the sprintf below.
    numdigs = strlen(digstring);
	numdigs2 = numdigs;
	
    
    if ( ( numdigs == 0 ) || (numdigs > 20 ) ) { // What happens if strlen returns -1? That might get awkward.
        disp_msg("Could not perform digread function; empty string given");
        return(-1);
    }
    
    memset( dig_table, 0, (sizeof( DX_IOTT ) * 21 ) );
    memset( xpb_table, 0, (sizeof( DX_XPB ) * numdigs) );
    memset( tpt_table, 0, sizeof( DV_TPT ) );
       
    tpt_table[0].tp_type = IO_EOT;
    tpt_table[0].tp_termno = DX_LCOFF;
    tpt_table[0].tp_length = 1;
    tpt_table[0].tp_flags = TF_LCOFF;
    
    numdigs--; // We count from zero from here on out.
    
    // This is a little crude, but whatever. No extra resources were used.
    
    sprintf( digfile, "sounds/digits1/%c.pcm", digstring[numdigs] );
    file[numdigs] = open( digfile, O_RDONLY );
        
    dig_table[numdigs].io_type = IO_DEV | IO_EOT;
    dig_table[numdigs].io_fhandle = file[numdigs];
    dig_table[numdigs].io_offset = 0;
    dig_table[numdigs].io_length = -1;
        
    xpb_table[numdigs].wFileFormat = FILE_FORMAT_VOX;
    xpb_table[numdigs].wDataFormat = DATA_FORMAT_MULAW;
    xpb_table[numdigs].nSamplesPerSec = DRT_8KHZ;
    xpb_table[numdigs].wBitsPerSample = 8;
    
    numdigs--;
    
    while( numdigs != 255 ) { // The counter will eventually roll over.
        sprintf( digfile, "sounds/digits1/%c.pcm", digstring[numdigs] );
        file[numdigs] = open( digfile, O_RDONLY );
        
        dig_table[numdigs].io_type = IO_DEV;
        dig_table[numdigs].io_fhandle = file[numdigs];
        dig_table[numdigs].io_offset = 0;
        dig_table[numdigs].io_length = -1;
        
        xpb_table[numdigs].wFileFormat = FILE_FORMAT_VOX;
        xpb_table[numdigs].wDataFormat = DATA_FORMAT_MULAW;
        xpb_table[numdigs].nSamplesPerSec = DRT_8KHZ;
        xpb_table[numdigs].wBitsPerSample = 8;
        
        numdigs--;
        
    }
       if ( dx_playiottdata( dxinfox[ channum ].chdev, dxinfox[ channum ].iott, tpt_table, xpb_table, EV_ASYNC ) != -1 ) return(numdigs2);
       else return(-1);
    }
    
/***************************************************************************
 * NAME: int playtonerep ( chdev, toneval, toneval2, ontime, pausetime )
 * DESCRPTION: Plays a repeating tone.
 *
 * INPUT: int channum; - Channel number
 *	      toneval - Hertz value of tone
 *        toneval2 - Hertz value of second tone (use 0 for single tone)
 *        amp1 - Amplitude of tone 1 in decibels
 *        amp2 - Amplitude of tone 2 in decibels
 *        int ontime - Time to play the tone
 *        int offtime - Time to pause between tone repetitions
 * OUTPUT: None.
 * 
 * CAUTIONS: I dunno, radioactive?
 ***************************************************************************/
int playtone_warble( int channum, int toneval, int toneval2, int amp1, int amp2, int ontime, int cycles)
{
   int          errcode;
   DV_TPT       tpt[ 1 ];
   TN_GENCAD cadtone;
   cadtone.cycles = cycles;
   cadtone.numsegs = 2;
   cadtone.offtime[0] = 0;
   cadtone.offtime[1] = 0;
   dx_bldtngen( &cadtone.tone[0], toneval, 0, amp1, 0, ontime );
   dx_bldtngen( &cadtone.tone[1], toneval2, 0, amp2, 0, ontime );
   
   memset( tpt, 0, (sizeof( DV_TPT ) ) );

   /* Terminate Play on Loop Current Drop */
   tpt[ 0 ].tp_type = IO_EOT;
   tpt[ 0 ].tp_termno = DX_LCOFF;
   tpt[ 0 ].tp_length = 1;
   tpt[ 0 ].tp_flags = TF_LCOFF;

   //sprintf( tmpbuff, "playtone_rep invoked from state %d", dxinfox[ channum ].state );
   //disp_msg(tmpbuff);
   
   if ( ( errcode = dx_playtoneEx( dxinfox[ channum ].chdev, &cadtone, tpt, EV_ASYNC ) ) == -1 )

   {
      disp_err(channum, dxinfox[ channum ].chdev, "PLAYTONE_WARBLE" );
      disp_msg( "There's an error in the cadenced tone generator function. Or maybe a pidgeon." );

   }

   return( errcode );
}

/***************************************************************************
 * NAME: int playtonerep ( chdev, toneval, toneval2, ontime, pausetime )
 * DESCRPTION: Plays a repeating tone.
 *
 * INPUT: int channum; - Channel number
 *	      toneval - Hertz value of tone
 *        toneval2 - Hertz value of second tone (use 0 for single tone)
 *        amp1 - Amplitude of tone 1 in decibels
 *        amp2 - Amplitude of tone 2 in decibels
 *        int ontime - Time to play the tone
 *        int offtime - Time to pause between tone repetitions
 * OUTPUT: None.
 * 
 * CAUTIONS: I dunno, radioactive?
 ***************************************************************************/
int playtone_rep( int channum, int toneval, int toneval2, int amp1, int amp2, int ontime, int pausetime, int cycles)
{
   int          errcode;
   DV_TPT       tpt[ 1 ];
   TN_GENCAD cadtone;
   cadtone.cycles = cycles;
   cadtone.numsegs = 1;
   cadtone.offtime[0] = pausetime;
   dx_bldtngen( &cadtone.tone[0], toneval, toneval2, amp1, amp2, ontime );
   
   memset( tpt, 0, (sizeof( DV_TPT ) ) );

   /* Terminate Play on Loop Current Drop */
   tpt[ 0 ].tp_type = IO_EOT;
   tpt[ 0 ].tp_termno = DX_LCOFF;
   tpt[ 0 ].tp_length = 1;
   tpt[ 0 ].tp_flags = TF_LCOFF;

   //sprintf( tmpbuff, "playtone_rep invoked from state %d", dxinfox[ channum ].state );
   //disp_msg(tmpbuff);
   
   if ( ( errcode = dx_playtoneEx( dxinfox[ channum ].chdev, &cadtone, tpt, EV_ASYNC ) ) == -1 )

   {
      disp_err(channum, dxinfox[ channum ].chdev, "PLAYTONE_REP" );
      disp_msg( "There's an error in the cadenced tone generator function. Or maybe a pidgeon." );

   }

   return( errcode );
}

/***************************************************************************
 * NAME: int playtone_stutter ( chdev, toneval, toneval2, ontime, pausetime )
 * DESCRPTION: Plays a repeating tone.
 *
 * INPUT: int channum; - Channel number
 *	      toneval - Hertz value of tone
 *        toneval2 - Hertz value of second tone (use 0 for single tone)
 *        amp1 - Amplitude of tone 1 in decibels
 *        amp2 - Amplitude of tone 2 in decibels
 *        int ontime - Time to play the tone
 *        int offtime - Time to pause between tone repetitions
 * OUTPUT: None.
 * 
 * CAUTIONS: I dunno, radioactive?
 ***************************************************************************/
int playtone_stutter( int channum, int toneval, int toneval2, int amp1, int amp2, int ontime, int pausetime, int cycles)
{
   int          errcode;
   DV_TPT       tpt[ 2 ];
   TN_GENCAD seqtone;
   seqtone.cycles = cycles;
   seqtone.numsegs = 4;
   seqtone.offtime[0] = pausetime;
   seqtone.offtime[1] = pausetime;
   seqtone.offtime[2] = pausetime;
   dx_bldtngen( &seqtone.tone[0], toneval, toneval2, amp1, amp2, ontime );
   dx_bldtngen( &seqtone.tone[1], toneval, toneval2, amp1, amp2, ontime );
   dx_bldtngen( &seqtone.tone[2], toneval, toneval2, amp1, amp2, ontime );
   dx_bldtngen( &seqtone.tone[3], toneval, toneval2, amp1, amp2, 1900 );
   
   memset( tpt, 0, (sizeof( DV_TPT ) ) );

   /* Terminate Play on Loop Current Drop */
   /* Terminate Play on Receiving any DTMF tone */
   tpt[0].tp_type = IO_CONT;
   tpt[0].tp_termno = DX_MAXDTMF;
   tpt[0].tp_length = 1;
   tpt[0].tp_flags = TF_MAXDTMF;

   /* Terminate Play on Loop Current Drop */
   tpt[1].tp_type = IO_EOT;
   tpt[1].tp_termno = DX_LCOFF;
   tpt[1].tp_length = 1;
   tpt[1].tp_flags = TF_LCOFF;

   if ( ( errcode = dx_playtoneEx( dxinfox[ channum ].chdev, &seqtone, tpt, EV_ASYNC ) ) == -1 )

   {
      disp_err(channum, dxinfox[ channum ].chdev, "PLAYTONE_STUTTER" );
      disp_msg( "There's an error in the stutter tone generator function. Or maybe a pidgeon." );

   }

   return( errcode );
}

/***************************************************************************
 * NAME: int playtone ( chdev, toneval, toneval2 )
 * DESCRPTION: Plays a single frequency tone. Eventually I'll make it
 * play dual tones or something more fun.
 * INPUT: int channum; - Channel number
 *	      toneval - Hertz value of tone
 *        toneval2 - Hertz value of second tone (use 0 for single tone)
 * OUTPUT: None.
 * 
 * CAUTIONS: I dunno, radioactive?
 ***************************************************************************/
int playtone_cad( int channum, int toneval, int toneval2, int time)
{
   int          errcode;
   DV_TPT       tpt[ 1 ];
   TN_GEN cadtone;
   dx_bldtngen( &cadtone, toneval, toneval2, -17, -20, time );

   memset( tpt, 0, (sizeof( DV_TPT ) ) );

   /* Terminate Play on Loop Current Drop */
   tpt[ 0 ].tp_type = IO_EOT;
   tpt[ 0 ].tp_termno = DX_LCOFF;
   tpt[ 0 ].tp_length = 1;
   tpt[ 0 ].tp_flags = TF_LCOFF;
   
   if ( ( errcode = dx_playtone( dxinfox[ channum ].chdev, &cadtone, tpt, EV_ASYNC ) ) == -1 )

   {
      disp_err(channum, dxinfox[ channum ].state, "PLAYTONE_CAD" );
      disp_msg( "There's an error in the tone generator function. Or maybe a pidgeon." );

   }

   return( errcode );
}

int ringphone( int channum, unsigned char linedev )
{
	// This seems excessive. Please make it more efficient, RE: = 0.
	connchan[channum] = linedev;
	// Basic translation table for the moment. This will be a config file at some point.
    /*
	if (strcmp("18", callednum ) == 0 ) connchan[channum] = 1; // These're swapped for backwards compatibility reasons with the shit we tell people.
	if (strcmp("10", callednum ) == 0 ) connchan[channum] = 2;
	if (strcmp("12", callednum ) == 0 ) connchan[channum] = 3;
	if (strcmp("13", callednum ) == 0 ) connchan[channum] = 4;
	if (strcmp("14", callednum ) == 0 ) connchan[channum] = 5;
	if (strcmp("15", callednum ) == 0 ) connchan[channum] = 6;
	if (strcmp("16", callednum ) == 0 ) connchan[channum] = 7;
	if (strcmp("17", callednum ) == 0 ) connchan[channum] = 8;
	if (strcmp("11", callednum ) == 0 ) connchan[channum] = 9;
	if (strcmp("19", callednum ) == 0 ) connchan[channum] = 10;
	if (strcmp("20", callednum ) == 0 ) connchan[channum] = 11;
	if (strcmp("21", callednum ) == 0 ) connchan[channum] = 12;
	if (strcmp("22", callednum ) == 0 ) connchan[channum] = 13;
	if (strcmp("23", callednum ) == 0 ) connchan[channum] = 14;
	if (strcmp("24", callednum ) == 0 ) connchan[channum] = 15;
	if (strcmp("25", callednum ) == 0 ) connchan[channum] = 16;
	if (strcmp("26", callednum ) == 0 ) connchan[channum] = 17;
	if (strcmp("27", callednum ) == 0 ) connchan[channum] = 18;
	if (strcmp("28", callednum ) == 0 ) connchan[channum] = 19;
	if (strcmp("29", callednum ) == 0 ) connchan[channum] = 20;
	if (strcmp("30", callednum ) == 0 ) connchan[channum] = 21;
	if (strcmp("31", callednum ) == 0 ) connchan[channum] = 22;
	if (strcmp("32", callednum ) == 0 ) connchan[channum] = 23;
	if (strcmp("33", callednum ) == 0 ) connchan[channum] = 24;
    */
    
	// This seems excessive. Please make it more efficient, RE: check for 0.
	if (connchan[channum] == 0 ) {
        // Placeholder code
		playtone_rep( channum, 480, 620, -25, -27, 25, 25, 40 );
	    dxinfox[ channum ].state = ST_REORDER;
	    return(0);
    }

	/*
	if ( connchan[channum] == channum ) {
		// Considering the busy test below, this is excessive.
        // The originating and terminating line numbers are the same. Play a busy tone.
        playtone_rep( channum, 480, 620, -24, -26, 50, 50, 40 );
	    dxinfox[ channum ].state = ST_BUSY;
	    return(0);
	}
	*/
	if (fxo[connchan[channum]] == 0x01) {
		sprintf(tmpbuff, "DEBUG: FXO signaling bits are 0x%lx", ATDT_TSSGBIT(dxinfox[channum].tsdev));
		disp_msg(tmpbuff);
		if ( ((ATDT_TSSGBIT(dxinfox[connchan[channum]].tsdev) & 0xF0) == 0xA0) && dxinfox[connchan[channum]].state == ST_WTRING) { // ESF/B8ZS
		// if ( (ATDT_TSSGBIT(dxinfox[connchan[channum]].tsdev) & 0x22) && dxinfox[connchan[channum]].state == ST_WTRING) { // SF/B8ZS
		// This presents a problem, since sometimes on incoming traffic, signaling bits return 0x22 (SF/B8ZS) from the channel bank
		// even when the unit is off-hook. This kinda sucks. Until this can be prevented in some way, we're doing a state
		// check here, so you don't just wind up taking over an in-progress call.
		connchan[connchan[channum]] = channum; // Indicate on the calling line that it'll be connecting to us.
		dxinfox[connchan[channum]].state = ST_FXOOUT;
		if (nr_scunroute(dxinfox[channum].chdev, SC_VOX, dxinfox[channum].tsdev, SC_DTI, SC_FULLDUP) != 0) {
			disp_msg("GAH! SCBUS UNROUTING FUNCTION FAILED ON OUTGOING CALL!! BAIL OOOOUUUT!!!1");
			// The best thing we can really do here is... uhh, well, we should do something later.
			dxinfox[channum].state = ST_WTRING;
			dxinfox[connchan[channum]].state = ST_WTRING;
			return(-1);
		}

		if (nr_scunroute(dxinfox[connchan[channum]].chdev, SC_VOX, dxinfox[connchan[channum]].tsdev, SC_DTI, SC_FULLDUP) != 0) {
			// This is failing. Let's try and route these channels back to the SCBus devices and act as if nothing happened. Least we can do, right?
			nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP);
			nr_scroute(dxinfox[connchan[channum]].tsdev, SC_DTI, dxinfox[connchan[channum]].chdev, SC_VOX, SC_FULLDUP);
			dxinfox[channum].state = ST_WTRING;
			dxinfox[connchan[channum]].state = ST_WTRING;
			return(-1);
		}

		if (nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[connchan[channum]].tsdev, SC_DTI, SC_FULLDUP) != 0) {
			// This is failing. Let's try and route these channels back to the SCBus devices and act as if nothing happened. Least we can do, right?
			nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP);
			nr_scroute(dxinfox[connchan[channum]].tsdev, SC_DTI, dxinfox[connchan[channum]].chdev, SC_VOX, SC_FULLDUP);
			dxinfox[channum].state = ST_WTRING;
			dxinfox[connchan[channum]].state = ST_WTRING;
			return(-1);
		}
		dxinfox[channum].state = ST_FXOOUT_S; // Update originating channel state to reflect outdial
		//dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BOFF); // Sets channel offhook (SF/B8ZS)
		dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BOFF | DTB_CON | DTB_DOFF); // Sets channel offhook (ESF/B8ZS)
        dt_settssigsim(dxinfox[channum].tsdev, DTB_AON | DTB_BON | DTB_COFF | DTB_DON); // (Polarity reversal for originating channel, E&M hack)
		return(0);
	    }

		else {
            connchan[channum] = 0; // Bugfix for incoming ISDN
			sprintf(tmpbuff, "Trunk busy - state of called trunk is %d, tsbits are %li", dxinfox[connchan[channum]].state, ATDT_TSSGBIT(dxinfox[connchan[channum]].tsdev));
			disp_msg(tmpbuff);
			playtone_rep(channum, 480, 620, -25, -27, 50, 50, 40);
			dxinfox[channum].state = ST_BUSY;
			return(0);
		}
	}

	if ( !( ATDT_TSSGBIT( dxinfox[ connchan[channum] ].tsdev ) & 0xF0 ) && ( dxinfox[ connchan[ channum] ].state == ST_WTRING ) ) { // E&M hack
		connchan[ connchan[channum] ] = channum; // Indicate on the calling line that it'll be connecting to us.
		dxinfox[ connchan[channum] ].state = ST_INCOMING;
		ownies[channum ] = 2;
		//dt_settssigsim( dxinfox[ connchan[channum] ].tsdev, DTB_AOFF | DTB_BOFF ); // (SF/B8ZS)
		dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BON | DTB_COFF | DTB_DON); // (Ringing, E&M hack)
		playtone_cad( channum, 440, 480, 200 );
		return(0);
		}

	else {
        connchan[channum] = 0; // Bugfix for incoming ISDN
		sprintf( tmpbuff, "Line busy - state of called line is %d", dxinfox[ connchan[ channum ] ].state );
		disp_msg(tmpbuff);
        playtone_rep( channum, 480, 620, -25, -27, 50, 50, 40 );
	    dxinfox[ channum ].state = ST_BUSY;
	    return(0);
	}
}

/***************************************************************************
 * NAME: int playtone ( chdev, toneval, toneval2 )
 * DESCRPTION: Plays a single frequency tone. Eventually I'll make it
 * play dual tones or something more fun.
 * INPUT: int channum; - Channel number
 *	      toneval - Hertz value of tone
 *        toneval2 - Hertz value of second tone (use 0 for single tone)
 * OUTPUT: None.
 * 
 * CAUTIONS: I dunno, radioactive?
 ***************************************************************************/
int playtone( int channum, int toneval, int toneval2, int duration)
{
   int          errcode;
   DV_TPT       tpt[ 2 ];
   TN_GEN singletone;
   dx_bldtngen( &singletone, toneval, toneval2, -11, -14, duration );

   memset( tpt, 0, (sizeof( DV_TPT ) * 2) );

   /* Terminate Play on Receiving any DTMF tone */
   tpt[ 0 ].tp_type = IO_CONT;
   tpt[ 0 ].tp_termno = DX_MAXDTMF;
   tpt[ 0 ].tp_length = 1;
   tpt[ 0 ].tp_flags = TF_MAXDTMF;

   /* Terminate Play on Loop Current Drop */
   tpt[ 1 ].tp_type = IO_EOT;
   tpt[ 1 ].tp_termno = DX_LCOFF;
   tpt[ 1 ].tp_length = 1;
   tpt[ 1 ].tp_flags = TF_LCOFF;
   
   if ( ( errcode = dx_playtone( dxinfox[ channum ].chdev, &singletone, tpt, EV_ASYNC ) ) == -1 )

   {
      disp_msg( "There's an error in the tone generator function. Or maybe a pidgeon." );

   }

   return( errcode );
}



/***************************************************************************
 *        NAME: int get_digits( channum, digbufp )
 * DESCRIPTION: Set up TPT's and Initiate get-digits function
 *       INPUT: int channum;		- Index into dxinfox structure
 *		DV_DIGIT *digbufp;	- Pointer to Digit Buffer
 *      OUTPUT: Starts to get the DTMF Digits
 *     RETURNS: -1 = Error
 *		 0 = Success
 *    CAUTIONS: None
 ***************************************************************************/
int get_digits( int channum, DV_DIGIT * digbufp, unsigned char numdigs )
{
   int		errcode;
   DV_TPT	tpt[ 3 ];

   /*
    * Clear and then Set the DV_TPT structures
    */
   memset( tpt, 0, (sizeof( DV_TPT ) * 3) );

   /* Terminate GetDigits on Receiving MAXDTMF Digits */
   tpt[ 0 ].tp_type = IO_CONT;
   tpt[ 0 ].tp_termno = DX_MAXDTMF;
   tpt[ 0 ].tp_length = numdigs;
   tpt[ 0 ].tp_flags = TF_MAXDTMF;


   /* Terminate GetDigits on Loop Current Drop */
   tpt[ 1 ].tp_type = IO_CONT;
   tpt[ 1 ].tp_termno = DX_LCOFF;
   tpt[ 1 ].tp_length = 1;
   tpt[ 1 ].tp_flags = TF_LCOFF;

   /* Terminate GetDigits after 5 Seconds */
   tpt[ 2 ].tp_type = IO_EOT;
   tpt[ 2 ].tp_termno = DX_MAXTIME;
   tpt[ 2 ].tp_length = 50;
   tpt[ 2 ].tp_flags = TF_MAXTIME;

   if ( ( errcode = dx_getdig( dxinfox[ channum ].chdev, tpt, digbufp,
				EV_ASYNC ) ) == -1 ) {
         disp_err(channum, dxinfox[ channum ].chdev,"GET DIGITS");
   }

   return( errcode );
}


/***************************************************************************
 *        NAME: int set_hkstate( channum, state )
 * DESCRIPTION: Set the channel to the appropriate hook status
 *       INPUT: int channum;		- Index into dxinfox structure
 *		int state;		- State to set channel to
 *      OUTPUT: None.
 *     RETURNS: -1 = Error
 *		 0 = Success
 *    CAUTIONS: None.
 ***************************************************************************/
int set_hkstate( int channum, int state )
{
   int chdev = dxinfox[ channum ].chdev;
   int tsdev = dxinfox[ channum ].tsdev;

   /*
    * Make sure you are in CS_IDLE state before setting the
    * hook status
    */
   if ( ATDX_STATE( chdev ) != CS_IDLE ) {
      dx_stopch( chdev, EV_ASYNC );
      while ( ATDX_STATE( chdev ) != CS_IDLE );
   }

   switch ( frontend ) {
   case CT_NTANALOG:
      if ( dx_sethook( chdev, (state == DX_ONHOOK) ? DX_ONHOOK : DX_OFFHOOK,
		                                         EV_ASYNC ) == -1 ) {
		 disp_err(channum, dxinfox[ channum ].chdev,"SETHOOK");
         sprintf( tmpbuff, "Cannot set channel %s to %s-Hook (%s)",
	            ATDV_NAMEP( chdev ), (state == DX_ONHOOK) ? "On" : "Off",
	                                             ATDV_ERRMSGP( chdev ) );
         disp_status(channum, tmpbuff );
         return( -1 );
      }
      break;

   case CT_NTTEST:
	  if (dt_settssigsim(tsdev, (state == DX_ONHOOK) ? DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON : // E&M bugfix
		                                          DTB_AON | DTB_BON | DTB_COFF | DTB_DON ) == -1) {
		  /*
      if ( dt_settssigsim( tsdev, (state == DX_ONHOOK) ? DTB_AOFF | DTB_BON : // SF/B8ZS
                                                DTB_AON | DTB_BON ) == -1 ) {
		  */
		 disp_err(channum, dxinfox[ channum ].chdev,"SET SIGSTATE");
         sprintf( tmpbuff, "Cannot set bits to %s on %s (%s)",
          (state == DX_ONHOOK) ? "AOFF-BOFF" : "AON-BON", ATDV_NAMEP( tsdev ),
	                                             ATDV_ERRMSGP( tsdev ) );
         disp_status(channum, tmpbuff );
         return( -1 );
      }
      break;
   case CT_NTT1:
      if ( dt_settssigsim( tsdev, (state == DX_ONHOOK) ? DTB_AOFF | DTB_BOFF :
                                                DTB_AON | DTB_BON ) == -1 ) {
		 disp_err(channum, dxinfox[ channum ].chdev,"SET SIGSTATE");
         sprintf( tmpbuff, "Cannot set bits to %s on %s (%s)",
          (state == DX_ONHOOK) ? "AOFF-BOFF" : "AON-BON", ATDV_NAMEP( tsdev ),
	                                             ATDV_ERRMSGP( tsdev ) );
         disp_status(channum, tmpbuff );
         return( -1 );
      }
      break;

   case CT_NTE1:
      if ( dt_settssigsim( tsdev, (state == DX_ONHOOK) ? DTB_AON : DTB_AOFF )
                                                                    == -1 ) {
		 disp_err(channum, dxinfox[ channum ].chdev,"SET SIG STATE");
         sprintf( tmpbuff, "Cannot set bits to %s on %s (%s)",
              (state == DX_ONHOOK) ? "AON-BOFF" : "AOFF",ATDV_NAMEP( tsdev ), 
	                                             ATDV_ERRMSGP( tsdev ) );
         disp_status(channum, tmpbuff );
         return( -1 );
      }
   }

   if (frontend != CT_NTANALOG) {
      switch ( state ) {
      case DX_ONHOOK:
         dxinfox[ channum ].state = ST_WTRING;

         if ( dx_clrdigbuf( chdev ) == -1 ) {
	    sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			ATDV_NAMEP( chdev ) );
	    disp_status(channum, tmpbuff );
		disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
         }

		 sprintf(tmpbuff,"Ready to Accept call - Access number %s - Set Hkstate", dxinfox[ channum ].ac_code );
		 disp_status( channum, tmpbuff);
         
         break;

      case DX_OFFHOOK:
         dxinfox[ channum ].state = ST_INTRO;

         if ( play( channum, introfd, 0, 0 ) == -1 ) {
	    sprintf( tmpbuff, "Error playing Introduction on channel %s",
						ATDV_NAMEP( chdev ) );
	    disp_status(channum, tmpbuff );
         }
         break;
      }
   }

   return( 0 );
}

/***************************************************************************
 *        NAME: int cst_hdlr()
 * DESCRIPTION: TDX_CST event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ***************************************************************************/
int cst_hdlr()
{
   int		chdev = sr_getevtdev();
   int		channum = get_channum( chdev );
   int		curstate;
   DX_CST	*cstp;

   if ( channum == -1 ) {
      return( 0 );		/* Discard Message - Not for a Known Device */
   }

   curstate = dxinfox[ channum ].state;		/* Current State */
   cstp = (DX_CST *) sr_getevtdatap();

   switch ( cstp->cst_event ) {
   case DE_RINGS:		/* Incoming Rings Event */
      if ( curstate == ST_WTRING ) {
	 /*
	  * Set Channel to Off-Hook
	  */
	 dxinfox[ channum ].state = ST_OFFHOOK;
	 set_hkstate( channum, DX_OFFHOOK );
     
     // The life of a call starts here, goes to sethook_hdlr

	disp_status(channum, "Incoming Call" );
      }
      break;

   case DE_DIGITS:
	   switch ( dxinfox[ channum ].state )
	   {
	   case ST_RINGPHONE1:
	   case ST_BUSY:
       case ST_WARBLE:
	   case ST_REORDER:
		   return(0);

	   case ST_FXSTEST1:
		   /*
		   sprintf( tmpbuff, "Digit CST Event (%d) Received on %s, data is %d", cstp->cst_event, ATDV_NAMEP( chdev ), cstp->cst_data );
           disp_msg( tmpbuff ); 
		   */
		   if (cstp->cst_data > 0x230) {
			   sprintf(tmpbuff, "Weird digit received, ignoring (%x) - check LPD", cstp->cst_data);
			   disp_msg(tmpbuff);
			   return(0);
		   }
		   dxinfox[ channum ].digbuf.dg_value[ dignum[channum] ] = cstp->cst_data;
		   dignum[channum]++;

		   switch( dignum[channum] ) {
		   case 1:

               if (cstp->cst_data == 0x38 ) {
                   dxinfox[ channum ].state = ST_ISDNOUT2;
                   if ( dx_clrdigbuf( chdev ) == -1 ) {
	                    sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			            ATDV_NAMEP( chdev ) );
	                    disp_status(channum, tmpbuff );
		                disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                    }
                   if (ATDX_STATE(chdev) == CS_IDLE) {
                       dxinfox[channum].state--;
        		       if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_RINGS | DM_DIGOFF ) == -1 ) {
		                   sprintf( tmpbuff, "Cannot set CST events for %s, error %s",
			                  ATDV_NAMEP( dxinfox[ channum ].chdev ), ATDV_ERRMSGP( dxinfox[ channum ].chdev ) );
		                   disp_status(channum, tmpbuff );
                           return(-1);
                        }
				       playtone( channum, 360, 450, 2000 );
                   }
                   else dx_stopch( dxinfox[channum].chdev, (EV_ASYNC | EV_NOSTOP) );
                   return(0);
               }
               
			   if (cstp->cst_data == 0x31 ) {
				   // No extensions beginning with 1 anymore
				   if ( ATDX_STATE( chdev ) == CS_IDLE ) playtone_rep( channum, 480, 620, -25, -27, 25, 25, 40 );
				   else dx_stopch(chdev, EV_ASYNC);
				   dxinfox[ channum ].state = ST_REORDER;
				   return(0);
			   }
               
			   if (cstp->cst_data == 0x32 ) {
				   // 2 either.
				   if ( ATDX_STATE( chdev ) == CS_IDLE ) playtone_warble(channum, 490, 325, -17, -17, 25, 40);
				   else dx_stopch(chdev, EV_ASYNC);
				   dxinfox[ channum ].state = ST_WARBLE;
				   return(0);
			   }
               if ( (cstp->cst_data == 0x33) || (cstp->cst_data == 0x34) ) {
                   /*
	               if ( ATDX_STATE( chdev ) == CS_IDLE ) if (get_digits(channum, &(dxinfox[channum].digbuf), 1) == -1) {
		              sprintf(tmpbuff, "Cannot get digits from channel %s",
			              ATDV_NAMEP(chdev));
		              disp_status(channum, tmpbuff);
	                }
                    else {
                        dxinfox[channum].state = ST_RETURNDIGS;
                        dx_stopch(chdev, EV_ASYNC | EV_NOSTOP);
                    }
                    */
                    return 0;
               }
               
               if (cstp->cst_data == 0x39) {
                   /*
	               if ( ATDX_STATE( chdev ) == CS_IDLE ) if (get_digits(channum, &(dxinfox[channum].digbuf), 1) == -1) {
		              sprintf(tmpbuff, "Cannot get digits from channel %s",
			              ATDV_NAMEP(chdev));
		              disp_status(channum, tmpbuff);
	                }
                    else {
                        dxinfox[channum].state = ST_RETURNDIGS;
                        dx_stopch(chdev, EV_ASYNC | EV_NOSTOP);
                    }
                    */
                   return(0);
               }
                 
			   if ( cstp->cst_data > 0x34 ) {
				   if ( ATDX_STATE( chdev ) == CS_IDLE ) playtone_warble(channum, 490, 325, -17, -17, 25, 40);
				   else dx_stopch(chdev, EV_ASYNC);
				   dxinfox[ channum ].state = ST_WARBLE;
				   sprintf(tmpbuff, "Digit found on termination was %d", cstp->cst_data);
				   disp_msg(tmpbuff);
				   return(0);
			   }
               
			   if (cstp->cst_data == 0x30 ) {
				   // Should route to operator extension
                   dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
                   if ( channum < 25 ) {
                       isdn_origtest(channum, operator, dntable[channum] );
                   }
                   else {
                       isdn_origtest(channum, operator, defaultcpn );
                   }
                   memset(dxinfox[channum].digbuf.dg_value, 0x00, DG_MAXDIGS);
                   return(0);
			   }

               /*
			   if (cstp->cst_data == 0x2A ) {
				   // * key. For the moment, let's make it give a stutter dialtone
                   dignum[channum] = 0;
                   if ( dx_clrdigbuf( chdev ) == -1 ) {
	                    sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			            ATDV_NAMEP( chdev ) );
	                    disp_status(channum, tmpbuff );
		                disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                   }
				   if (ATDX_STATE(chdev) == CS_IDLE) playtone_stutter(channum, 350, 440, -24, -26, 11, 11, 1);
				   else {
					   dx_stopch(chdev, EV_ASYNC);
					   // playtone_stutter(channum, 350, 440, -24, -26, 11, 11, 1);
				   }
				   return(0);
			   }
               */
               /*
			   if (cstp->cst_data == 0x61 ) {
				   // A key
                   dignum[channum] = 0;
                   if ( dx_clrdigbuf( chdev ) == -1 ) {
	                    sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			            ATDV_NAMEP( chdev ) );
	                    disp_status(channum, tmpbuff );
		                disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                   }
				   if ( ATDX_STATE( chdev ) == CS_IDLE ) playtone_stutter( channum, 400, 0, -24, -26, 11, 11, 1 );
				   return(0);
			   }
			   if (cstp->cst_data == 0x62 ) {
				   // B key
                   dignum[channum] = 0;
                   if ( dx_clrdigbuf( chdev ) == -1 ) {
	                    sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			            ATDV_NAMEP( chdev ) );
	                    disp_status(channum, tmpbuff );
		                disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                   }
				   if ( ATDX_STATE( chdev ) == CS_IDLE ) playtone_stutter( channum, 440, 0, -24, -26, 11, 11, 1 );
				   return(0);
			   }
               
			   if (cstp->cst_data == 0x63 ) {
				   // C key
                   dignum[channum] = 0;
                   if ( dx_clrdigbuf( chdev ) == -1 ) {
	                    sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			            ATDV_NAMEP( chdev ) );
	                    disp_status(channum, tmpbuff );
		                disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                   }
				   if ( ATDX_STATE( chdev ) == CS_IDLE ) playtone_stutter( channum, 480, 0, -24, -26, 11, 11, 1 );
				   return(0);
			   }
               
			   if (cstp->cst_data == 0x64 ) {
				   // D key
                   dignum[channum] = 0;
                   if ( dx_clrdigbuf( chdev ) == -1 ) {
	                    sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			            ATDV_NAMEP( chdev ) );
	                    disp_status(channum, tmpbuff );
		                disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                   }
				   if ( ATDX_STATE( chdev ) == CS_IDLE ) playtone_stutter( channum, 480, 0, -24, -26, 11, 11, 1 );
				   return(0);
			   }
               */

			   if (cstp->cst_data == 0x23) {
				   // # key
				   // For the moment, let's just do this.
				   if ( ATDX_STATE( chdev ) == CS_IDLE ) playtone_rep( channum, 480, 620, -25, -27, 25, 25, 40 );
				   else dx_stopch(chdev, EV_ASYNC);
				   dxinfox[ channum ].state = ST_REORDER;
				   return(0);
			   }
			   return(0);
		   case 2:
               if ((dxinfox[channum].digbuf.dg_value[0] == 0x33) || (dxinfox[channum].digbuf.dg_value[0] == 0x34) || (dxinfox[channum].digbuf.dg_value[0] == 0x2A) || (dxinfox[channum].digbuf.dg_value[0] == 0x39) ) {
                   if( ( cstp->cst_data == 0x23) || (cstp->cst_data == 0x2A) ) {
				       playtone_rep( channum, 480, 620, -25, -27, 25, 25, 40 );
				       dxinfox[ channum ].state = ST_REORDER;
                       return 0;
                   }
                   /*
	               if (get_digits(channum, &(dxinfox[channum].digbuf), 1) == -1) {
		              sprintf(tmpbuff, "Cannot get digits from channel %s",
			              ATDV_NAMEP(chdev));
		              disp_status(channum, tmpbuff);
	                }

                    */
                   return 0;
               }
			   // if (cstp->cst_data == 0x39 ) return(0);

               /*
			   if ( (cstp->cst_data > 0x38 ) || ( cstp->cst_data < 0x30 ) ) {
				   // For the moment, let's just do this.
				   playtone_rep( channum, 480, 620, -24, -26, 25, 25, 40 );
				   dxinfox[ channum ].state = ST_REORDER;
				   return(0);

			   }
			   else {
				   dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
				   dxinfox[ channum ].state = ST_RINGPHONE1;
				   ringphone( channum, dxinfox[ channum ].digbuf.dg_value );
				   return(0);
			   }
               */
               return 0;
		   case 3:
               if ((dxinfox[channum].digbuf.dg_value[0] == 0x33) || (dxinfox[channum].digbuf.dg_value[0] == 0x34)) {
                   if( ( cstp->cst_data == 0x23) || (cstp->cst_data == 0x2A) ) {
				       playtone_rep( channum, 480, 620, -25, -27, 25, 25, 40 );
				       dxinfox[ channum ].state = ST_REORDER;
                       return 0;
                   }
                   /*
	               if (get_digits(channum, &(dxinfox[channum].digbuf), 1) == -1) {
		              sprintf(tmpbuff, "Cannot get digits from channel %s",
			              ATDV_NAMEP(chdev));
		              disp_status(channum, tmpbuff);
	                }
                    */
                   return 0;
               }
               
               if (dxinfox[channum].digbuf.dg_value[0] == 0x39) {
                   dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
                   dxinfox[ channum ].state = ST_RINGPHONE1;
                   isdn_origtest( channum, dxinfox[channum].digbuf.dg_value, dntable[channum]);
                   memset(dxinfox[channum].digbuf.dg_value, 0x00, DG_MAXDIGS);
                   return 0;
               }
               
               if (dxinfox[channum].digbuf.dg_value[0] == 0x2A) {
                   if (strcmp(dxinfox[channum].digbuf.dg_value, "*67") == 0) {
                       modifier[channum] |= 1;
                       dignum[channum] = 0;
                       if ( dx_clrdigbuf( chdev ) == -1 ) {
	                        sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			                ATDV_NAMEP( chdev ) );
	                        disp_status(channum, tmpbuff );
		                    disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                        }
				       if (ATDX_STATE(chdev) == CS_IDLE) playtone_stutter(channum, 350, 440, -24, -26, 11, 11, 1);
				       else {
    					   dx_stopch(chdev, EV_ASYNC);
				       }
				   return(0);
                   }
                   
                   else if (strcmp(dxinfox[channum].digbuf.dg_value, "*82") == 0) {
                       modifier[channum] &= 1;
                       dignum[channum] = 0;
                       if ( dx_clrdigbuf( chdev ) == -1 ) {
	                        sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			                ATDV_NAMEP( chdev ) );
	                        disp_status(channum, tmpbuff );
		                    disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                        }
				       if (ATDX_STATE(chdev) == CS_IDLE) playtone_stutter(channum, 350, 440, -24, -26, 11, 11, 1);
				       else {
    					   dx_stopch(chdev, EV_ASYNC);
				       }
				   return(0);
                   }
                   
                   else if (strcmp(dxinfox[channum].digbuf.dg_value, "*99") == 0) {
                       dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
                       // Announcement test
                       if ( dx_clrdigbuf( chdev ) == -1 ) {
	                        sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			                ATDV_NAMEP( chdev ) );
	                        disp_status(channum, tmpbuff );
		                    disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                        }
                       dxinfox[ channum ].state = ST_ERRORANNC;
                       dxinfox[ channum ].msg_fd = open( "sounds/error/cbcad.pcm", O_RDONLY );
                       play( channum, dxinfox[ channum ].msg_fd, 0x81, 0 );
                       return(0);
                   }
                   
                   else if (strcmp(dxinfox[channum].digbuf.dg_value, "*98") == 0) {
                       dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
                       // Announcement test
                       if ( dx_clrdigbuf( chdev ) == -1 ) {
	                        sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			                ATDV_NAMEP( chdev ) );
	                        disp_status(channum, tmpbuff );
		                    disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                        }
                       dxinfox[ channum ].state = ST_ERRORANNC;
                       dxinfox[ channum ].msg_fd = open( "sounds/error/ycdngt.pcm", O_RDONLY );
                       play( channum, dxinfox[ channum ].msg_fd, 0x81, 0 );
                       return(0);
                   }
                   else if (strcmp(dxinfox[channum].digbuf.dg_value, "*97") == 0) {
                       dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
                       // Announcement test
                       if ( dx_clrdigbuf( chdev ) == -1 ) {
	                        sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			                ATDV_NAMEP( chdev ) );
	                        disp_status(channum, tmpbuff );
		                    disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                        }
                       dxinfox[ channum ].state = ST_ERRORANNC;
                       dxinfox[ channum ].msg_fd = open( "sounds/error/facilitytrouble.pcm", O_RDONLY );
                       play( channum, dxinfox[ channum ].msg_fd, 0x81, 0 );
                       return(0);
                   }
                   else if (strcmp(dxinfox[channum].digbuf.dg_value, "*96") == 0) {
                       dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
                       // Announcement test
                       if ( dx_clrdigbuf( chdev ) == -1 ) {
	                        sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			                ATDV_NAMEP( chdev ) );
	                        disp_status(channum, tmpbuff );
		                    disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                        }
                       dxinfox[ channum ].state = ST_ERRORANNC;
                       dxinfox[ channum ].msg_fd = open( "sounds/error/acb.pcm", O_RDONLY );
                       play( channum, dxinfox[ channum ].msg_fd, 0x81, 0 );
                       return(0);
                   }
                   else if (strcmp(dxinfox[channum].digbuf.dg_value, "*95") == 0) {
                       dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
                       // Announcement test
                       if ( dx_clrdigbuf( chdev ) == -1 ) {
	                        sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			                ATDV_NAMEP( chdev ) );
	                        disp_status(channum, tmpbuff );
		                    disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                        }
                       dxinfox[ channum ].state = ST_ERRORANNC;
                       dxinfox[ channum ].msg_fd = open( "sounds/error/acb2.pcm", O_RDONLY );
                       play( channum, dxinfox[ channum ].msg_fd, 0x81, 0 );
                       return(0);
                   }
                   
                   else if (strcmp(dxinfox[channum].digbuf.dg_value, "*94") == 0) {
                       dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
                       // IVR test
                       if ( dx_clrdigbuf( chdev ) == -1 ) {
	                        sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			                ATDV_NAMEP( chdev ) );
	                        disp_status(channum, tmpbuff );
		                    disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                        }
     	  	  	       dxinfox[ channum ].state = ST_DYNPLAY;
		  	  	       ownies[ channum ] = 0; // Initialize variables
	  	  	  	       playoffset2[ channum ] = 0;
	 	  	  	       anncnum[ channum ] = 2000;
		  	  	       errcnt[ channum ] = 1;
		  	  	       while ( errcnt[ channum ] == 1 )
	 	  	  	       {
		  	  	       sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
	 	  	  	       if (stat( dxinfox[ channum ].msg_name, &sts ) == -1) errcnt[ channum ] = 0;
		  	  	       else anncnum[ channum ]++;
	 	  	  	       }
		  	  	       maxannc[ channum ] = ( anncnum[ channum ] );
	 	  	  	       anncnum[ channum ] = 2000;
	 	  	  	       errcnt[ channum ] = 1;
	 
	 	  	  	       while ( errcnt[ channum ] == 1 )
		  	  	       {
	 	  	  	           sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
	 	  	  	           if (stat( dxinfox[ channum ].msg_name, &sts ) == -1) errcnt[ channum ] = 0;
	 	  	  	           else anncnum[ channum ]--;
		  	  	       }

	  	              minannc[ channum ] = ( anncnum[ channum ] );
	 	  	          sprintf( tmpbuff, "maxannc is %d.", maxannc[ channum ] );
		  	          disp_msg( tmpbuff );

	                  anncnum[ channum ] = 2000;
	  	  	          sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
     	  	  	      dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
      	  	  	      if ( dxinfox[ channum ].msg_fd == -1 ) {
        	  	          sprintf( tmpbuff, "Cannot open %s for play-back",
                          dxinfox[ channum ].msg_name );
         	              disp_msg( tmpbuff );
           	              return(-1);
				        }

         	         play( channum, dxinfox[ channum ].msg_fd, 1, 0 );
   	  	  	         return( 0 );
                   }
                   else if (strcmp(dxinfox[channum].digbuf.dg_value, "*93") == 0) {
                       dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
                       // Announcement test
                       if ( dx_clrdigbuf( chdev ) == -1 ) {
	                        sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			                ATDV_NAMEP( chdev ) );
	                        disp_status(channum, tmpbuff );
		                    disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
                        }
                       dxinfox[ channum ].state = ST_REORDER;
                       dxinfox[ channum ].msg_fd = open( "sounds/start.pcm", O_RDONLY );
                       confparse();
                       play( channum, dxinfox[ channum ].msg_fd, 0x81, 0 );
                       return(0);
                   }
               }
                playtone_rep( channum, 480, 620, -25, -27, 25, 25, 40 );
                dxinfox[ channum ].state = ST_REORDER;
                return 0;
               /*
			   if (cstp->cst_data == 0x31 ) {
				   // For the moment, let's just do this.
				   playtone_rep( channum, 480, 620, -24, -26, 25, 25, 40 );
				   dxinfox[ channum ].state = ST_REORDER;
				   return(0);
			   }
               */
		   case 4:
               if ((dxinfox[channum].digbuf.dg_value[0] == 0x33) || (dxinfox[channum].digbuf.dg_value[0] == 0x34)) {
                   if( ( cstp->cst_data == 0x23) || (cstp->cst_data == 0x2A) ) {
				       playtone_rep( channum, 480, 620, -25, -27, 25, 25, 40 );
				       dxinfox[ channum ].state = ST_REORDER;
                       return 0;
                   }
				   dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
                   
                   /*
                   if (strcmp (dxinfox[channum].digbuf.dg_value, "3973") == 0 ) {
                       // Announcement test
                       dxinfox[ channum ].state = ST_ERRORANNC;
                       dxinfox[ channum ].msg_fd = open( "sounds/error/cbcad.pcm", O_RDONLY );
                       play( channum, dxinfox[ channum ].msg_fd, 0x81, 0 );
                       return(0);
                   }
                   */
                   if (strcmp (dxinfox[channum].digbuf.dg_value, ivrtest) == 0 ) {
     	  	  	       dxinfox[ channum ].state = ST_DYNPLAY;
		  	  	       ownies[ channum ] = 0; // Initialize variables
	  	  	  	       playoffset2[ channum ] = 0;
	 	  	  	       anncnum[ channum ] = 2000;
		  	  	       errcnt[ channum ] = 1;
		  	  	       while ( errcnt[ channum ] == 1 )
	 	  	  	       {
		  	  	       sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
	 	  	  	       if (stat( dxinfox[ channum ].msg_name, &sts ) == -1) errcnt[ channum ] = 0;
		  	  	       else anncnum[ channum ]++;
	 	  	  	       }
		  	  	       maxannc[ channum ] = ( anncnum[ channum ] );
	 	  	  	       anncnum[ channum ] = 2000;
	 	  	  	       errcnt[ channum ] = 1;
	 
	 	  	  	       while ( errcnt[ channum ] == 1 )
		  	  	       {
	 	  	  	           sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
	 	  	  	           if (stat( dxinfox[ channum ].msg_name, &sts ) == -1) errcnt[ channum ] = 0;
	 	  	  	           else anncnum[ channum ]--;
		  	  	       }

	  	              minannc[ channum ] = ( anncnum[ channum ] );
	 	  	          sprintf( tmpbuff, "maxannc is %d.", maxannc[ channum ] );
		  	          disp_msg( tmpbuff );

	                  anncnum[ channum ] = 2000;
	  	  	          sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
     	  	  	      dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
      	  	  	      if ( dxinfox[ channum ].msg_fd == -1 ) {
        	  	          sprintf( tmpbuff, "Cannot open %s for play-back",
                          dxinfox[ channum ].msg_name );
         	              disp_msg( tmpbuff );
           	              return(-1);
				        }

         	         play( channum, dxinfox[ channum ].msg_fd, 1, 0 );
   	  	  	         return( 0 );
                    }
				   dxinfox[ channum ].state = ST_RINGPHONE1;
                   // Insert code to do route treatment here, and only go to ringphone() if a logical identifier is found
                   // Since we no longer need dignum, why not reuse it?
                   dignum[channum] = dnlookup[atoi(dxinfox[channum].digbuf.dg_value) & 0x3FF];
                   if (dignum[channum] != 0 ) {
                       ringphone( channum, dignum[channum] );
                   }
                   else {
                       isdn_origtest( channum, dxinfox[channum].digbuf.dg_value, dntable[channum]);
                       memset(dxinfox[channum].digbuf.dg_value, 0x00, DG_MAXDIGS);
                   }
                   return 0;
               }
               /*
			   if (cstp->cst_data == 0x31 ) {
				   // For the moment, let's just do this.
				   playtone_rep( channum, 480, 620, -24, -26, 25, 25, 40 );
				   dxinfox[ channum ].state = ST_REORDER;
				   return(0);
			   }
			   if (cstp->cst_data == 0x32 ) {
				   // Just for the hell of it...
				   playtone_cad( channum, 480, 481, -1 );
				   dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
				   dxinfox[channum].state = ST_PERMSIG;
				   return(0);
			   }
                

			   else {
				   dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
     	  	  	   dxinfox[ channum ].state = ST_DYNPLAY;
		  	  	   ownies[ channum ] = 0; // Initialize variables
	  	  	  	   playoffset2[ channum ] = 0;
	 	  	  	   anncnum[ channum ] = 2000;
		  	  	   errcnt[ channum ] = 1;
		  	  	   while ( errcnt[ channum ] == 1 )
	 	  	  	   {
		  	  	   sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
	 	  	  	   if (stat( dxinfox[ channum ].msg_name, &sts ) == -1) errcnt[ channum ] = 0;
		  	  	   else anncnum[ channum ]++;
	 	  	  	   }
		  	  	   maxannc[ channum ] = ( anncnum[ channum ] );
	 	  	  	   anncnum[ channum ] = 2000;
	 	  	  	   errcnt[ channum ] = 1;
	 
	 	  	  	   while ( errcnt[ channum ] == 1 )
		  	  	   {
	 	  	  	   sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
	 	  	  	   if (stat( dxinfox[ channum ].msg_name, &sts ) == -1) errcnt[ channum ] = 0;
	 	  	  	   else anncnum[ channum ]--;
		  	  	   }

	  	          minannc[ channum ] = ( anncnum[ channum ] );
	 	  	      sprintf( tmpbuff, "maxannc is %d.", maxannc[ channum ] );
		  	      disp_msg( tmpbuff );

	              anncnum[ channum ] = 2000;
	  	  	      sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
     	  	  	  dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
      	  	  	  if ( dxinfox[ channum ].msg_fd == -1 ) {
        	  	  sprintf( tmpbuff, "Cannot open %s for play-back",
                  dxinfox[ channum ].msg_name );
         	      disp_msg( tmpbuff );
           	      return(-1);
				  }

         	     play( channum, dxinfox[ channum ].msg_fd, 1, 0 );
   	  	  	     return( 0 );
			   	 }
                 */
     	   default:
	           playtone_rep( channum, 480, 620, -25, -27, 25, 25, 40 );
			   dxinfox[ channum ].state = ST_REORDER;
		   }

		   return(0);

	   default:
	     disp_msg("Digit detector being tripped on invalid state. Disabling...");
                 dx_stopch( dxinfox[ channum ].chdev, EV_ASYNC ); // The event mask was returning errors, so let's do this first
		 if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_RINGS | DM_DIGOFF ) == -1 ) {
		 sprintf( tmpbuff, "Cannot set CST events for %s",
			ATDV_NAMEP( dxinfox[ channum ].chdev ) );
		 disp_status(channum, tmpbuff );
		 //sys_quit(); // This was causing the system to have issues, so let's have it stop this.
                 return -1;
         } // end if 
		   return(0);
	   }

   default:
      sprintf( tmpbuff, "Unknown CST Event (%d) Received on %s",
		cstp->cst_event, ATDV_NAMEP( chdev ) );
      disp_msg( tmpbuff ); 
      disp_status(channum, "Unknown Event" );
   }

   return( 0 );
}

/***************************************************************************
 *        NAME: int dial_hdlr()
 * DESCRIPTION: TDX_DIAL event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ***************************************************************************/
int dial_hdlr() {
   int  chdev = sr_getevtdev();
   //int  event = sr_getevttype();
   int channum = get_channum( chdev );
   int curstate;
   //int errcode = 0;
   if (chdev == -1) disp_msg("ERROR: -1 on dial chdev");
   if ( channum == -1 ) {
	   disp_msg("ERROR: -1 on get_channum");
      return( 0 );              /* Discard Message - Not for a Known Device */
   }

   curstate = dxinfox[ channum ].state;          /* Current State */
   
   /*
    * If drop in loop current, set state to ONHOOK and
    * set channel to ONHOOK in case of ANALOG frontend.
    * In case of digital frontend, the sig_hdlr will take
    * care of it.
    */
   switch ( frontend ) {
   case CT_NTANALOG:
      if ( ATDX_TERMMSK( chdev ) & TM_LCOFF ) {
         dxinfox[ channum ].state = ST_ONHOOK;
         set_hkstate( channum, DX_ONHOOK );
         return( 0 );
      }
      break;

   case CT_NTT1:
      if ( ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev ) & DTSG_RCVA ) == 0 ) {
         return( 0 );
      }
      break;

   case CT_NTE1:
      if ( ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev ) & DTSG_RCVA ) != 0 ) {
         return( 0 );
      }
      break;
   case CT_NTTEST:

	   // Don't go here on ESF/B8ZS if 0xA5. Also, find a better way to bitmask this.
       if (fxo[channum] == 2) {
           if (isdninfo[channum].status == 1) {
               /*
		       if (ownies[channum] == 2) {
                   sprintf(tmpbuff, "DEBUG: connchan is %d", connchan[channum]);
                   disp_msg(tmpbuff);
                   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // Turn the ringing voltage off plz (E&M hack)
    			   dxinfox[connchan[channum]].state = ST_WTRING;
	    		   // Reset the calling/called party channel connection table
			       ownies[channum] = 0;
		    	   connchan[channum] = 0;
			       connchan[connchan[channum]] = 0;
		       }
               */
               return(0);
           }
       }
       
	   switch (ATDT_TSSGBIT(dxinfox[channum].tsdev)) {
           // This is for loop start signaling. So, uh, commented out.
	   case 0xA0:
	   case 0xA1:
	   case 0xA2:
	   case 0xA3:
	   case 0xA4:
	   case 0xA6:
	   case 0xA7:
	   case 0xA8:
	   case 0xA9:
	   case 0xAB:
	   case 0xAC:
	   case 0xAD:
	   case 0xAE:
	   case 0xAF:

           /* Here be E&M hax */
	   case 0xAA:
       case 0x08:
       case 0x09:
       case 0x0A:
       case 0x0B:
       case 0x0C:
       case 0x0D:
       case 0x0E:
       case 0x0F:
		   sprintf(tmpbuff, "DEBUG: dial_hdlr stopped during FXS hangup, state %d, ownies %d, tsbits %li", dxinfox[channum].state, ownies[channum], ATDT_TSSGBIT(dxinfox[channum].tsdev));
		   disp_msg(tmpbuff);
       if (fxo[channum] == 0)    {
	       if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF ) == -1 ) {
		        sprintf( tmpbuff, "Cannot set CST events DM_DIGOFF for %s",
	            ATDV_NAMEP( dxinfox[ channum ].chdev ) );
    	        disp_status(channum, tmpbuff );
                disp_err( channum, dxinfox[ channum ].chdev, "dx_setevtmsk() error for DM_DIGOFF via playtone_hdlr" );
                printf("[DEBUG] current device state is: %li\n", ATDX_STATE( dxinfox[channum].chdev ));
		        sys_quit();
            } // end if 
       }

		   return(0);
	   default: // No. Baaaaad! This shouldn't be doing this for 0xA5! >:( Or really anything else.
		   break; 
	   }

      break;
   }

   switch ( curstate ) {

   case ST_PERMSIG:
	   // Permanent signal workaround for pause state.
	   playtone_cad( channum, 480, 0, -1 );
	   return(0);
   case ST_ROUTED:
	   // We're done sending a pause for ringback timing? Cool, doesn't matter. Keep the DSP in an idle state.
	   ownies[ channum ] = 0;
	   return(0);

   case ST_RINGPHONE2:
	   // ownies[ channum ] = 0;
	   // dt_settssigsim( dxinfox[ connchan[channum] ].tsdev, DTB_AOFF | DTB_BOFF ); // SF/B8ZS
	   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BON | DTB_COFF | DTB_DON ); // Ringing, E&M hack
	   playtone_cad( channum, 440, 480, 200 );
	   dxinfox[ channum ].state--;
	   disp_msg("Ringback off phase");
	   return(0);
   case ST_FXSTEST1:
       if (ownies[channum] == 69) {
           ownies[channum] = 0;
	    if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGITS ) == -1 ) {
		     sprintf( tmpbuff, "Cannot set CST events DM_DIGITS for %s",
	    	 ATDV_NAMEP( dxinfox[ channum ].chdev ) );
    		 disp_status(channum, tmpbuff );
		    sys_quit();
            } // end if 
           playtone( channum, 400, 0, 2000 );
           return 0;
       }
	   //disp_msg("Hitting bugfix. Does it work?");
	   dxinfox[ connchan[ channum ]].state = ST_WTRING;
       connchan[ channum ] = 0;
	   connchan[ connchan[ channum ] ] = 0;
	   ownies[ channum ] = 0;
	   return(0);

   default:
   sprintf( tmpbuff, "DEBUG: Dial thingie stopped for unidentified reason in test mode. State is %d", dxinfox[channum].state );
   disp_msg(tmpbuff);
   return(0);

   }


}

/***************************************************************************
 *        NAME: int playtone_hdlr()
 * DESCRIPTION: TDX_PLAYTONE event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ***************************************************************************/
int playtone_hdlr() {
   int  chdev = sr_getevtdev();
   // int  event = sr_getevttype();
   int channum = get_channum( chdev );
   int curstate;
   //int errcode = 0;
   if (chdev == -1) disp_msg("ERROR: -1 on playtone chdev");
   if ( channum == -1 ) {
	   disp_msg("ERROR: -1 on get_channum");
      return( 0 );              /* Discard Message - Not for a Known Device */
   }

   curstate = dxinfox[ channum ].state;          /* Current State */
   
   /*
    * If drop in loop current, set state to ONHOOK and
    * set channel to ONHOOK in case of ANALOG frontend.
    * In case of digital frontend, the sig_hdlr will take
    * care of it.
    */
   switch ( frontend ) {
   case CT_NTANALOG:
      if ( ATDX_TERMMSK( chdev ) & TM_LCOFF ) {
         dxinfox[ channum ].state = ST_ONHOOK;
         set_hkstate( channum, DX_ONHOOK );
         return( 0 );
      }
      break;

   case CT_NTT1:
      if ( ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev ) & DTSG_RCVA ) == 0 ) {
         return( 0 );
      }
      break;

   case CT_NTE1:
      if ( ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev ) & DTSG_RCVA ) != 0 ) {
         return( 0 );
      }
      break;
   case CT_NTTEST:
	   if (fxo[channum] == 1) {
		   if (ATDT_TSSGBIT(dxinfox[channum].tsdev) == 0x31) { // SF/B8ZS
			   sprintf(tmpbuff, "DEBUG: playtone_hdlr stopped during FXO hangup, state %d, ownies is %d", dxinfox[channum].state, ownies[channum]);
			   disp_msg(tmpbuff);
			   return(0);
		   }

	   }
       
       else if (fxo[channum] == 2) {
           if (isdninfo[channum].status == 1) {
               /*
               // Hi! You're handled in the ISDN handler now.
	           if ( ownies[ channum ] == 2 ) {
		           dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // Turn the ringing voltage off plz (E&M hack)
		           dxinfox[ connchan[ channum ]].state = ST_WTRING;
		           ownies[ channum ] = 0;
		           // Reset the calling/called party channel connection table
		           connchan[ channum ] = 0;
        		   connchan[ connchan[ channum ] ] = 0;
	            }
                */
               
               return(0);
           }
       }

	   else if ((ATDT_TSSGBIT(dxinfox[channum].tsdev) & 0xF0) == 0xA0) { // ESF/B8ZS
	   // else if ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev )  == 0x22 ) { // SF/B8ZS
       if (fxo[channum] == 0 ) {
	       if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF ) == -1 ) {
		        sprintf( tmpbuff, "Cannot set CST events DM_DIGOFF for %s",
	            ATDV_NAMEP( dxinfox[ channum ].chdev ) );
    	        disp_status(channum, tmpbuff );
                disp_err( channum, dxinfox[ channum ].chdev, "dx_setevtmsk() error for DM_DIGOFF via playtone_hdlr" );
                printf("[DEBUG] current device state is: %li\n", ATDX_STATE( dxinfox[channum].chdev ));
		        sys_quit();
            } // end if 
       }
	   if ( ownies[ channum ] == 2 ) {
		   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // Turn the ringing voltage off plz (E&M hack)
		   dxinfox[ connchan[ channum ]].state = ST_WTRING;
		   ownies[ channum ] = 0;
		   // Reset the calling/called party channel connection table
		   connchan[ channum ] = 0;
		   connchan[ connchan[ channum ] ] = 0;
	   }
	   if ( ownies[ channum ] == 3 ) {
		   dxinfox[ channum ].state = ST_WTRING;
		   // Reset the calling/called party channel connection table
		   //ownies[ channum ] = 0;
	   }
	   return(0);
	   }
      break;
   }
  

   switch ( curstate ) {
       
   case ST_RETURNDIGS:
   dxinfox[channum].state = ST_FXSTEST1;
   if (get_digits(channum, &(dxinfox[channum].digbuf), 1) == -1) {
       sprintf(tmpbuff, "Cannot get digits from channel %s",
       ATDV_NAMEP(chdev));
       disp_status(channum, tmpbuff);
	}
    return(0);
       
   case ST_ISDNOUT2:
       dxinfox[channum].state--;
       dx_setevtmsk( dxinfox[ channum ].chdev, DM_RINGS | DM_DIGOFF );
       playtone(channum, 360, 450, 2000);
       return(0);
       
   case ST_ACBANNC:
       dxinfox[channum].state = ST_REORDER;
       play( channum, dxinfox[channum].msg_fd, 0x81, 0 );
       break;
       
   case ST_ISDNOUT:
       if( get_digits( channum, &dxinfox[ channum ].digbuf, 11) == -1 ) {
		   sprintf(tmpbuff, "Cannot get digits from channel %s",
			   ATDV_NAMEP(chdev));
		   disp_status(channum, tmpbuff);
           dxinfox[channum].state = ST_REORDER;
           playtone_rep( channum, 480, 620, -25, -27, 25, 25, 40 );
       }
       return(0);
       
   case ST_FXODISA:
	   if (get_digits(channum, &(dxinfox[channum].digbuf), 6) == -1) {
		   sprintf(tmpbuff, "Cannot get digits from channel %s",
			   ATDV_NAMEP(chdev));
		   disp_status(channum, tmpbuff);
	   }
	   return(0);

   case ST_PERMSIG:
	   dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // ESF/B8ZS
	   // dt_settssigsim( dxinfox[channum].tsdev, DTB_AOFF | DTB_BON ); // SF/B8ZS
	   playtone_cad( channum, 480, 0, -1 );
	   return(0);
   case ST_ROUTED:
	   // They're not listening to the DSP anymore :( . Turn everything off.
	   ownies[ channum ] = 0;
	   return(0);
   case ST_RINGPHONE2:
	   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BON | DTB_COFF | DTB_DON ); // Ringing, E&M hack
	   // dt_settssigsim( dxinfox[ connchan[channum] ].tsdev, DTB_AOFF | DTB_BOFF ); // SF/B8ZS
	   playtone_cad( channum, 440, 480, 200 );
	   dxinfox[ channum ].state--;
	   disp_msg("Ringback off phase");
	   return(0);

   case ST_RINGPHONE1:
	   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // Idle, E&M hack
	   dx_dial( dxinfox[ channum ].chdev, ",,", NULL, EV_ASYNC);
	   dxinfox[ channum ].state++;
	   return(0);

   case ST_REORDER:
       if (fxo[channum] == 0 ) {
           playtone_rep( channum, 480, 620, -24, -26, 25, 25, 40 );
       }
       
       else if (fxo[channum] == 1) {
           if (ownies[channum] == 1) {
	           dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // We're not removing loop current for FXO, just hanging the interface up. ESF/B8ZS
		       dxinfox[channum].state = ST_WTRING;
           }
           else {
               playtone_rep( channum, 480, 620, -24, -26, 25, 25, 40 );
               ownies[channum] = 1;
           }
       }
       
       else if (fxo[channum] == 2) {
            // Pass temporarily unavailable cause code to network and gtfo
            if (isdn_drop(channum, 41) == -1) {
                disp_msg("Holy shit! isdn_drop() failed! D:");
                return (-1);
            }
       }
	   return(0);
       
   case ST_WARBLE:
       if (fxo[channum] == 0 ) {
           playtone_warble(channum, 490, 325, -17, -17, 25, 40);
       }
       else if (fxo[channum] == 1) {
           if (ownies[channum] == 1) {
	           dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // We're not removing loop current for FXO, just hanging the interface up. ESF/B8ZS
		       dxinfox[channum].state = ST_WTRING;
           }
           else {
               playtone_warble(channum, 490, 325, -17, -17, 25, 40);
               ownies[channum] = 1;
           }
       }
       
       else if (fxo[channum] == 2) {
            // Pass temporarily unavailable cause code to network and gtfo
            if (isdn_drop(channum, 41) == -1) {
                disp_msg("Holy shit! isdn_drop() failed! D:");
                return (-1);
            }
       }
	   return(0);
       
       
   case ST_BUSY:
        if (fxo[channum] == 0 ) {
			playtone_rep(channum, 480, 620, -24, -26, 50, 50, 40);
        }
       else if (fxo[channum] == 1) {
           if (ownies[channum] == 1) {
	           dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // We're not removing loop current for FXO, just hanging the interface up. ESF/B8ZS
		       dxinfox[channum].state = ST_WTRING;
           }
           else {
               playtone_rep(channum, 480, 620, -24, -26, 50, 50, 40);
               ownies[channum] = 1;
           }
       }
        
        else if (fxo[channum] == 2) {
            // Pass user busy cause code to network and split
            if (isdn_drop(channum, GC_USER_BUSY) == -1) {
                disp_msg("Holy shit! isdn_drop() failed! D:");
                return (-1);
            }
        }
        return(0);
   case ST_FXSTEST1:
 
       if (ownies[channum] == 69) {
           ownies[channum] = 0;
	    if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGITS ) == -1 ) {
		     sprintf( tmpbuff, "Cannot set CST events DM_DIGITS for %s",
	    	 ATDV_NAMEP( dxinfox[ channum ].chdev ) );
    		 disp_status(channum, tmpbuff );
		    sys_quit();
            } // end if 
           playtone( channum, 400, 0, 2000 );
           return 0;
       }
       if ( ATDX_TERMMSK( chdev ) & TM_MAXDTMF ) {

		   switch (dxinfox[channum].digbuf.dg_value[dignum[channum]]) {

		   case 0x2A:
			   playtone_stutter(channum, 350, 440, -24, -26, 11, 11, 1);
			   return(0);

		   default:
			   return(0);
		   }

           // disp_msg("DEBUG: Dialtone thingie received a digit!");
		   return(0);
		   // playtone_rep( channum, 480, 620, -24, -26, 25, 25 );
       }
	   if ( dignum[ channum ] == 0 ) {
		   // We've, uh, sat, on the dialtone without pressing anything. Here's a permanent signal for you.
		   dxinfox[ channum ].state = ST_PERMSIG;
		   dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
		   // playtone_cad( dxinfox[ channum ].chdev, 480, 0, 100 );
           // This should go to the loop current drop state.
		   if (dt_castmgmt(dxinfox[channum].tsdev, &reversalxmit, &reversalxmit_response) == -1) {
               sprintf(tmpbuff, "E&M Permanent Signal routine failed with error %s", ATDV_ERRMSGP( dxinfox[channum].tsdev ));
               disp_err( channum, dxinfox[ channum ].tsdev, tmpbuff );
		   }
               return(0);   
	   }
           if (fxo[connchan[channum]] == 0) {
	   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // Ringing, E&M hack
	   dxinfox[ connchan[ channum ]].state = ST_WTRING;
       connchan[ channum ] = 0;
	   connchan[ connchan[ channum ] ] = 0;
	   ownies[ channum ] = 0;
           }
       return(0);
   default:
	   sprintf(tmpbuff, "DEBUG: Tone generator thingie stopped for unidentified reason in test mode. Chan %d, State %d, mask %li", channum, dxinfox[channum].state, ATDX_TERMMSK(chdev));
       disp_msg(tmpbuff);
   return(0);

   }


}


/***************************************************************************
 *        NAME: int play_hdlr()
 * DESCRIPTION: TDX_PLAY event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ***************************************************************************/
int play_hdlr()
{
   int	chdev = sr_getevtdev();
   // int	event = sr_getevttype();
   int	channum = get_channum( chdev );
   int	curstate;
   int	errcode = 0;

   if ( channum == -1 ) {
      return( 0 );		/* Discard Message - Not for a Known Device */
   }

   curstate = dxinfox[ channum ].state;		/* Current State */

   /*
    * If drop in loop current, set state to ONHOOK and
    * set channel to ONHOOK in case of ANALOG frontend.
    * In case of digital frontend, the sig_hdlr will take
    * care of it.
    */
   switch ( frontend ) {
   case CT_NTANALOG:
      if ( ATDX_TERMMSK( chdev ) & TM_LCOFF ) {
         dxinfox[ channum ].state = ST_ONHOOK;
         set_hkstate( channum, DX_ONHOOK );
         return( 0 );
      }
      break;

   case CT_NTT1:
      if ( ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev ) & DTSG_RCVA ) == 0 ) {
         return( 0 );
      }
      break;

   case CT_NTE1:
      if ( ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev ) & DTSG_RCVA ) != 0 ) {
         return( 0 );
      }
      break;

   case CT_NTTEST:
       if (channum > 24) {
           // This is a really stupid way to detect that a channel is ISDN
           // (in our current configuration, second span is a PRI)
           if (isdninfo[channum].status == 1) {
                close(dxinfox[ channum ].msg_fd);
                return (0);
           }
       }
	   if (fxo[channum] == 0) {
           if (!(ATDT_TSSGBIT(dxinfox[channum].tsdev) & 0xF0)) { // E&M Hack
		   //if ((ATDT_TSSGBIT(dxinfox[channum].tsdev) & 0xF0) == 0xA0) { // ESF/B8ZS
		   // if (ATDT_TSSGBIT(dxinfox[channum].tsdev) == 0x22) { // SF/B8ZS
			   close(dxinfox[channum].msg_fd);
               
	           if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF ) == -1 ) {
		            sprintf( tmpbuff, "Cannot set CST events DM_DIGOFF for %s",
	                ATDV_NAMEP( dxinfox[ channum ].chdev ) );
    	            disp_status(channum, tmpbuff );
                    disp_err( channum, dxinfox[ channum ].chdev, "dx_setevtmsk() error for DM_DIGOFF via playtone_hdlr" );
                    printf("[DEBUG] current device state is: %li\n", ATDX_STATE( dxinfox[channum].chdev ));
		            sys_quit();
                } // end if 
               
			   /*
			   sprintf(tmpbuff, "DEBUG: fd output is %d", _get_osfhandle(dxinfox[channum].msg_fd));
			   disp_msg(tmpbuff);
			   */
		   return(0);
	       }
	   }
	   else if (fxo[channum] == 1) {
           switch(ATDT_TSSGBIT(dxinfox[channum].tsdev)) {
               case 0xFA:
               case 0xF5:
			   close(dxinfox[channum].msg_fd);
               return(0);
               default:
               sprintf(tmpbuff, "DEBUG: tssgbit returned output %li", ATDT_TSSGBIT(dxinfox[channum].tsdev) );
               disp_msg(tmpbuff);
            }
        }
        
        else if (fxo[channum] == 2 ) {
            if (isdninfo[channum].status == 1 ) {
                close(dxinfox[channum].msg_fd);
                return(0);
            }
        }
           
	}

   switch ( curstate ) {
   case ST_INTRO:
   // From sethook_hdlr, after the intro file stops playing, the call ends up here.
      if ( ATDX_TERMMSK( chdev ) & TM_MAXDTMF ) {
	    dxinfox[ channum ].state = ST_GETDIGIT;

	    if ( get_digits( channum, &(dxinfox[ channum ].digbuf), MAXDTMF ) == -1 ) {
	        sprintf( tmpbuff, "Cannot get digits from channel %s",
			    ATDV_NAMEP( chdev ) );
	        disp_status(channum, tmpbuff );
            // If you were pressing digits, the call goes via the get_digits function to the ST_GETDIGIT function in the getdig_hdlr
	    }
      } else {			/* Need to Record a Message */
	    dxinfox[ channum ].state = ST_RECORD;
        
        // If under four digits (or none) are pressed, it records, and goes to the ST_RECORD state in the record_hdlr

	    if ( dx_clrdigbuf( chdev ) == -1 ) {
	        sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
		        ATDV_NAMEP( chdev ) );
	    
		    disp_err(channum, dxinfox[ channum ].chdev,tmpbuff);
        }

	    dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name,
			O_RDWR | O_TRUNC | O_CREAT, 0666);
	    if ( dxinfox[ channum ].msg_fd == -1 ) {
            sprintf( tmpbuff, "Cannot create message file %s",
		        dxinfox[ channum ].msg_name );
            disp_status(channum, tmpbuff );
            errcode = -1;
	    }

	    if ( errcode == 0 ) {
	        if (record( channum, dxinfox[ channum ].msg_fd ) == -1) {
                sprintf( tmpbuff, "Cannot Record Message on channel %s",
		            ATDV_NAMEP( chdev ) );
	            disp_status(channum, tmpbuff );
            }
	    }

        else {
            dxinfox[ channum ].state = ST_GOODBYE;

            if ( play( channum, goodbyefd, 0, 0 ) == -1 ) {
	            sprintf( tmpbuff, "Cannot Play Goodbye Message on channel %s",
		            ATDV_NAMEP( chdev ) );
	            disp_status(channum, tmpbuff );
            }
        }
      }
      break;
   
   case ST_ACBANNC:  
   case ST_ERRORANNC:
       dxinfox[channum].state = ST_REORDER;
       play( channum, dxinfox[channum].msg_fd, 0x81, 0 );
       break;

   case ST_PLAY:
      close( dxinfox[ channum ].msg_fd );
      dxinfox[ channum ].state = ST_GOODBYE;
      // Stuff terminating here goes back into the play_hdlr when done under the ST_GOODBYE state
      if ( play( channum, goodbyefd, 0, 0 ) == -1 ) {
	    sprintf( tmpbuff, "Cannot Play Goodbye Message on channel %s",
		    ATDV_NAMEP( chdev ) );
	    disp_status(channum, tmpbuff );
      }
      break;

   case ST_REORDER:
       close( dxinfox[ channum ].msg_fd );
       playtone_rep(channum, 480, 620, -24, -26, 25, 25, 40);
       return(0);
       
   case ST_PERMANNC:
       close( dxinfox[ channum ].msg_fd );
       dxinfox[channum].state = ST_PERMANNC2;
       dxinfox[channum].msg_fd = open( "sounds/error/ohtone.pcm", O_RDONLY );
       play( channum, dxinfox[ channum ].msg_fd, 0x81, 0 );
       return(0);
   case ST_PERMANNC2:
       close( dxinfox[ channum ].msg_fd );
       dxinfox[channum].state = ST_PERMSIG;
       playtone_cad( channum, 480, 0, -1 );
       return(0);
       
       
   case ST_EMPLAY2:
   case ST_DYNPLAY:
	   	close( dxinfox[ channum ].msg_fd );
   case ST_DYNPLAYE:
	   if ( dxinfox[ channum ].state == ST_DYNPLAYE ) dxinfox[ channum ].state = ST_DYNPLAY; // I don't like this, but what do you want for a 2 AM "I don't give a fuck" fix?
        playoffset[ channum ] = ATDX_TRCOUNT( chdev );
		dx_adjsv( dxinfox[ channum ].chdev, SV_VOLUMETBL, SV_ABSPOS, 0 ); // Reset volume to normal
		if (( ATDX_TERMMSK( chdev ) & TM_MAXDTMF ) || ownies[ channum ] == 1 ) {
			if ( get_digits( channum, &(dxinfox[ channum ].digbuf), 1 ) == -1 ) {
				sprintf( tmpbuff, "Cannot get digits from clownboat %s",
				ATDV_NAMEP( chdev ) );
				disp_msg( tmpbuff );
				}
			if ( ownies[ channum ] == 1 ) ownies[ channum ] = 0;
	/* The ownies variable is important; after receiving another playback event, it'll fall
	into the else function below. Otherwise, there's no way to make it do *just* that. */
    // Shit like that is embarassing. I really, really, really didn't know my stuff when I first started making this early code >.<
			return( 0 );
		}

		else {

			 if ( dx_clrdigbuf( chdev ) == -1 ) {
			 sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
			 ATDV_NAMEP( chdev ) );
			 disp_msg( tmpbuff );
             disp_err(channum, dxinfox[ channum ].chdev,"CLEAR DIGIT BUFFER");
			 // disp_err(channum, chdev, dxinfox[ channum ].state);
			 }
			ownies[ channum ] = 1;
			dx_adjsv( dxinfox[ channum ].chdev, SV_VOLUMETBL, SV_ABSPOS, 0 ); // Reset volume to normal
			if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/ivr_betabackfwd.pcm" );
			else sprintf( dxinfox[ channum ].msg_name, "sounds/ivr_embackfwd.pcm" );
			dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
			if ( play( channum, dxinfox[ channum ].msg_fd, 1, 0 ) == -1 ) {
				sprintf( tmpbuff, "Couldn't play the back/forward IVR message on channel %s, error was %s", ATDV_NAMEP( chdev ), ATDV_ERRMSGP( chdev ) );
				disp_msg( tmpbuff );
                dxinfox[channum].state = ST_GOODBYE;
                play( channum, goodbyefd, 0, 0);
                return(-1);
										  }

			return(0);
		}

   case ST_INVALID:
   case ST_GOODBYE:
   default:
	   if (frontend == CT_NTTEST) {
		   // playtone_cad( channum, 480, 620, -1 );
		   // dxinfox[channum].state = ST_PERMSIG;
		   return(0);
	   }
      dxinfox[ channum ].state = ST_ONHOOK;
      set_hkstate( channum, DX_ONHOOK );
      break;
   }

   return( 0 );
}

int nostop_hdlr()
{
    
    int chdev = sr_getevtdev();
    int channum = get_channum(chdev);
    if ( channum == -1) return 0; // FAKE channel?! ICCKK!!!!
    
    switch ( frontend ) {
        case CT_NTTEST:
        
        if (dxinfox[channum].state == ST_ISDNOUT2) {
            dxinfox[channum].state--;
        	if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_RINGS | DM_DIGOFF ) == -1 ) {
		        sprintf( tmpbuff, "Cannot set CST events for %s, error %s",
			      ATDV_NAMEP( dxinfox[ channum ].chdev ), ATDV_ERRMSGP( dxinfox[ channum ].chdev ) );
		        disp_status(channum, tmpbuff );
                return(-1);
            }
            playtone(channum, 360, 450, 2000);
            return 0;
        }
        if (dxinfox[channum].state == ST_PERMSIG) {
            playtone_cad( channum, 480, 0, -1 );
            return 0;
        }
        
        if (fxo[channum] == 2 ) {
            // For the moment, just return; nothing we can do here.
            return(0);
        }
	   switch (ATDT_TSSGBIT(dxinfox[channum].tsdev)) {
	   case 0xA0:
	   case 0xA1:
	   case 0xA2:
	   case 0xA3:
	   case 0xA4:
	   case 0xA6:
	   case 0xA7:
	   case 0xA8:
	   case 0xA9:
	   case 0xAB:
	   case 0xAC:
	   case 0xAD:
	   case 0xAE:
	   case 0xAF:
       /* Here be E&M hax */
       case 0xAA:
       // All tests for bit 4 being high and the four MSBs being low. We could just put that...
       case 0x08:
       case 0x09:
       case 0x0A:
       case 0x0B:
       case 0x0C:
       case 0x0D:
       case 0x0E:
       case 0x0F:
       
       if (dxinfox[channum].state == ST_ACBANNC) {
           dxinfox[channum].state = ST_REORDER;
           play( channum, dxinfox[channum].msg_fd, 0x81, 0 );
           return 0;
       }
       
       if (dxinfox[channum].state == ST_RETURNDIGS) {
	            if ( ATDX_STATE( chdev ) == CS_IDLE ) if (get_digits(channum, &(dxinfox[channum].digbuf), 1) == -1) {
		          sprintf(tmpbuff, "Cannot get digits from channel %s",
                  ATDV_NAMEP(chdev));
		          disp_status(channum, tmpbuff);
	            }
                return 0;
       }
       
       if (fxo[channum] == 0) {
	       if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF ) == -1 ) {
		        sprintf( tmpbuff, "Cannot set CST events DM_DIGOFF for %s",
	            ATDV_NAMEP( dxinfox[ channum ].chdev ) );
    	        disp_status(channum, tmpbuff );
                disp_err( channum, dxinfox[ channum ].chdev, "dx_setevtmsk() error for DM_DIGOFF via playtone_hdlr" );
                printf("[DEBUG] current device state is: %li\n", ATDX_STATE( dxinfox[channum].chdev ));
		        sys_quit();
            } // end if 
		   return 0;
       }

	   default:
       if (ownies[channum] == 69) {
           ownies[channum] = 0;
	    if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGITS ) == -1 ) {
		     sprintf( tmpbuff, "Cannot set CST events DM_DIGITS for %s",
	    	 ATDV_NAMEP( dxinfox[ channum ].chdev ) );
    		 disp_status(channum, tmpbuff );
		    sys_quit();
            } // end if 
           playtone( channum, 400, 0, 2000 );
           return 0;
       }
       
       if (dxinfox[channum].state == ST_ACBANNC) {
           close( dxinfox[ channum ].msg_fd );
       }
		   return 0;
	   }
        
        default:
        disp_msg("Nostop_hdlr has been reached on unknown frontend.");
    }
    return 0;
}

/***************************************************************************
 *        NAME: long record_hdlr()
 * DESCRIPTION: TDX_RECORD event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ***************************************************************************/
int record_hdlr()
{
   int	chdev = sr_getevtdev();
   // int	event = sr_getevttype();
   int	channum = get_channum( chdev );
   int	curstate;

   if ( channum == -1 ) {
      return( 0 );		/* Discard Message - Not for a Known Device */
   }

   curstate = dxinfox[ channum ].state;		/* Current State */

   close( dxinfox[ channum ].msg_fd );

   /*
    * If drop in loop current, set state to ONHOOK and
    * set channel to ONHOOK in case of ANALOG frontend.
    * In case of digital frontend, the sig_hdlr will take
    * care of it.
    */
   switch ( frontend ) {
   case CT_NTANALOG:
      if ( ATDX_TERMMSK( chdev ) & TM_LCOFF ) {
         dxinfox[ channum ].state = ST_ONHOOK;
         set_hkstate( channum, DX_ONHOOK );
         return( 0 );
      }
      break;

   case CT_NTT1:
      if ( ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev ) & DTSG_RCVA ) == 0 ) {
         return( 0 );
      }
      break;

   case CT_NTE1:
      if ( ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev ) & DTSG_RCVA ) != 0 ) {
         return( 0 );
      }
      break;
   }

   if ( curstate != ST_ONHOOK ) {
      dxinfox[ channum ].state = ST_GOODBYE;
      if ( play( channum, goodbyefd, 0, 0 ) == -1 ) {
         sprintf( tmpbuff, "Cannot Play Goodbye Message on channel %s",
		   ATDV_NAMEP( chdev ) );
         disp_status(channum, tmpbuff );
      }
   }

   return( 0 );
}


/***************************************************************************
 *        NAME: long getdig_hdlr()
 * DESCRIPTION: TDX_GETDIG event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ***************************************************************************/
int getdig_hdlr()
{
   int	chdev = sr_getevtdev();
   // int	event = sr_getevttype();
   int	channum = get_channum( chdev );
   int	curstate;
   int	errcode[MAXCHANS];
   
   errcode[channum] = 0;

   if ( channum == -1 ) {
      return( 0 );		/* Discard Message - Not for a Known Device */
   }

   curstate = dxinfox[ channum ].state;		/* Current State. Why the balls is this in the demo code? */

   /*
    * If drop in loop current, set state to ONHOOK and
    * set channel to ONHOOK in case of ANALOG frontend.
    * In case of digital frontend, the sig_hdlr will take
    * care of it.
    */
   switch ( frontend ) {
   case CT_NTANALOG:
      if ( ATDX_TERMMSK( chdev ) & TM_LCOFF ) {
         dxinfox[ channum ].state = ST_ONHOOK;
         set_hkstate( channum, DX_ONHOOK );
         return( 0 );
      }
      break;

   case CT_NTT1:
      if ( ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev ) & DTSG_RCVA ) == 0 ) {
         return( 0 );
      }
      break;

   case CT_NTE1:
      if ( ( ATDT_TSSGBIT( dxinfox[ channum ].tsdev ) & DTSG_RCVA ) != 0 ) {
         return( 0 );
      }
      break;
   case CT_NTTEST:
       if (fxo[channum] == 2) {
           if ( isdninfo[channum].status == 1) return(0);
           else break; // Don't run the switch case below plz
       }
	   /* This was re-written for B8ZS/ESF. It looks ugly as fuck, and should be done more efficiently. */
	   switch (ATDT_TSSGBIT(dxinfox[channum].tsdev)) {
           // Loop start handlers commented out.
	   case 0xA0:
	   case 0xA1:
	   case 0xA2:
	   case 0xA3:
	   case 0xA4:
	   case 0xA6:
	   case 0xA7:
	   case 0xA8:
	   case 0xA9:
	   case 0xAB:
	   case 0xAC:
	   case 0xAD:
	   case 0xAE:
	   case 0xAF:
       /* Here be E&M hax */
	   case 0xAA:
       case 0x08:
       case 0x09:
       case 0x0A:
       case 0x0B:
       case 0x0C:
       case 0x0D:
       case 0x0E:
       case 0x0F:
       
       if (fxo[channum] == 0) {
	       if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF ) == -1 ) {
		        sprintf( tmpbuff, "Cannot set CST events DM_DIGOFF for %s",
	            ATDV_NAMEP( dxinfox[ channum ].chdev ) );
    	        disp_status(channum, tmpbuff );
                disp_err( channum, dxinfox[ channum ].chdev, "dx_setevtmsk() error for DM_DIGOFF via playtone_hdlr" );
                printf("[DEBUG] current device state is: %li\n", ATDX_STATE( dxinfox[channum].chdev ));
		        sys_quit();
            } // end if 
       }
       
		   return(0);
	   default:
		   break;
	   }
	   /*
	   if ((ATDT_TSSGBIT(dxinfox[channum].tsdev) & 0xF0) == 0xA0) { // ESF/B8ZS
		   sprintf(tmpbuff, "DEBUG: getdig_hdlr stopped with tsbits in state %d", ATDT_TSSGBIT(dxinfox[channum].tsdev));
		   disp_msg(tmpbuff);
	   // if (ATDT_TSSGBIT(dxinfox[channum].tsdev) == 0x22) { // SF/B8ZS
		   return(0);
	   }
	   */
   }

   /*
    * Validate Digits for the Channel
    */
    
    switch( curstate ) {
   case ST_REORDER:
       disp_msg("DEBUG: Invoked reorder handler on digit processing halt");
       if (fxo[channum] == 0 ) {
           playtone_rep( channum, 480, 620, -24, -26, 25, 25, 40 );
       }
       
       else if (fxo[channum] == 1) {
           if (ownies[channum] == 1) {
	           dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // We're not removing loop current for FXO, just hanging the interface up. ESF/B8ZS
		       dxinfox[channum].state = ST_WTRING;
           }
           else {
               playtone_rep( channum, 480, 620, -24, -26, 25, 25, 40 );
               ownies[channum] = 1;
           }
       }
       
       else if (fxo[channum] == 2) {
            // Pass temporarily unavailable cause code to network and gtfo
            if (isdn_drop(channum, 41) == -1) {
                disp_msg("Holy shit! isdn_drop() failed! D:");
                return (-1);
            }
       }
	   return(0);
        
   case ST_FXSTEST1:
       disp_msg("DEBUG: Halted successfully on digit timer");
	   if ( dx_clrdigbuf( chdev ) == -1 ) {
            sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s", ATDV_NAMEP( chdev ) );
            disp_msg( tmpbuff );
					    }
       return 0;
        
   case ST_ISDNOUT:
   /*
       // This code is very temporary.
       if (strcmp("501033", dxinfox[channum].digbuf.dg_value) == 0) {
           connchan[channum] = 9;
	if (fxo[connchan[channum]] == 0x01) {
		sprintf(tmpbuff, "DEBUG: FXO signaling bits are 0x%lx", ATDT_TSSGBIT(dxinfox[channum].tsdev));
		disp_msg(tmpbuff);
		if ( ((ATDT_TSSGBIT(dxinfox[connchan[channum]].tsdev) & 0xF0) == 0xA0) && dxinfox[connchan[channum]].state == ST_WTRING) { // ESF/B8ZS
		// if ( (ATDT_TSSGBIT(dxinfox[connchan[channum]].tsdev) & 0x22) && dxinfox[connchan[channum]].state == ST_WTRING) { // SF/B8ZS
		// This presents a problem, since sometimes on incoming traffic, signaling bits return 0x22 (SF/B8ZS) from the channel bank
		// even when the unit is off-hook. This kinda sucks. Until this can be prevented in some way, we're doing a state
		// check here, so you don't just wind up taking over an in-progress call.
		connchan[connchan[channum]] = channum; // Indicate on the calling line that it'll be connecting to us.
		dxinfox[connchan[channum]].state = ST_FXOOUT;

		dxinfox[channum].state = ST_FXOOUT_S; // Update originating channel state to reflect outdial
		//dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BOFF); // Sets channel offhook (SF/B8ZS)
		dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BOFF | DTB_CON | DTB_DOFF); // Sets channel offhook (ESF/B8ZS)
        // dt_settssigsim(dxinfox[channum].tsdev, DTB_AON | DTB_BON | DTB_COFF | DTB_DON); // (Polarity reversal for originating channel, E&M hack)
        
        dx_dial(dxinfox[connchan[channum]].chdev, ",18324225379", NULL, EV_SYNC);
		if (nr_scunroute(dxinfox[channum].chdev, SC_VOX, dxinfox[channum].tsdev, SC_DTI, SC_FULLDUP) != 0) {
			disp_msg("GAH! SCBUS UNROUTING FUNCTION FAILED ON OUTGOING CALL!! BAIL OOOOUUUT!!!1");
			// The best thing we can really do here is... uhh, well, we should do something later.
			dxinfox[channum].state = ST_WTRING;
			dxinfox[connchan[channum]].state = ST_WTRING;
			return(-1);
		}

		if (nr_scunroute(dxinfox[connchan[channum]].chdev, SC_VOX, dxinfox[connchan[channum]].tsdev, SC_DTI, SC_FULLDUP) != 0) {
			// This is failing. Let's try and route these channels back to the SCBus devices and act as if nothing happened. Least we can do, right?
			nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP);
			nr_scroute(dxinfox[connchan[channum]].tsdev, SC_DTI, dxinfox[connchan[channum]].chdev, SC_VOX, SC_FULLDUP);
			dxinfox[channum].state = ST_WTRING;
			dxinfox[connchan[channum]].state = ST_WTRING;
			return(-1);
		}

		if (nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[connchan[channum]].tsdev, SC_DTI, SC_FULLDUP) != 0) {
			// This is failing. Let's try and route these channels back to the SCBus devices and act as if nothing happened. Least we can do, right?
			nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP);
			nr_scroute(dxinfox[connchan[channum]].tsdev, SC_DTI, dxinfox[connchan[channum]].chdev, SC_VOX, SC_FULLDUP);
			dxinfox[channum].state = ST_WTRING;
			dxinfox[connchan[channum]].state = ST_WTRING;
			return(-1);
		}
        dt_settssigsim(dxinfox[channum].tsdev, DTB_AON | DTB_BON | DTB_COFF | DTB_DON); // Set originating channel off-hook
		return(0);
	    }
       }
    }
    */
       if ( channum < 25 ) {
           if (modifier[channum] & 1) {
               isdn_origtest(channum, dxinfox[ channum ].digbuf.dg_value, "#" );
           }
           else {
               isdn_origtest(channum, dxinfox[ channum ].digbuf.dg_value, dntable[channum] );
           }
       }
       else {
           isdn_origtest(channum, dxinfox[ channum ].digbuf.dg_value, defaultcpn );
       }
        memset(dxinfox[channum].digbuf.dg_value, 0x00, DG_MAXDIGS);
       return(0);

   case ST_FXODISA:
	   if (strcmp(disapasscode, dxinfox[channum].digbuf.dg_value) == 0) {
		   if (dx_setevtmsk(dxinfox[channum].chdev, DM_DIGITS) == -1) {
			   sprintf(tmpbuff, "Cannot set CST events for %s",
				   ATDV_NAMEP(dxinfox[channum].chdev));
			   disp_status(channum, tmpbuff);
			   dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // ESF/B8ZS
			   // dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON); // SF/B8ZS
			   dxinfox[channum].state = ST_WTRING;
			   return(0);
		   }
		   dxinfox[channum].state = ST_FXSTEST1;
		   // Do basic initialization steps
		   // Y'know, you could just do memset...
		   memset(dxinfox[channum].digbuf.dg_value, 0x00, DG_MAXDIGS);
		   dignum[channum] = 0;
		   ownies[channum] = 0;
		   playtone_stutter(channum, 350, 440, -24, -26, 11, 11, 1);
		   return(0);
	   }
	   else {
           dxinfox[channum].state = ST_REORDER;
		   playtone_rep(channum, 480, 620, -25, -27, 25, 25, 40);
		   return(0);
	   }
   case ST_EMPLAY2:
   case ST_DYNPLAY:
	  if ( strcmp( "1", dxinfox[ channum ].digbuf.dg_value ) == 0) {
		  errcnt[ channum ] = 0;
		  anncnum[ channum ]--;
		  dx_adjsv( dxinfox[ channum ].chdev, SV_VOLUMETBL, SV_ABSPOS, 0 ); // Reset volume to normal
		  if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[channum]);
		  else sprintf( dxinfox[ channum ].msg_name, "sounds/emtanon/%d.pcm", anncnum[channum]);
		  errcode[channum] = dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
		  playoffset2[ channum ] = 0; //Initializing offset variable
	  /* Decrease the announcement number and play it */
	  if ( dx_clrdigbuf( chdev ) == -1 ) {
            sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s", ATDV_NAMEP( chdev ) );
            disp_msg( tmpbuff );
					    }
      if ( errcode[channum] != -1 ) {
          disp_msg( "Accessing dynamic sound player..." );
         errcode[channum] = play( channum, dxinfox[ channum ].msg_fd, 1, 0 );
      }
      if ( errcode[channum] == -1 ) {
          disp_msg( "Dynamic sound player error!" );
		 /* Attempt to resolve the error. */
		  if ( anncnum[ channum ] <= minannc[ channum ] ) {
			  anncnum[ channum ] = ( minannc[ channum ] + 1 );
			  if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[channum]);
			  else sprintf( dxinfox[ channum ].msg_name, "sounds/emtanon/%d.pcm", anncnum[channum]);
			  dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
			  errcode[channum] = play( channum, dxinfox[ channum ].msg_fd, 1, 0 );
		  }
	  }

   return ( errcode[channum] );
	  }
	  if ( strcmp( "3", dxinfox[ channum ].digbuf.dg_value ) == 0) {
		  errcnt[ channum ] = 0;
		  anncnum[ channum ]++;
		  dx_adjsv( dxinfox[ channum ].chdev, SV_VOLUMETBL, SV_ABSPOS, 0 ); // Reset volume to normal
		  if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[channum]);
		  else sprintf( dxinfox[ channum ].msg_name, "sounds/emtanon/%d.pcm", anncnum[channum]);
		  errcode[channum] = dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
		  playoffset2[ channum ] = 0; //Initializing offset variable
		  sprintf( tmpbuff, "Announcement number is %d, error code is %d", anncnum[channum], errcode[channum] );
		  disp_msg ( tmpbuff );
	  if ( dx_clrdigbuf( chdev ) == -1 ) {
            sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s", ATDV_NAMEP( chdev ) );
            disp_msg( tmpbuff );
					    }
      if ( errcode[channum] != -1 ) {
         errcode[channum] = play( channum, dxinfox[ channum ].msg_fd, 1, 0 );
      }
	  if ( errcode[channum] == -1 ) {
         disp_msg( "Dynamic sound player error!" );
		 /* Attempt to resolve the error. */
		 if ( anncnum[ channum ] >= maxannc[ channum ] ) {
			 anncnum[ channum ] = ( maxannc[ channum ] - 1 );
			 if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[channum]);
			 else sprintf( dxinfox[ channum ].msg_name, "sounds/emtanon/%d.pcm", anncnum[channum]);
			 dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
			 errcode[channum] = play( channum, dxinfox[ channum ].msg_fd, 1, 0 );
			 if (errcode[ channum ] == -1) disp_msg("Your shit failed to play. Or something. Why is anyone's guess." );
		  }
	  }
   return ( errcode[channum] );
	  }
      
	  if ( strcmp( "4", dxinfox[ channum ].digbuf.dg_value ) == 0) {
		  errcnt[ channum ] = 0;
		  if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[channum]);
		  else sprintf( dxinfox[ channum ].msg_name, "sounds/emtanon/%d.pcm", anncnum[channum]);
		  errcode[channum] = dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
		  sprintf( tmpbuff, "Announcement number is %d, error code is %d", anncnum[channum], errcode[channum] );
		  disp_msg ( tmpbuff );
	  if ( dx_clrdigbuf( chdev ) == -1 ) {
            sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s", ATDV_NAMEP( chdev ) );
            disp_msg( tmpbuff );
					    }
      if ( errcode[channum] != -1 ) {
		 playoffset[ channum ] = ( playoffset[channum] + playoffset2[ channum ] - 56000 ); // 8000 bytes per sec * 7 = 56,000
		 currentlength[ channum ] = lseek( dxinfox[channum].msg_fd, 0, SEEK_END );
		 if ( playoffset[ channum ] > currentlength[ channum ] ) playoffset[ channum ] = 0;
		 playoffset2[ channum ] = playoffset[ channum ]; // Comment this shit better!
         errcode[channum] = play( channum, dxinfox[ channum ].msg_fd, 1, playoffset[ channum ] );
      }
	  if ( errcode[channum] == -1 ) {
		 sprintf( tmpbuff, "Dynamic sound player error! Offset is %lu", playoffset[ channum ] );
         disp_msg( tmpbuff );
		  }
      
   return ( errcode[channum] );
	  }
      
      // This code isn't really needed; all the Emtanon garbage isn't in the SR5 codebase yet. Uncomment as necessary.
      
      /*
      
      if (( strcmp( "5", dxinfox[ channum ].digbuf.dg_value ) == 0) && ( dxinfox[ channum ].state == ST_EMPLAY2 )) { 
       // Repeat the stuff from 2109 processing for a main menu return.
       anncnum[ channum ] = 0;
	   errcnt[ channum ] = 0;
	   dxinfox[ channum ].state = ST_EMPLAY1;
	   playoffset2[ channum ] = 0;
	   
       if (ownies[channum] != 100) {
       
       if ( dx_blddt( TID_1, 1880, 15, 697, 15, TN_TRAILING) == -1 ) 
	   {
           disp_msg( "Shit we couldn't build the Chucktone!" );
       }
       
       if ( dx_addtone( chdev, 'E', DG_USER1 ) == -1 ) {
           sprintf(tmpbuff, "Unable to add Chucktone. Error %s", ATDV_ERRMSGP( chdev ));
           disp_msg(tmpbuff);
       }
       
	   if ( dx_enbtone( chdev, TID_1, DM_TONEON ) == -1 )
	   {
		   sprintf(tmpbuff, "Unable to enable Chucktone.");
           disp_msg(tmpbuff);
	   }
       
       ownies[channum] = 100;
       
       }
       
       // If the Chucktone doesn't work, just keep going.
       
	   sprintf( dxinfox[ channum ].msg_name, "sounds\\emtanon\\em_greeting.vox" );
	   dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
	   errcode[channum] = play( channum, dxinfox[ channum ].msg_fd, 0, 0 );
	   return( errcode[channum] );
      }
      
      */
	  
	  if ( strcmp( "6", dxinfox[ channum ].digbuf.dg_value ) == 0) {
		  errcnt[ channum ] = 0;
		  if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[channum]);
		  else sprintf( dxinfox[ channum ].msg_name, "sounds/emtanon/%d.pcm", anncnum[channum]);
		  errcode[channum] = dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
	  if ( dx_clrdigbuf( chdev ) == -1 ) {
            sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s", ATDV_NAMEP( chdev ) );
            disp_msg( tmpbuff );
					    }
      if ( errcode[channum] != -1 ) {
		  // For the NT/DOS builds, the play_hdlr has already invoked ATDX_TRCOUNT
		 // playoffset[ channum ] = ATDX_TRCOUNT( chdev ); // 8000 bytes per sec * 7 = 56,000
		 currentlength[ channum ] = lseek( dxinfox[channum].msg_fd, 0, SEEK_END );
		 playoffset[ channum ] = ( playoffset[ channum ] + 56000 + playoffset2[ channum ]);
		 playoffset2[ channum ] = playoffset[ channum ]; // Comment this shit better!
		 if ( playoffset[ channum ] > currentlength[ channum ] ) {
            playoffset2[ channum ] = 0;
			playoffset[ channum ] = 0;
		    anncnum[ channum ]++;
			dx_adjsv( dxinfox[ channum ].chdev, SV_VOLUMETBL, SV_ABSPOS, 0 ); // Reset volume to normal
		    if ( anncnum[ channum ] >= maxannc[ channum ] ) {
			   anncnum[ channum ] = ( maxannc[ channum ] - 1 );
			   }
		    if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[channum]);
			else {
				close( dxinfox[ channum ].msg_fd );
				ownies[ channum ] = 1;
			dx_adjsv( dxinfox[ channum ].chdev, SV_VOLUMETBL, SV_ABSPOS, 0 ); // Reset volume to normal
			if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/ivr_betabackfwd.pcm" );
			else sprintf( dxinfox[ channum ].msg_name, "sounds/ivr_embackfwd.pcm" );
			dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
			playoffset[ channum ] = 0;
			}
		    }
		 }
         errcode[channum] = play( channum, dxinfox[ channum ].msg_fd, 1, playoffset[ channum ] );
      
	  if ( errcode[channum] == -1 ) {
         disp_msg( "Dynamic sound player error!" );
		  }
	  
   return ( errcode[channum] );
	  }
	  if ( strcmp( "7", dxinfox[ channum ].digbuf.dg_value ) == 0) {
		  errcnt[ channum ] = 0;
	  if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[channum]);
	  else sprintf( dxinfox[ channum ].msg_name, "sounds/emtanon/%d.pcm", anncnum[channum]);
	  errcode[channum] = dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
	  sprintf( tmpbuff, "Announcement number is %d, error code is %d", anncnum[channum], errcode[channum] );
	  disp_msg ( tmpbuff );
		if ( dx_clrdigbuf( chdev ) == -1 ) {
            sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s", ATDV_NAMEP( chdev ) );
            disp_msg( tmpbuff );
					    }
		if ( errcode[channum] != -1 ) {
			playoffset[ channum ] = ( playoffset[channum] + playoffset2[ channum ] ); // Make sure these first three lines are actually needed. I was super tired when I did this.
			currentlength[ channum ] = lseek( dxinfox[channum].msg_fd, 0, SEEK_END );
			playoffset2[ channum ] = playoffset[ channum ];
			errcode[channum] = dx_adjsv( dxinfox[ channum ].chdev, SV_VOLUMETBL, SV_RELCURPOS, -2 );
			play( channum, dxinfox[ channum ].msg_fd, 1, playoffset[ channum ] );
      }
		if ( errcode[channum] == -1 ) {
			disp_msg( "Dynamic sound player error in volume adjust function" );
		  }
		  
   return ( errcode[channum] );
	  }
	  if ( strcmp( "8", dxinfox[ channum ].digbuf.dg_value ) == 0) {
		  errcnt[ channum ] = 0;
	  if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[channum]);
	  else sprintf( dxinfox[ channum ].msg_name, "sounds/emtanon/%d.pcm", anncnum[channum]);
	  errcode[channum] = dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
	  sprintf( tmpbuff, "Announcement number is %d, error code is %d", anncnum[channum], errcode[channum] );
	  disp_msg ( tmpbuff );
		if ( dx_clrdigbuf( chdev ) == -1 ) {
            sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s", ATDV_NAMEP( chdev ) );
            disp_msg( tmpbuff );
					    }
		if ( errcode[channum] != -1 ) {
			playoffset[ channum ] = ( playoffset[channum] + playoffset2[ channum ] ); // Make sure these first three lines are actually needed. I was super tired when I did this.
			currentlength[ channum ] = lseek( dxinfox[channum].msg_fd, 0, SEEK_END );
			playoffset2[ channum ] = playoffset[ channum ];
			errcode[channum] = dx_adjsv( dxinfox[ channum ].chdev, SV_VOLUMETBL, SV_ABSPOS, 0 ); // Reset volume to normal
			play( channum, dxinfox[ channum ].msg_fd, 1, playoffset[ channum ] );
      }
		if ( errcode[channum] == -1 ) {
			disp_msg( "Dynamic sound player error in volume adjust function" );
		  }
	  
   return ( errcode[channum] );
	  }
	  
 	  if ( strcmp( "9", dxinfox[ channum ].digbuf.dg_value ) == 0) {
		  errcnt[ channum ] = 0;
	  if ( dxinfox[ channum ].state == ST_DYNPLAY ) sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[channum]);
	  else sprintf( dxinfox[ channum ].msg_name, "sounds/emtanon/%d.pcm", anncnum[channum]);
	  errcode[channum] = dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
	  sprintf( tmpbuff, "Announcement number is %d, error code is %d", anncnum[channum], errcode[channum] );
	  disp_msg ( tmpbuff );
		if ( dx_clrdigbuf( chdev ) == -1 ) {
            sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s", ATDV_NAMEP( chdev ) );
            disp_msg( tmpbuff );
					    }
		if ( errcode[channum] != -1 ) {
			playoffset[ channum ] = ( playoffset[channum] + playoffset2[ channum ] ); // Make sure these first three lines are actually needed. I was super tired when I did this.
			currentlength[ channum ] = lseek( dxinfox[channum].msg_fd, 0, SEEK_END );
			playoffset2[ channum ] = playoffset[ channum ];
			errcode[channum] = dx_adjsv( dxinfox[ channum ].chdev, SV_VOLUMETBL, SV_RELCURPOS, 2 );
			play( channum, dxinfox[ channum ].msg_fd, 1, playoffset[ channum ] );
      }
		if ( errcode[channum] == -1 ) {
			disp_msg( "Dynamic sound player error in volume adjust function" );
		  }
	  
   return ( errcode[channum] );
	  }
      
      // A lot of these functions aren't in the SR5 codebase yet.
      
      /*
      
     if (( strcmp( "E", dxinfox[ channum ].digbuf.dg_value ) == 0 ) && (ownies[channum] == 100)  && (anncnum[channum] == 3) && (dxinfox[ channum ].state == ST_EMPLAY2) && (frontend == CT_GCISDN) ) {
        // Call origination test function
        ownies[ channum ] = 0;
        errcnt[ channum ] = 0;
        anncnum[ channum ] = 0;
        ownies[ channum ] = 0;
        connchan[ channum ] = 1;
        if ( dx_clrdigbuf( chdev ) == -1 ) {
           sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s", ATDV_NAMEP( chdev ) );
           disp_msg( tmpbuff );
        }
        dx_distone( chdev, TID_1, DM_TONEON ); // Remove custom tone
        dx_deltones( chdev ); // Remove all custom tones
        while (dxinfox[connchan[channum]].state > ST_WTRING ) connchan[channum]++;
        sprintf( tmpbuff, "Dest. channel is %d", connchan[channum] );
        disp_msg(tmpbuff);
        
        if (connchan[ channum ] > 23 ) { // Error handling for all circuits being busy
        
        // To-do: make this play an actual error message
        
        disp_msg("Error: all circuits are busy");
        return(-1);
        }
        connchan[connchan[channum]] = channum;
        makecall( connchan[channum], "1174", "2109\0", FALSE ); // Call 1174 on channel 2
        dxinfox[ channum ].state = ST_ROUTED;
        return(0);
     }
     
     */

	  
	  else { // This else statement seems to apply to everything; not just case ST_DYNPLAY
	  disp_msg( "Crap, we're hitting the else statement at the bottom when we shouldn't." );
		  dx_adjsv( dxinfox[ channum ].chdev, SV_VOLUMETBL, SV_ABSPOS, 0 );
		  errcnt[ channum ]++;
		  if ( errcnt[ channum ] > 2 ) {
			  dxinfox[ channum ].state = ST_INVALID;
			  errcnt[ channum ] = 0;
		  }
		  else dxinfox[ channum ].state = ST_DYNPLAYE;
      if ( play( channum, invalidfd, 0, 0 ) == -1 ) {
         sprintf( tmpbuff, "Cannot Play Invalid Message on channel %s",
                ATDV_NAMEP( chdev ) );
         disp_msg( tmpbuff );
      }

   return ( 0 );
	  }
      
    default:
    

   if ( strcmp( "2111", dxinfox[ channum ].digbuf.dg_value ) == 0) {
       // There is no wink-start code, so this shouldn't be necessary
       /*
	   if (altsig && 1) {
		set_hkstate( channum, DX_OFFHOOK );
		 }
       */
      dxinfox[ channum ].state = ST_DYNPLAY;
	  ownies[ channum ] = 0; // Initialize variables
	  playoffset2[ channum ] = 0;
	  anncnum[ channum ] = 2000;
	  errcnt[ channum ] = 1;
	  while ( errcnt[ channum ] == 1 )
	 {
	 sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
	 if (stat( dxinfox[ channum ].msg_name, &sts ) == -1) errcnt[ channum ] = 0;
	 else anncnum[ channum ]++;
	 }
	 
	 maxannc[ channum ] = ( anncnum[ channum ] );
	 anncnum[ channum ] = 2000;
	 errcnt[ channum ] = 1;
	 
	  while ( errcnt[ channum ] == 1 )
	{
	 sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
	 if (stat( dxinfox[ channum ].msg_name, &sts ) == -1) errcnt[ channum ] = 0;
	 else anncnum[ channum ]--;
	}


   	 minannc[ channum ] = ( anncnum[ channum ] );
	 sprintf( tmpbuff, "maxannc is %d.", maxannc[ channum ] );
	 disp_msg( tmpbuff );

	  anncnum[ channum ] = 2000;
	  sprintf( dxinfox[ channum ].msg_name, "sounds/%d.pcm", anncnum[ channum ]);
      dxinfox[ channum ].msg_fd = open( dxinfox[ channum ].msg_name, O_RDONLY );
      if ( dxinfox[ channum ].msg_fd == -1 ) {
         sprintf( tmpbuff, "Cannot open %s for play-back",
                dxinfox[ channum ].msg_name );
         disp_msg( tmpbuff );
         errcode[channum] = -1;
					   }

      if ( errcode[channum] == 0 ) {
         errcode[channum] = play( channum, dxinfox[ channum ].msg_fd, 1, 0 );
			  }
   return( errcode[channum] );
								  }

   if ( strcmp( dxinfox[ channum ].ac_code, dxinfox[ channum ].digbuf.dg_value )){
      dxinfox[ channum ].state = ST_INVALID;

      if ( play( channum, invalidfd, 0, 0 ) == -1 ) {
	    sprintf( tmpbuff, "Cannot Play Invalid Message on channel %s",
		    ATDV_NAMEP( chdev ) );
	    disp_status(channum, tmpbuff );
      }
   } 

   
   else {
      dxinfox[ channum ].state = ST_PLAY;

	  
      dxinfox[ channum ].msg_fd = open( dxinfox[channum].msg_name, O_RDONLY);
      if ( dxinfox[ channum ].msg_fd == -1 ) {
	    errcode[channum] = -1;
      }
      if ( errcode[channum] == 0 ) {
	    if ( play( channum, dxinfox[ channum ].msg_fd, 0, 0 ) == -1) {
            sprintf( tmpbuff, "Cannot Play Recorded Message on channel %s",
		        ATDV_NAMEP( chdev ) );
	        disp_status(channum, tmpbuff );
        }
      }

      // Msg file has not yet been recorded, lets play the goodbye file
      else {
            dxinfox[ channum ].state = ST_GOODBYE;

            if ( play( channum, goodbyefd, 0, 0 ) == -1 ) {
	            sprintf( tmpbuff, "Cannot Play Goodbye Message on channel %s",
		            ATDV_NAMEP( chdev ) );
	            disp_status(channum, tmpbuff );
            }
        }

      
   }
   
    }

   return( 0 );
}


/***************************************************************************
 *        NAME: long sethook_hdlr()
 * DESCRIPTION: TDX_SETHOOK event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ***************************************************************************/
int sethook_hdlr()
{
   int		chdev = sr_getevtdev();
   // int		event = sr_getevttype();
   int		channum = get_channum( chdev );
   int		curstate;
   DX_CST	*cstp;

   if ( channum == -1 ) {
      return( 0 );		/* Discard Message - Not for a Known Device */
   }

   curstate = dxinfox[ channum ].state;		/* Current State */
   cstp = (DX_CST *) sr_getevtdatap();

   switch ( cstp->cst_event ) {
   case DX_ONHOOK:
      dxinfox[ channum ].state = ST_WTRING;

      if ( dx_clrdigbuf( chdev ) == -1 ) {
	 sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
		ATDV_NAMEP( chdev ) );
	 
	 disp_err(channum, dxinfox[ channum ].chdev,tmpbuff);
      }

      sprintf(tmpbuff,"Ready to Accept call - Access number %s - Sethook Handler", dxinfox[ channum ].ac_code );
	  disp_status(channum, tmpbuff);
      
      break;

   case DX_OFFHOOK:
      // A call continues here. After the play command, it goes to case ST_INTRO in play_hdlr
      dxinfox[ channum ].state = ST_INTRO;

      if ( play( channum, introfd, 0, 0 ) == -1 ) {
	 sprintf( tmpbuff, "Error playing Introduction on channel %s",
		ATDV_NAMEP( chdev ) );
	 disp_status(channum, tmpbuff );
      }
      break;
   }

   return( 0 );
}


/***************************************************************************
 *        NAME: long error_hdlr()
 * DESCRIPTION: TDX_ERROR event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ***************************************************************************/
int error_hdlr()
{
   int chdev = sr_getevtdev();
   // int event = sr_getevttype();
   int channum = get_channum( chdev );
   int curstate;

   if ( channum == -1 ) {
      return( 0 );		/* Discard Message - Not for a Known Device */
   }

   curstate = dxinfox[ channum ].state;		/* Current State */

   /*
    * Print a message
    */
   sprintf( tmpbuff, "Received an ERROR Event for %s: %s", ATDV_NAMEP( chdev ), ATDV_ERRMSGP( chdev ) );
   disp_status(channum, tmpbuff );

   /*
    * Put state into ST_ERROR
    */
   dxinfox[ channum ].state = ST_ERROR;
   set_hkstate( channum, DX_ONHOOK );

   return( 0 );
}


/***************************************************************************
 *        NAME: int fallback_hdlr()
 * DESCRIPTION: Fall-Back event handler
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ***************************************************************************/
int fallback_hdlr(unsigned long event_handle)
{
   int chtsdev = sr_getevtdev();
   int event = sr_getevttype();
   int channum = get_channum( chtsdev );
   int curstate;

   if ( channum == -1 ) {
      return( 0 );		/* Discard Message - Not for a Known Device */
   }

   curstate = dxinfox[ channum ].state;		/* Current State */

   /*
    * Print a message
    */
   sprintf( tmpbuff, "Unknown event %d for device %s", event,
	ATDV_NAMEP( chtsdev ) );
   disp_status(channum, tmpbuff );

   /*
    * If a fallback handler is called put the state in ST_ERROR
    */
   dxinfox[ channum ].state = ST_ERROR;
   // Don't run this on ISDN connections.
   if (fxo[channum] != 2) set_hkstate( channum, DX_ONHOOK );

   return( 0 );
}


/******************************************************************************
 *        NAME: int sig_hdlr()
 * DESCRIPTION: Signal handler to catch DTEV_SIG events generated by the dti
 *              timeslots.
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ******************************************************************************/
int sig_hdlr()
{
   int tsdev = sr_getevtdev();
   int event = sr_getevttype();
   unsigned short *ev_datap = (unsigned short *)sr_getevtdatap();
   unsigned short sig = (unsigned short)(*ev_datap);
   int channum = get_channum( tsdev );
   int curstate;
   short indx;


   if ( channum == -1 ) {
      return( 0 );		/* Discard Message - Not for a Known Device */
   }

   if ( frontend == CT_NTTEST ) {
       if (fxo[channum] == 2) return (0); // ISDN calls shouldn't go here. Ever.
	   long sigbits;
	   sigbits = ATDT_TSSGBIT(dxinfox[channum].tsdev);
	   sprintf( tmpbuff, "Signaling bits changed to 0x%lx, state %d, channel %d", sigbits, dxinfox[ channum ].state, channum );
	   disp_status(channum,tmpbuff);
	   disp_msg(tmpbuff );

	   switch (sigbits)
	   {
	   case 0xAF:
	   // case 0xA3:   // TO DO: ESF/B8ZS
       // case 0x23: // SF/B8ZS
           // Error fix
		   if (fxo[channum] != 0) return(0); // We shouldn't be running this for FXO.
		   // dxinfox[channum].state = ST_PERMSIG_LD;
		   ownies[channum] = 3;
		   // This state is special; if the line is still on-hook a couple seconds after loop cutoff, remove tone - the channel bank doesn't give us any status updates to indicate battery is restored, but line is still on-hook
		   dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGOFF );
		   dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // ESF/B8ZS
           // dt_settssigsim( dxinfox[channum].tsdev, DTB_AOFF | DTB_BON ); // SF/B8ZS
           if (ATDX_STATE(dxinfox[channum].chdev) != CS_DIAL ) playtone_cad( channum, 480, 0, 250 ); // Check to see if we're in the pause state before doing this. Otherwise, we'll do it in the dial handler.
           return(0);
        
	   // case 0x30: // SF/B8ZS
		   // Offhook
		   // This is an incoming FXO call. Get rid of the ringing voltage.
		   dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // Just in case we're applying ringing voltage or something, make sure we stop. ESF/B8ZS
		   // dt_settssigsim( dxinfox[channum].tsdev, DTB_AOFF | DTB_BON ); // Just in case we're applying ringing voltage or something, make sure we stop. SF/B8ZS
	   case 0xFA: // ESF/B8ZS (use OR)
	   // case 0xF5: // This might belong elsewhere - check.
       // E&M guess states
       case 0xFF:
       case 0xFE:
       case 0xFD:
       case 0xFB:
       case 0xFC:
       case 0xF9:
       case 0xF8:
       
	   case 0xF2: // ESF/B8ZS
	   case 0xF0: // ESF/B8ZS
	   // case 0x32: // SF/B8ZS
		   switch( dxinfox[channum].state ) {

		   case ST_PERMSIG:
			   return(0);

		   case ST_INCOMING:
			   // Connect the call here.

			   // ...but check first; did the calling party go back on-hook and are we in a glare condition? Lose that shit.
			   // But only if this is FXS -> FXS. Otherwise, ignore; FXO will never be in the state 0x32.

			   if (!(ATDT_TSSGBIT(dxinfox[connchan[channum]].tsdev) & 0xF2) && (fxo[connchan[channum]] == 0)) { // ESF/B8ZS
			   //if ( ( ATDT_TSSGBIT( dxinfox[ connchan[ channum ] ].tsdev )  != 0x32 ) && (fxo[connchan[channum]] == 0) ) { // SF/B8ZS
				   // dxinfox[ channum ].state = ST_WTRING; // This actually creates a bug, so lets stop for now.
				   // connchan[ channum ] = 0; // This is unnecessary; the originating channel will get rid of this for us.
				   break;
			   }

			   else {
				   ownies[ channum ] = 0;
				   dx_stopch( dxinfox[ connchan[ channum ] ].chdev, EV_ASYNC ); // Stop if ringback tone is being applied
				   //dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON); // Bugfix. Stop the ringback voltage if _that_ is being applied.

				   if (fxo[connchan[channum]] == 0) {
					   sprintf(tmpbuff, "DEBUG: Polarity reversal on channel %d!", connchan[channum]);
					   disp_msg(tmpbuff);

					   if (dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BON | DTB_COFF | DTB_DON) != 0) disp_msg("ERROR: Polarity Reversal"); // Test. Reverse polarity for originating end.
				   }
                   else if ( fxo[connchan[channum]] == 2 ) {
                       // Signal to the ISDN network that the call is connected
                       isdn_hkstate(connchan[channum], DX_OFFHOOK);
                   }
				   dxinfox[ channum ].state = ST_ROUTED;
				   dxinfox[ connchan[ channum ] ].state = ST_ROUTED;
				   // dt_settssigsim( dxinfox[channum].tsdev, DTB_AOFF | DTB_BON ); // Just in case we're applying ringing voltage or something, make sure we stop.

				   if (nr_scunroute( dxinfox[ channum ].chdev, SC_VOX, dxinfox[ channum ].tsdev, SC_DTI, SC_FULLDUP ) != 0 ) {
					   disp_msg("GAH! SCBUS UNROUTING FUNCTION FAILED ON INCOMING CALL!! BAIL OOOOUUUT!!!1");
					   // The best thing we can really do here is... uhh, well, we should do something later.
					   dxinfox[ channum ].state = ST_WTRING;
					   dxinfox[ connchan[ channum ] ].state = ST_WTRING;
					   return(-1);
				   }

				   if (nr_scunroute( dxinfox[ connchan[ channum ] ].chdev, SC_VOX, dxinfox[ connchan[ channum ] ].tsdev, SC_DTI, SC_FULLDUP ) != 0 ) {
					   // This is failing. Let's try and route these channels back to the SCBus devices and act as if nothing happened. Least we can do, right?
					   nr_scroute( dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP );
					   nr_scroute( dxinfox[ connchan[ channum ] ].tsdev, SC_DTI, dxinfox[ connchan[ channum ] ].chdev, SC_VOX, SC_FULLDUP );
					   dxinfox[ channum ].state = ST_WTRING;
					   dxinfox[ connchan[ channum ] ].state = ST_WTRING;
					   return(-1);
				   }

				   if (nr_scroute( dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ connchan[ channum ] ].tsdev, SC_DTI, SC_FULLDUP ) != 0 ) {
					   // This is failing. Let's try and route these channels back to the SCBus devices and act as if nothing happened. Least we can do, right?
					   nr_scroute( dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP );
					   nr_scroute( dxinfox[ connchan[ channum ] ].tsdev, SC_DTI, dxinfox[ connchan[ channum ] ].chdev, SC_VOX, SC_FULLDUP );
					   dxinfox[ channum ].state = ST_WTRING;
					   dxinfox[ connchan[ channum ] ].state = ST_WTRING;
					   return(-1);
				   }
				   return(0);
			   }
		   case ST_ROUTED:
			   return(0); // Bugfix; is a call still connected? Don't execute the default code below.
                   case ST_ROUTEDISDN:
                   // Bugfix: don't have it do any of that stupid shit below! Falling into ST_FXSTEST1 makes it fuxx0r itself.
                           printf("DEBUG: Executing ST_ROUTEDISDN bugfix. Remove this message when you know it's not a problem.\n");
			   return 0;
		   default:
		   // To do: set least significant bit off
		// Clear digits. Is there a better way to do this? Could just memset...
		dxinfox[ channum ].digbuf.dg_value[0] = '\0';
		dxinfox[ channum ].digbuf.dg_value[1] = '\0';
		dxinfox[ channum ].digbuf.dg_value[2] = '\0';
        dxinfox[ channum ].digbuf.dg_value[3] = '\0';
		dignum[channum] = 0;
		ownies[channum] = 69;
		   // set_hkstate( channum, DX_OFFHOOK );
					   sprintf( tmpbuff, "Off-hook status in state %d", dxinfox[channum].state );
			   disp_status(channum, tmpbuff );
		   dxinfox[channum].state = ST_FXSTEST1;
	       // sprintf( dxinfox[ channum ].msg_name, "sounds\\test.pcm");
	       dx_setdigtyp( dxinfox[channum].chdev, D_DTMF | D_LPD );
	       // dxinfox[ channum ].msg_fd = open( "sounds\\test.pcm", O_RDONLY );
	       // play( channum, dxinfox[ channum ].msg_fd, 1, 0 );
	       // This'll be our dialtone for the moment.
	       // Don't forget to build the tone event handler for ringback event timing. Or something.
               dx_stopch( dxinfox[channum].chdev, (EV_ASYNC | EV_NOSTOP) );
               // The handlers for this dx_stopch are responsible for giving you dialtone now.
               /*
	    if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_DIGITS ) == -1 ) {
		     sprintf( tmpbuff, "Cannot set CST events DM_DIGITS for %s",
	    	 ATDV_NAMEP( dxinfox[ channum ].chdev ) );
    		 disp_status(channum, tmpbuff );
		    sys_quit();
            } // end if 
            */
	       // playtone( channum, 400, 0, 2000 );
	       return(0);

		   }
       
       // Just a guess; tell me if I'm wrong.
       case 0x09:
       case 0x08:
       case 0x0B:
       case 0x0A:
       case 0x0C:
       case 0x0D:
       case 0x0E:
       case 0x0F:
       
       modifier[channum] = 0;
       if( fxo[channum] != 0) {
		   sprintf(tmpbuff, "DEBUG: FXO ringing state entered on channel %d", channum);
		   disp_msg(tmpbuff);
		   dxinfox[channum].state = ST_FXORING;
		   //time(&hosttimer[channum]);
           clock_gettime( CLOCK_REALTIME, &hosttimer[channum] );
		   return(0);
       }
       else if (ownies[channum] == 2) {
           dxinfox[connchan[channum]].state = ST_WTRING;
           // Reset the calling/called party channel connection table
           dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // Turn the ringing voltage off plz (E&M hack)
           connchan[channum] = 0;
           connchan[connchan[channum]] = 0;
           ownies[channum] = 0;
       }

       
       // For loop start, this is largely just used for FXO
       case 0xA8:
	   case 0xA0:
	   case 0xAA: // On-hook - ESF/B8ZS - do an OR
	   case 0xA2: // On-hook - ESF/B8ZS
	   // case 0x22: // On-hook - SF/B8ZS
		   // To do: for FXS hangups, we may need to have proper handling here.
		   switch ( dxinfox[ channum ].state ) {
               
           case ST_ROUTEDISDN:
               isdn_hkstate(connchan[channum], DX_ONHOOK);
			   if (nr_scunroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[connchan[channum]].tsdev, SC_DTI, SC_FULLDUP) != 0) {
				   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really should look into this...", channum, connchan[channum]);
				   // This'll halt execution, so we should only do it in places we really need it.
				   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
				   // Proceed with the call teardown procedure anyway. Not much else we can do, right?
			   }
			   dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // Unsets reversed polarity for originating line. E&M hack.
			   // dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BON); // Sets FXO channel on-hook. SF/B8ZS
			   if (nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP) != 0) {
				   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really, *really* should look into this...", channum, connchan[channum]);
				   // This'll halt execution, so we should only do it in places we really need it.
				   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
			   }
			   if (nr_scroute(dxinfox[connchan[channum]].tsdev, SC_DTI, dxinfox[connchan[channum]].chdev, SC_VOX, SC_FULLDUP) != 0) {
				   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really, *really*, *REALLY* should look into this...", channum, connchan[channum]);
				   // This'll halt execution, so we should only do it in places we really need it.
				   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
			   }
			   dxinfox[channum].state = ST_WTRING;

			   // Reset line assignment variables.
			   connchan[channum] = 0;
			   connchan[connchan[channum]] = 0;
               return(0);               

		   case ST_FXORING:
               
			   clock_gettime( CLOCK_REALTIME, &hosttimer2[channum] );
               /*
               sprintf(tmpbuff, "DEBUG: Ring timer returning value %ld", (time(NULL) - hosttimer[channum]) );
               disp_msg(tmpbuff);
               */
               hosttimer[channum].tv_sec++;
               hosttimer[channum].tv_nsec += 500000000; // Add 500 milliseconds to amount; if we're getting the same second value, is it at least over 500 milliseconds?
			   if ( (hosttimer[channum].tv_sec < hosttimer2[channum].tv_sec) || ( ( hosttimer[channum].tv_sec == hosttimer2[channum].tv_sec) && (hosttimer[channum].tv_nsec < hosttimer2[channum].tv_nsec) ) ) {
				   dx_setevtmsk(dxinfox[channum].chdev, DM_DIGOFF); // Just to make sure, turn off the event mask for digits
				   dt_settssigsim(dxinfox[channum].tsdev, DTB_AON | DTB_BOFF | DTB_CON | DTB_DOFF ); // Let's answer this hatbowl! Moar traffics in moar places. ESF/B8ZS
				   // dt_settssigsim(dxinfox[channum].tsdev, DTB_AON | DTB_BOFF); // Let's answer this hatbowl! Moar traffics in moar places. SF/B8ZS
				   playtone(channum, 400, 0, 2000);
				   dxinfox[channum].state = ST_FXODISA;
                   hosttimer[channum].tv_sec = 0;
                   hosttimer2[channum].tv_sec = 0;
				   return(0);
			   }
			   hosttimer[channum].tv_sec = 0; // Reset the variable for now.
               hosttimer2[channum].tv_sec = 0;
			   dxinfox[channum].state = ST_WTRING;
			   return(0);

		   case ST_FXOOUT_S:
			   // This is mostly a copy of the FXO side of the disconnect code, but without the code to send the outgoing FXS to permanent signal
			   // This shouldn't be default. To do: change!
			   sprintf(tmpbuff, "DEBUG: FXO loop current drop state entered on channel %d", channum);
			   disp_msg(tmpbuff);
			   // This is copied and pasted from that other shit for the moment. You should also account for non-connected call states.
			   dxinfox[connchan[channum]].state = ST_WTRING; // Did this other loser not hang up? They're hitting the permanent signal condition.
			   if (nr_scunroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[connchan[channum]].tsdev, SC_DTI, SC_FULLDUP) != 0) {
				   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really should look into this...", channum, connchan[channum]);
				   // This'll halt execution, so we should only do it in places we really need it.
				   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
				   // Proceed with the call teardown procedure anyway. Not much else we can do, right?
			   }
			   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // Sets FXO channel on-hook. ESF/B8ZS
			   dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // Unsets reversed polarity for originating line. E&M hack.
			   // dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BON); // Sets FXO channel on-hook. SF/B8ZS
			   if (nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP) != 0) {
				   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really, *really* should look into this...", channum, connchan[channum]);
				   // This'll halt execution, so we should only do it in places we really need it.
				   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
			   }
			   if (nr_scroute(dxinfox[connchan[channum]].tsdev, SC_DTI, dxinfox[connchan[channum]].chdev, SC_VOX, SC_FULLDUP) != 0) {
				   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really, *really*, *REALLY* should look into this...", channum, connchan[channum]);
				   // This'll halt execution, so we should only do it in places we really need it.
				   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
			   }
			   dxinfox[channum].state = ST_WTRING;

			   // Reset line assignment variables.
			   connchan[channum] = 0;
			   connchan[connchan[channum]] = 0;
			   // We are at ground zero.
			   return(0);

		   case ST_ROUTED:
			   // We've got a call to tear down! TEAR IT DOWN MISTER GORBACHEV, TEAR IT DOWN I SAY!!!
               switch(fxo[connchan[channum]]) {
               case 2:
                   isdn_hkstate(connchan[channum], DX_ONHOOK);
                   break;
               case 1: 
				   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // We're not removing loop current for FXO, just hanging the interface up. ESF/B8ZS
				   // dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BON); // We're not removing loop current for FXO, just hanging the interface up. SF/B8ZS
				   dxinfox[connchan[channum]].state = ST_WTRING;
                   break;
               case 0:
				   dxinfox[connchan[channum]].state = ST_PERMSIG; // Did this other loser not hang up? They're hitting the permanent signal condition.
				   // dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BON); // SF/B8ZS
				   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // E&M hack
                   dx_stopch( dxinfox[connchan[channum]].chdev, (EV_ASYNC | EV_NOSTOP) );
                   break;
               default:
                   break;
               }
               
				    dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // E&M code - this is to make sure the channel is back on-hook if it came offhook during the ringing state
			   if ( nr_scunroute( dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ connchan[ channum ] ].tsdev, SC_DTI, SC_FULLDUP ) != 0 ) {
				    sprintf( tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really should look into this...", channum, connchan[channum] );
					// This'll halt execution, so we should only do it in places we really need it.
					disp_err(channum, dxinfox[ channum ].chdev, tmpbuff );
					// Proceed with the call teardown procedure anyway. Not much else we can do, right?
			   }
			   if ( nr_scroute( dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP ) != 0 ) {
				    sprintf( tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really, *really* should look into this...", channum, connchan[channum] );
					// This'll halt execution, so we should only do it in places we really need it.
					disp_err(channum, dxinfox[ channum ].chdev, tmpbuff );
			   }
			   if ( nr_scroute( dxinfox[ connchan[ channum ] ].tsdev, SC_DTI, dxinfox[ connchan[ channum ] ].chdev, SC_VOX, SC_FULLDUP ) != 0 ) {
				    sprintf( tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really, *really*, *REALLY* should look into this...", channum, connchan[channum] );
					// This'll halt execution, so we should only do it in places we really need it.
					disp_err(channum, dxinfox[ channum ].chdev, tmpbuff );
			   }
			   dxinfox[channum].state = ST_WTRING;

			   // Send the other call on its way. To facilitate permanent signal, we'll need to send battery drop.
			   // playtone_cad( connchan[ channum ], 480, 0, 100 );

			   // Reset line assignment variables.
			   connchan[ channum ] = 0;
			   connchan[ connchan[ channum ] ] = 0;
			   // We are at ground zero.
			   return(0);
           case ST_RINGPHONE1:
               dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // For E&M hack - set receiving phone back to an idle state
               dxinfox[connchan[channum]].state = ST_WTRING;
		   default:
	      dx_stopch( dxinfox[channum].chdev, (EV_ASYNC | EV_NOSTOP) );
		  // set_hkstate( channum, DX_ONHOOK );
		if (ATDX_STATE(dxinfox[channum].chdev) != CS_IDLE) {
			sprintf(tmpbuff, "Warning: channel device isn't idle; in state %li", ATDX_STATE(dxinfox[channum].chdev));
	        disp_msg(tmpbuff);
		}

          if ( dx_clrdigbuf( dxinfox[channum].chdev ) == -1 ) {
	           sprintf( tmpbuff, "Cannot clear DTMF Buffer for %s",
		       ATDV_NAMEP( dxinfox[channum].chdev ) );
               disp_err(channum, dxinfox[ channum ].chdev,tmpbuff);
	           return(0);
      }
		  dxinfox[channum].state = ST_WTRING;
		  return(0);

		   }
		   case 0xF5: // FXO loop current drop state. No need to check the state of the opposing side; it'll always be FXS right now. ESF/B8ZS
	    // case 0x31: // FXO loop current drop state. No need to check the state of the opposing side; it'll always be FXS right now. SF/B8ZS
		   switch (dxinfox[channum].state) {
               
           case ST_ROUTEDISDN:
               isdn_hkstate(connchan[channum], DX_ONHOOK);
               // Yeah, I know, this is duplicated and ugly - but it's also here because timeslot routing wasn't being performed right when the FXO side released an ISDN call; it falls into ST_ISDNOUT and returns.
               if (nr_scunroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[connchan[channum]].tsdev, SC_DTI, SC_FULLDUP) != 0) {
                   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really should look into this...", channum, connchan[channum]);
                        disp_err(channum, dxinfox[channum].chdev, tmpbuff);
                        // Proceed with the call teardown procedure anyway. Not much else we can do, right?
               }
               if (nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP) != 0) {
                   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really, *really* should look into this...", channum, connchan[channum]);
                   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
               }
               if (nr_scroute(dxinfox[connchan[channum]].tsdev, SC_DTI, dxinfox[connchan[channum]].chdev, SC_VOX, SC_FULLDUP) != 0) {
                   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really, *really*, *REALLY* should look into this...", channum, connchan[channum]);
                   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
               }
               dx_stopch(dxinfox[channum].chdev, EV_ASYNC);
               dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // ESF/B8ZS
               dxinfox[channum].state = ST_WTRING;
               return(0);

		   case ST_BUSY:
                   case ST_WARBLE:
		   case ST_REORDER:
		   case ST_FXSTEST1:
		   case ST_PERMSIG:
			   dx_setevtmsk(dxinfox[channum].chdev, DM_DIGOFF);
		   case ST_DYNPLAY:
		   case ST_DYNPLAYE:
		   case ST_FXODISA:
           case ST_ISDNOUT:
			   dx_stopch(dxinfox[channum].chdev, EV_ASYNC);
			   dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // ESF/B8ZS
			   // dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON); // SF/B8ZS
			   dxinfox[channum].state = ST_WTRING;
			   return(0);
		   default:
			   // This shouldn't be default. To do: change!
			   sprintf(tmpbuff, "DEBUG: FXO loop current drop state entered on channel %d via 0xF5", channum);
			   disp_msg(tmpbuff);
			   // This is copied and pasted from that other shit for the moment. You should also account for non-connected call states.
			   if (nr_scunroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[connchan[channum]].tsdev, SC_DTI, SC_FULLDUP) != 0) {
				   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really should look into this...", channum, connchan[channum]);
				   // This'll halt execution, so we should only do it in places we really need it.
				   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
				   // Proceed with the call teardown procedure anyway. Not much else we can do, right?
			   }
			   dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // Sets channel on-hook, at least I hope. ESF/B8ZS
			   // dt_settssigsim(dxinfox[channum].tsdev, DTB_AOFF | DTB_BON); // Sets channel on-hook, at least I hope. SF/B8ZS
			   if (nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP) != 0) {
				   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really, *really* should look into this...", channum, connchan[channum]);
				   // This'll halt execution, so we should only do it in places we really need it.
				   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
			   }
			   if (nr_scroute(dxinfox[connchan[channum]].tsdev, SC_DTI, dxinfox[connchan[channum]].chdev, SC_VOX, SC_FULLDUP) != 0) {
				   sprintf(tmpbuff, "Uhh, call teardown between channel %d and %d failed. You really, *really*, *REALLY* should look into this...", channum, connchan[channum]);
				   // This'll halt execution, so we should only do it in places we really need it.
				   disp_err(channum, dxinfox[channum].chdev, tmpbuff);
			   }
			   dxinfox[channum].state = ST_WTRING;
			   // Send the other call on its way. To facilitate permanent signal, we'll need to send battery drop.
			   // playtone_cad( connchan[ channum ], 480, 0, 100 );
			   if (dxinfox[connchan[channum]].state != ST_INCOMING) {
				   dxinfox[connchan[channum]].state = ST_PERMSIG; // Did this other loser not hang up? They're hitting the permanent signal condition.
				   // Loop current drop for a ringing phone is the kiss of death! Don't do this, or it's not coming back out!
				   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // ESF/B8ZS
                   dx_stopch( dxinfox[connchan[channum]].chdev, (EV_ASYNC | EV_NOSTOP) );
				   // dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BON); // SF/B8ZS
			   }
			   else {
                   dxinfox[connchan[channum]].state = ST_WTRING;
                   dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // ESF/B8ZS. This is a bugfix that I was hoping we wouldn't need.
               }
			   // Reset line assignment variables.
			   connchan[channum] = 0;
			   connchan[connchan[channum]] = 0;
			   // We are at ground zero.
		   }
		   return(0);

		   case 0xA5: // ESF/B8ZS
	   //case 0x21: // SF/B8ZS
		   sprintf(tmpbuff, "DEBUG: FXO channel %d entered loop current active state", channum);
		   disp_msg(tmpbuff);
		   return(0);

	   default:
		   sprintf( tmpbuff, "Unidentified channel bank bit state entered: %li", sigbits);
		   disp_msg(tmpbuff);
		   return(0);
	   }


   }

   curstate = dxinfox[ channum ].state;		/* Current State */

   if (event != DTEV_SIG) {
      sprintf( tmpbuff, "Unknown Event 0x%x Received on %s.  Data = 0x%hx",
                                     event, ATDV_NAMEP( tsdev ), sig );
      disp_msg(tmpbuff );
	  disp_status(channum, "Unknown Event" );
      dxinfox[ channum ].state = ST_ERROR;
      set_hkstate( channum, DX_ONHOOK );
      return 0;
   }

   for (indx = 0; indx < 4; indx++) {
      /*
       * Check if bit in change mask (upper nibble - lower byte) is set or
       * if this is a WINK (upper nibble - upper byte) event
       */
      if (!(sig & (SIGEVTCHK << indx))) {
         continue;
      }
      switch (sig & (SIGBITCHK << indx)) {
      case DTMM_AON:
         switch ( frontend ) {
         case CT_NTT1:		/* Incoming Rings Event */
            if ( curstate == ST_WTRING ) {
	       /*
	        * Set Channel to Off-Hook
	        */
	       dxinfox[ channum ].state = ST_OFFHOOK;
	       set_hkstate( channum, DX_OFFHOOK );

 	       disp_status(channum, "Incoming Call" );
	       return 0;
            }
            break;

         case CT_NTE1:
            /*
             * Caller hangup, set state to ONHOOK and set channel to ONHOOK
             */
	    dxinfox[ channum ].state = ST_ONHOOK;
            set_hkstate( channum, DX_ONHOOK );
	    return 0;
         }
         break;

      case DTMM_AOFF:
         switch ( frontend ) {
         case CT_NTE1:		/* Incoming Rings Event */
            if ( curstate == ST_WTRING ) {
	       /*
	        * Set Channel to Off-Hook
	        */
	       dxinfox[ channum ].state = ST_OFFHOOK;
	       set_hkstate( channum, DX_OFFHOOK );

 	       disp_status(channum, "Incoming Call" );
	       return 0;
            }
            break;

         case CT_NTT1:
            /*
             * Caller hangup, set state to ONHOOK and set channel to ONHOOK
             */
	    dxinfox[ channum ].state = ST_ONHOOK;
            set_hkstate( channum, DX_ONHOOK );
	    return 0;
         }
         break;

      case DTMM_BOFF:
      case DTMM_BON:
      case DTMM_COFF:
      case DTMM_CON:
      case DTMM_DOFF:
      case DTMM_DON:
      case DTMM_WINK:
         break;

      default:
         sprintf( tmpbuff, "Unknown DTEV_SIG Event 0x%hx Received on %s",
						sig, ATDV_NAMEP( tsdev ) );
         disp_status(channum, tmpbuff );
      }
   }
   return 0;
}


/******************************************************************************
 *        NAME: long dtierr_hdlr()
 * DESCRIPTION: Signal handler to catch DTEV_ERREVT events generated by the
 *              dti timeslots.
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: 0 if event to be eaten by SRL otherwise 1
 *    CAUTIONS: None.
 ******************************************************************************/
int dtierr_hdlr()
{
   int tsdev = sr_getevtdev();
   // int event = sr_getevttype();
   int channum = get_channum( tsdev );
   int curstate;

   if ( channum == -1 ) {
      return( 0 );		/* Discard Message - Not for a Known Device */
   } // end if

   curstate = dxinfox[ channum ].state;		/* Current State */

   /*
    * Print a message
    */
   sprintf( tmpbuff, "Received an ERROR Event for %s", ATDV_NAMEP( tsdev ) );
   disp_status(channum, tmpbuff );

   /*
    * Put state into ST_ERROR
    */
   dxinfox[ channum ].state = ST_ERROR;
   set_hkstate( channum, DX_ONHOOK );

   return( 0 );
}





/***************************************************************************
 *        NAME: void sysinit()
 * DESCRIPTION: Start D/4x System, Enable CST events, put line ON-Hook
 *		and open VOX files.
 *       INPUT: None.
 *      OUTPUT: None.
 *     RETURNS: None.
 *    CAUTIONS: None.
 ***************************************************************************/
void sysinit()
{
   int	 channum;
   char	 d4xname[ 32 ];
   char	 dtiname[ 32 ];
   CT_DEVINFO ct_devinfo;


   if ( maxchans > MAXCHANS )  {
      sprintf( tmpbuff, "Only %d Channels will be used", MAXCHANS );
      disp_msg( tmpbuff );
      maxchans = MAXCHANS;
   } // ENDIF

   disp_msg( "Initializing ...." );

   /*
    * Open VOX Files
    */

     if ( ( introfd = open( INTRO_VOX, O_RDONLY ) ) == -1 ) {
      sprintf( tmpbuff, "Cannot open %s", INTRO_VOX );
      disp_msg( tmpbuff );

	  exit(2);
      
   } // ENDIF

   //  UNIX file open converted NT/Dialogic file open

   if ( ( invalidfd = open( INVALID_VOX, O_RDONLY  ) ) == -1 ) {
      sprintf( tmpbuff, "Cannot open %s", INVALID_VOX );
      disp_msg( tmpbuff );
	  exit(2);
      
   } // ENDIF

   //  UNIX file open converted NT/Dialogic file open

   if ( ( goodbyefd = open( GOODBYE_VOX, O_RDONLY ) ) == -1 ) {
      sprintf( tmpbuff, "Cannot open %s", GOODBYE_VOX );
      disp_msg( tmpbuff );
	  exit(2);
      
   } // endif

   /*
    * Clear the dxinfox structure.
    * Initialize Channel States to Detect Call.
    */
   memset( dxinfox, 0, (sizeof( DX_INFO ) * (MAXCHANS+1)) );
   
   reversaltest.msg_code = DTCAS_CREATE_TRAIN;
   reversaltest.rfu = 0;
   reversaltest.template_id = 1;
   reversaltest.OffPulseCode = (DTB_AON | DTB_BON | DTB_COFF | DTB_DON);
   reversaltest.OnPulseCode = (DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON );
   reversaltest.PulseIntervalMin = 100; // Minimum pulse sending time
   reversaltest.PulseIntervalNom = 200; // Nominal pulse sending time
   reversaltest.PulseIntervalMax = 400; // Maximum pulse sending time
   reversaltest.PreTrainInterval = 0;
   reversaltest.PreTrainIntervalNom = 0;
   reversaltest.InterPulseIntervalMin = 1000;
   reversaltest.InterPulseIntervalMax = 1400;
   reversaltest.InterPulseIntervalNom = 1200;
   reversaltest.PostTrainInterval = 1500;
   reversaltest.PostTrainIntervalNom = 1700;
   
   reversalenable.msg_code = DTCAS_ENABLE_TEMPLATE;
   reversalenable.rfu = 0;
   reversalenable.template_id = 1;
   
   reversalxmit.msg_code = DTCAS_TRANSMIT_TEMPLATE;
   reversalxmit.rfu = 0;
   reversalxmit.template_id = 1;
   reversalxmit.pulse_count = 1;
   reversalxmit.sequence_count = 0;

   clearmsg.msg_code = DTCAS_CLEAR_ALL_TEMPLATE;
   clearmsg.rfu = 0;
   clearmsg.template_id = 1;
   
for ( channum = 1; channum <= maxchans; channum++ ) {
      /*
       * Open the D/4x Channels
       */
      sprintf( d4xname, "dxxxB%dC%d",
		(channum % 4) ? (channum / 4) + d4xbdnum :
			d4xbdnum + (channum / 4) - 1,
		(channum % 4) ? (channum % 4) : 4 );

      if ( ( dxinfox[ channum ].chdev = dx_open( d4xname, 0 ) ) == -1 ) {
		 sprintf( tmpbuff, "Unable to open channel %s, errno = %d",
			d4xname, errno );
		 disp_status(channum, tmpbuff );
         // if at least two channels have been opened, then proceed updating 
         // the maximum number of channels accordingly
         if (channum > 1) {
            maxchans = channum-1;
            continue;
         }
         else {
             sys_quit();
         }
      } // endif

	  disp_status(channum, "Initializing...");

      if (frontend == CT_NTANALOG) {
         /*
          * Route analog frontend timeslots to its resource in SCbus mode,
          * if required. Note that since there is a possibility that the 
          * card is a non-SCbus card, we will check the busmode first
          */

         if ( dx_getctinfo( dxinfox[ channum ].chdev, &ct_devinfo ) == -1 ) {
		    sprintf( tmpbuff, "dx_getctinfo() failed for %s",
			    ATDV_NAMEP( dxinfox[ channum ].chdev ) );
		    disp_status(channum, tmpbuff );
		    sys_quit();
         } 
 
         if ((scbus == TRUE) && (routeag == TRUE) && ct_devinfo.ct_busmode == CT_BMSCBUS) {

            if ( nr_scunroute( dxinfox[ channum ].chdev, SC_LSI,
                          dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP ) == -1){
				
				disp_status(channum,"Error unrouting Analog during SYSINIT");
				sys_quit();
			}
				

            if (nr_scroute( dxinfox[ channum ].chdev, SC_LSI,
                            dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP )
                                                                    == -1 ) {
			   sprintf( tmpbuff, "nr_scroute() failed for %s",
								 ATDV_NAMEP( dxinfox[ channum ].chdev ) );
			   disp_status(channum, tmpbuff );
			   sys_quit();
            } // end if
         } // endif

      } else {		/* Digital Frontend */
	
         /*
          * Form digital timeslots' names based upon bus mode.
          */

		  // Removed CCM board naming that was in UNIX version

         if (scbus == TRUE) {
            sprintf( dtiname, "dtiB%dT%d", dtibdnum, channum );
         } else if (boardtag == TRUE) { 
            sprintf( dtiname, "dtiB%dT%d", dtibdnum, channum );
         } // end if


         /*
          * Open DTI timeslots.
          */
         if ( ( dxinfox[ channum ].tsdev = dt_open( dtiname, 0 ) ) == -1 ) {
			sprintf( tmpbuff, "Unable to open timeslot %s, errno = %d",
			dtiname, errno );
			disp_status(channum, tmpbuff );
			sys_quit();
         } // endif

         /*
          * Route timeslots to channels based upon bus mode.
          */
         if (scbus == TRUE) {


            if  (nr_scunroute( dxinfox[ channum ].tsdev, SC_DTI,
                          dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP ) == -1){

				 disp_status(channum,"Error unrouting Digital during SYSINIT");
				 exit(-1);
			}



            if (nr_scroute( dxinfox[ channum ].tsdev, SC_DTI,
                            dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP )
                                                                    == -1 ) {
			   sprintf( tmpbuff, "nr_scroute() failed for %s - %s",
								 ATDV_NAMEP( dxinfox[ channum ].chdev ),
								 ATDV_NAMEP( dxinfox[ channum ].tsdev ) );
			   disp_status(channum, tmpbuff );
			   sys_quit();
            } // endif
         } // endif
		 
		 //  -- removed PEB mode checking from UNIX demo

          
         } // end if (else)
 

      /*
       * Enable the CST events
       */

	  if (frontend != CT_NTTEST ) {
      if ( dx_setevtmsk( dxinfox[ channum ].chdev, DM_RINGS | DM_DIGOFF ) == -1 ) {
		 sprintf( tmpbuff, "Cannot set CST events for %s",
			ATDV_NAMEP( dxinfox[ channum ].chdev ) );
		 disp_status(channum, tmpbuff );
		 sys_quit();
         } // end if 

	  }
      
      else if (dx_setevtmsk(dxinfox[channum].chdev, DM_DIGITS) == -1 ) {
		 sprintf( tmpbuff, "Cannot set CST events for %s",
			ATDV_NAMEP( dxinfox[ channum ].chdev ) );
		 disp_status(channum, tmpbuff );
		 sys_quit();
         } // end if 

      /*
       * Set to answer after MAXRING rings
       */
      if ( dx_setrings( dxinfox[ channum ].chdev, MAXRING ) == -1 ) {
		 sprintf( tmpbuff, "dx_setrings() failed for %s",
			ATDV_NAMEP( dxinfox[ channum ].chdev ) );
		 disp_status(channum, tmpbuff );
		 sys_quit();
      } // end if 

      // set the sampling rate
      
      // These calls should be unnecessary.
      
	  /*
      if ( dx_setparm( dxinfox[ channum ].chdev, DXBD_MAXPDOFF, (void*)&maxoff ) == -1 ) {
		 sprintf( tmpbuff, "dx_setparm() failed for %s",
			ATDV_NAMEP( dxinfox[ channum ].chdev ) );
		 disp_status( g_hWnd,channum, tmpbuff );
		 sys_quit();
      } // end if 
      if ( dx_setparm( dxinfox[ channum ].chdev, DXCH_RECRDRATE, (void*)&sampling_rate ) == -1 ) {
		 sprintf( tmpbuff, "dx_setparm() failed for %s",
			ATDV_NAMEP( dxinfox[ channum ].chdev ) );
		 disp_status( g_hWnd,channum, tmpbuff );
		 sys_quit();
      } // end if 

      */
	  
	  sprintf( dxinfox[ channum ].ac_code, "%d234", channum%10 );
	  sprintf( dxinfox[ channum ].msg_name, "msg%d.vox", channum );
	  /*
       * If it is a digital network environment, disable idle on the timeslot
       * and set it to signalling insertion.  Also setup the signalling event
       * handler.
       */
      if (frontend != CT_NTANALOG) {
         if ( dt_setidle( dxinfox[ channum ].tsdev, DTIS_DISABLE ) == -1 ) {
			sprintf( tmpbuff, "Cannot disable IDLE for %s",
								 ATDV_NAMEP( dxinfox[ channum ].tsdev ) );
			disp_status(channum, tmpbuff );
			sys_quit();
         } // end if

         if ( dt_setsigmod( dxinfox[ channum ].tsdev, DTM_SIGINS ) == -1 ) {
			sprintf( tmpbuff, "Cannot set SIGINS for %s",
								 ATDV_NAMEP( dxinfox[ channum ].tsdev ) );
			disp_status(channum, tmpbuff );
			sys_quit();
         } // end if

         if (sr_enbhdlr(dxinfox[channum].tsdev, DTEV_CASSENDENDEVT, (EVTHDLRTYP) casxmit_hdlr) == -1 ) {
			disp_status( channum, "Unable to set-up the DTI CAS transmission handler" );
			sys_quit();
         } // end if 

         if (sr_enbhdlr(dxinfox[channum].tsdev, DTEV_SIG, (EVTHDLRTYP) sig_hdlr)
                                                                    == -1 ) {
			disp_status( channum, "Unable to set-up the DTI signalling handler" );
			sys_quit();
         } // end if 



         if (sr_enbhdlr(dxinfox[channum].tsdev, DTEV_ERREVT, (EVTHDLRTYP) dtierr_hdlr) == -1 ) {
			disp_status( channum, "Unable to set-up the DTI error handler" );
			sys_quit();
         } // end if 

         if ( dt_setevtmsk( dxinfox[ channum ].tsdev, DTG_SIGEVT,
                                   DTMM_AOFF | DTMM_AON | DTMM_BON | DTMM_BOFF | DTMM_WINK | DTMM_CON | DTMM_COFF | DTMM_DON | DTMM_DOFF, DTA_SETMSK) == -1) {
			disp_status( channum, "Unable to set DTI signalling event mask" );
			sys_quit();
		 }
         if ( dt_setevtmsk( dxinfox[ channum ].tsdev, DTG_PDIGEVT, DTMM_AON, DTA_SETMSK) == -1) {
			disp_status( channum, "Unable to set DTI pulse digit event mask" );
			sys_quit();
		    }
            
         if ( dt_setevtmsk( dxinfox[ channum ].tsdev, DTG_PDIGEVT, DTMM_AOFF, DTA_SETMSK) == -1) {
			disp_status( channum, "Unable to set DTI pulse digit event mask" );
			sys_quit();
		    }
            
        if (dt_castmgmt(dxinfox[channum].tsdev, &reversaltest, &reversal_response) == -1) {
     	   sprintf(tmpbuff, "ERROR: dt_castmgmt failed with message %s", ATDV_ERRMSGP(dxinfox[channum].tsdev));
	       disp_err( channum, dxinfox[channum].tsdev, tmpbuff);
        }
        
        if (dt_castmgmt(dxinfox[channum].tsdev, &reversalenable, &reversal_enbresponse) == -1) {
	        disp_err( channum, dxinfox[channum].tsdev, "FUUUUUUUUUUUUUUUUUUCK2");
            
        }

      } // end if

      /*
       * Start the application by putting the channel to ON-HOOK state.
       */
      dxinfox[ channum ].state = ST_ONHOOK;
      set_hkstate( channum, DX_ONHOOK );

	 } // end for
     
      /*
       * Enable the callback event handlers
       */
	  if (sr_enbhdlr(EV_ANYDEV, TDX_DIAL, (EVTHDLRTYP) dial_hdlr)
                                                                    == -1 ) {
         disp_status( channum, "Unable to set-up the DIAL handler" );
      }

      if (sr_enbhdlr(EV_ANYDEV, TDX_PLAYTONE, (EVTHDLRTYP) playtone_hdlr)
                                                                    == -1 ) {
         disp_status( channum, "Unable to set-up the PLAYTONE handler" );
      }

      if (sr_enbhdlr(EV_ANYDEV, TDX_CST, (EVTHDLRTYP) cst_hdlr)
                                                                    == -1 ) {
		 disp_status( channum, "Unable to set-up the CST handler" );
	
	  } // end if 

      if (sr_enbhdlr(EV_ANYDEV, TDX_PLAY, (EVTHDLRTYP) play_hdlr)
                                                                    == -1 ) {
		 disp_status(channum, "Unable to set-up the PLAY handler" );
		
      }// end if 

      if (sr_enbhdlr(EV_ANYDEV, TDX_RECORD, (EVTHDLRTYP) record_hdlr)
                                                                    == -1 ) {
		 disp_status(channum, "Unable to set-up the RECORD handler" );
		
      } // end if 

      if (sr_enbhdlr(EV_ANYDEV, TDX_GETDIG, (EVTHDLRTYP) getdig_hdlr)
                                                                    == -1 ) {
		 disp_status(channum, "Unable to set-up the GETDIG handler" );
		
      } // end if 
	  if ( frontend != CT_NTTEST ) {
      if (sr_enbhdlr(EV_ANYDEV,TDX_SETHOOK, (EVTHDLRTYP) sethook_hdlr)
                                                                    == -1 ) {
		 disp_status(channum, "Unable to set-up the SETHOOK handler" );
		
      } // end if 
	  }

      if (sr_enbhdlr(EV_ANYDEV, TDX_ERROR, (EVTHDLRTYP) error_hdlr)
                                                                    == -1 ) {
		 disp_status(channum, "Unable to set-up the ERROR handler" );
	
      } // end if 
      
      if (sr_enbhdlr(EV_ANYDEV, TDX_NOSTOP, (EVTHDLRTYP) nostop_hdlr)
                                                                    == -1 ) {
		 disp_status(channum, "Unable to set-up the ERROR handler" );
	
      } // end if 


   /*
    * Display number of channels being used
    */

   /*
   if (options[0] == fAnalog){

	   sprintf(tmpbuff," - Using Analog Frontend");

   } else if ( options[0] == fT1){

	   sprintf(tmpbuff," - Using T1 Frontend");

   } else if ( options[0] == fE1){

	   sprintf(tmpbuff," - Using E1 Frontend");
   } else {
   */

   // }

   isdn_prep();

   sprintf( tmpbuff, "Using %d line%s", maxchans, maxchans > 1 ? "s" : "");
   disp_msg( tmpbuff );
   confparse();
   unsigned char counter;
   for ( counter = 0; counter < 25; counter++ ) {
       printf("DEBUG: For %d: %s\n", counter, dntable[counter] );
   }

}

/*********************************************************************
 *        NAME: void chkargs()
 * DESCRIPTION: Check options that were sleected
 *		channels to use.
 *       INPUT: None
 *      OUTPUT: None.
 *     RETURNS: None.
 *    CAUTIONS: None.
 **********************************************************************/
void chkargs()
{
    
   // For now, these are all hardcoded. Sorry.
   
   /*
    * q.931 debug mode. This should be in a config file as needed.
    */ 
    q931debug = 0;
    
    /*
     * Start/end channel for ISDN shennanigans. This should be in a config file too.
     */
     startchan = 25;
     isdnmax = 47;
     isdnbdnum = 16; // Should be same as d4xbdnum to use adjacent span
 /*
  * First D/4x Board Number
  */
     d4xbdnum = 16;
	 // d4xbdnum = options[2];
	 
 /*
  * First DTI Board Number
  */
	 // dtibdnum = options[3];
     dtibdnum = 1;

 /*
  * Number of Channels to Use
  */
	 // maxchans = options[4];
     maxchans = 24;
	 frontend = CT_NTTEST;
	 boardtag = TRUE;
}
