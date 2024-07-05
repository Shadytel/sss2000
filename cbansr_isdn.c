// Dialogic-specific #includes

#include <dtilib.h>
#include <srllib.h>
#include <dxxxlib.h>
#include <gclib.h>
#include <gcisdn.h>
#include <sctools.h>

// Generic C includes

#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h> // For memset
#include <fcntl.h> // For all of one open() uses

// Local includes

#include "cbansrx.h"

// For numbering plan identification in set_cpn():
// These definitions are all pre-LSHed to fit into the most significant bits of an 8-bit integer. Please note, the MSB is an extension bit; MSB low indicates screen/presentation info follows.
#define UNKNOWN 0
#define INTERNATIONAL 16
#define NATIONAL 32
#define NETWORK_SPECIFIC 48
#define SUBSCRIBER 64
#define ABBREVIATED 96
#define RESERVED_TYPE 112

#define ISDN 1
#define DATA 3
#define TELEX 4
#define NATIONAL_PLAN 8
#define PRIVATE 9
#define RESERVED_PLAN 15

#define PRES_ALLOW 128
#define PRES_RESTRICT 160
#define PRES_UNAVAIL 192
#define PRES_RESERVED 224
#define USER_NOSCREEN 0
#define USER_VERPASS 1
#define USER_VERFAIL 2
#define NETSCREEN 3

#define NOREC 0
#define REC 1
#define ONEWAY 2

#define SPEECH 0x80
#define UNRESTRICTED 0x88
#define RESTRICTED 0x89
#define NB_AUDIO 0x90
#define UNRESTRICTED_AUDIO 0x91
#define VIDEO 0x98

#define random_at_most(x) (rand() % (x))

// Globals
Q931SIG isdninfo[ MAXCHANS + 1];
char q931debug;
char isdnstatus[ MAXCHANS + 1 ];
char ownies[ MAXCHANS + 1 ];
//

char cutthrough[ MAXCHANS ];
static bool dm3board; // We really don't need the FXS code to know this.

const unsigned char altbdnum = 1	; // Temporary fix for what should be in the config file

static GC_IE_BLK raw_info_elements[MAXCHANS + 1];
static IE_BLK info_elements[MAXCHANS + 1];

// char    tmpbuff[ 256 ];     /* Temporary Buffer */

static struct linebag {
    LINEDEV ldev;               /* GlobalCall API line device handle */
    CRN     crn;                /* GlobalCAll API call handle */
    int     blocked;            /* channel blocked/unblocked */
    // See cbansr.c for other resources
} port[MAXCHANS + 1];

GC_INFO         gc_error_info;  // GC error data

void disp_msg(char *string);
short get_linechan(int linedev);
char writesig(unsigned char offset, short channum);
int playtone_rep( int channum, int toneval, int toneval2, int amp1, int amp2, int ontime, int pausetime, int cycles);
char isdn_drop(short channum, int cause);
int isdn_answer(short channum);

int gc_errprint(char *function, int channel, int callstate) {
    disp_msg("Performing gc_errprint");
    FILE  *errfd;
    gc_ErrorInfo(&gc_error_info);
    disp_msg("Opening error file");
    errfd = fopen("isdn_error.txt", "a+");
    sprintf(tmpbuff, "%s - Printing to error file, fd %i", gc_error_info.gcMsg, (int)errfd);
    disp_msg(tmpbuff);
    fprintf(errfd, "%s() GC ErrorValue: 0x%hx - %s, CC ErrorValue: 0x%lx - %s, additional info: %s, channel %d, call state 0x%hx, program state %d, CRN %ld, ldev %li\n", function, gc_error_info.gcValue, gc_error_info.gcMsg, gc_error_info.ccValue, gc_error_info.ccMsg, gc_error_info.additionalInfo, channel, callstate, dxinfox[channel].state, port[channel].crn, port[channel].ldev);
    disp_msg("Closing error file");
    fclose(errfd);
    return (gc_error_info.gcValue);
}

/***************************************************************************
 *        NAME: int isdn_hkstate( channum, state )
 * DESCRIPTION: Set the channel to the appropriate hook status
 *       INPUT: short channum;      - Index into dxinfo structure
 *      int state;      - State to set channel to
 *      OUTPUT: None.
 *     RETURNS: -1 = Error
 *       0 = Success
 *    CAUTIONS: None.
 ***************************************************************************/
int isdn_hkstate(short channum, int state) {
    int chdev = dxinfox[ channum ].chdev;

    /*
     * Make sure you are in CS_IDLE state before setting the
     * hook status
     */

    if (ATDX_STATE(chdev) != CS_IDLE) {
        dx_stopch(chdev, EV_ASYNC);

        while (ATDX_STATE(chdev) != CS_IDLE);
    }
            /*
            if (state == DX_ONHOOK) {
                // Uhh, what was this for?

            }
            */

        switch (state) {
            case DX_ONHOOK:
                // Tear one down
                sprintf(tmpbuff, "Call being terminated on channel %i", channum);
                disp_msg(tmpbuff);
                
                if (isdn_drop(channum, GC_NORMAL_CLEARING) == -1) {
                    disp_msg("Holy shit! isdn_drop() failed! D:");
                    return (-1);
                }
                disp_status(channum, "ISDN channel ready!");

                if (dx_clrdigbuf(chdev) == -1) {
                    sprintf(tmpbuff, "Cannot clear DTMF Buffer for %s - state %d",
                            ATDV_NAMEP(chdev), dxinfox[channum].state);
                    disp_err(channum, chdev, tmpbuff);
                }

                break;

            case DX_OFFHOOK:
            
                // Accept a call!
                sprintf(tmpbuff, "Call being answered on channel %i", channum);
                disp_msg(tmpbuff);

                if (isdn_answer(channum) == -1) {
                    disp_msg("Holy shit! isdn_answer() failed! D:");
                    return (-1);
                }

                if (dxinfox[ channum ].state == ST_ROUTEDISDN2) {
                    return (0);    // If this is an outgoing call, don't try and run the welcome wagon.
                }
        }

    return (0);
}

int isdn_resetfail() {
    int  ldev = sr_getevtdev();
    short  lnum = get_linechan(ldev);
    disp_err(lnum, ldev, "Couldn't reset line device after critical failure! Card restart is recommended");
    return(0);
}

/***********************************************************
       Handle reset conditions for fatal switch errors
***********************************************************/

int isdn_reset() {
    int  ldev = sr_getevtdev();
    short  lnum = get_linechan(ldev);
    if (gc_WaitCall(port[lnum].ldev, NULL, 0, 0, EV_ASYNC) == -1) {
        disp_err(lnum, port[lnum].ldev, "Couldn't perform waitcall after resetting line device!");
        gc_errprint("gc_WaitCall", lnum, -1);
        return(0);
    }

    dxinfox[lnum].state = ST_WTRING;
    return (0);
}

/***********************************************************
               Handle unblocked ISDN events
***********************************************************/

int isdn_unblock() {
    int  ldev = sr_getevtdev();
    // int  event = sr_getevttype();
    short  lnum = get_linechan(ldev);

    sprintf(tmpbuff, "ldev %i unblocked", ldev);
    disp_msg(tmpbuff);

    port[lnum].blocked = 0; //Indicate a lack of blocking
    disp_status(lnum, "ISDN channel ready!");

    if (gc_WaitCall(port[lnum].ldev, NULL, 0, 0, EV_ASYNC) == -1) {
        gc_errprint("gc_WaitCall", lnum, -1);
        exit(2);
    }

    dxinfox[lnum].state = ST_WTRING;
    return (0);

}

/***********************************************************
     Handle a dropped call (release, send to releasehdlr)
***********************************************************/

int isdn_drophdlr()

{
    int  ldev = sr_getevtdev();
    short  lnum = get_linechan(ldev);

    if (gc_ReleaseCallEx(port[lnum].crn, EV_ASYNC) != GC_SUCCESS) {
        disp_msg("Call won't release. This thing is clingy...");
        gc_errprint("gc_ReleaseCallEx", lnum, -1);
        return (-1);
    }

    disp_msg("Call dropped successfully");
    return (0);
}

/***********************************************************
             Release a call, re-prep channel
***********************************************************/

int isdn_releasehdlr() {
    int  ldev = sr_getevtdev();
    short  lnum = get_linechan(ldev);

    port[lnum].crn = '\0';


    sprintf(tmpbuff, "Call released in state %d! Returning to waitcall state.", dxinfox[ lnum ].state);
    disp_msg(tmpbuff);
    // memset(info_elements[lnum].data, 0x00, 260); // There appear to be memory errors sometimes. This didn't fix, but maybe it'll fix some future problem
    if (gc_WaitCall(port[lnum].ldev, NULL, 0, 0, EV_ASYNC) == -1) {
        gc_errprint("gc_WaitCall", lnum, -1);
        exit(2);
    }

    cutthrough[ lnum ] = 0;

    return (0);
}

/***********************************************************
              Report state setting for channels
***********************************************************/

void setchanstate_hdlr()
{
   int  ldev = sr_getevtdev();
   short  lnum = get_linechan(ldev);
   if ( lnum == -1 ) {
      return;		/* Discard Message - Not for a Known Device */
   }
   sprintf( tmpbuff, "State successfully set on channel %d!", lnum);
   disp_msg(tmpbuff);
   return;
}


/***********************************************************
               Handle blocked ISDN events
***********************************************************/

int isdn_block() {
    int  ldev = sr_getevtdev();
    short  lnum = get_linechan(ldev);

    sprintf(tmpbuff, "ldev %i blocked", ldev);
    disp_msg(tmpbuff);
    /*

    // We can comment these out for now; they're basically debug events
    sprintf( tmpbuff, "event %i in block handler", event);
    disp_msg(tmpbuff);
    sprintf( tmpbuff, "lnum %i not ready, event %i", lnum, event);
    disp_msg(tmpbuff);
    */
    port[lnum].blocked = 1; //Indicate blocking
    disp_status(lnum, "ISDN channel not ready - blocked state");

    dxinfox[lnum].state = ST_BLOCKED;
    return (0);

}


void isdn_recordop(short channum, int causeval) {
    switch( causeval) {
        sprintf( tmpbuff, "DEBUG: Returned value was %ld\n", causeval);
        disp_msg(tmpbuff);
                    
        case 0x22C: // Requested circuit/channel not available
        case 0x22A: // Switching equipment congestion
        case 0x222: // No circuit/channel available
        case 0x23F: // Service or option not available. Unspecified.
        case 0x22F: // Resource unavailable, unspecified.
        case 0x208: // Preemption
        case 0x209: // Preemption - circuit resrved for reuse
        dxinfox[ connchan[ channum ] ].state = ST_ERRORANNC;
        dxinfox[ connchan[ channum ] ].msg_fd = open( "sounds/error/acb.pcm", O_RDONLY );
        play( connchan[channum], dxinfox[ connchan[ channum ] ].msg_fd, 0x81, 0 );
        break;                    
                    
        case 0x201: // Number unallocated
        dxinfox[ connchan[ channum ] ].state = ST_ERRORANNC;
        dxinfox[ connchan[ channum ] ].msg_fd = open( "sounds/error/toorcamp_nwn.pcm", O_RDONLY );
        play( connchan[channum], dxinfox[ connchan[ channum ] ].msg_fd, 0x81, 0 );
        break;
                    
        case 0x202: // No route to specified transit network (national use)
        case 0x21C: // Invalid number format (address incomplete)
        case 0x246: // Only restricted digital information bearer capability is available
        case 0x262: // Message not compatible with call state or message type non-existent
        case 0x239: // Bearer capability not authorized
        case 0x24F: // Service or option not implemented unspecified
        case 0x242: // Channel type not implemented
        case 0x216: // Number changed
        case 0x258: // Incompatible destination
        dxinfox[ connchan[ channum ] ].state = ST_ERRORANNC;
        dxinfox[ connchan[ channum ] ].msg_fd = open( "sounds/error/cbcad.pcm", O_RDONLY );
        play( connchan[channum], dxinfox[ connchan[ channum ] ].msg_fd, 0x81, 0 );
        break;
                    
        case 0x266: // Recovery on timer expiry
        case 0x226: // Network out of order
        dxinfox[ connchan[ channum ] ].state = ST_ERRORANNC;
        dxinfox[ connchan[ channum ] ].msg_fd = open( "sounds/error/facilitytrouble.pcm", O_RDONLY );
        play( connchan[channum], dxinfox[ connchan [ channum ] ].msg_fd, 0x81, 0 );
        break;
                    
        case 0x21B: // Destination out of order
        case 0x22B: // Access information discarded
        case 0x22E: // Precedence call blocked
        case 0x210: // Normal termination
        case 0x212: // No user responding
        case 0x215: // Call rejected
        dxinfox[ connchan[ channum ] ].state = ST_ERRORANNC;
        dxinfox[ connchan[ channum ] ].msg_fd = open( "sounds/error/ycdngt.pcm", O_RDONLY );
        play( connchan[channum], dxinfox[ connchan[ channum ] ].msg_fd, 0x81, 0 );
        break;
                    
        case 0x211: // User busy
		dxinfox[ connchan[ channum ] ].state = ST_BUSY;
                    if ( ATDX_STATE ( dxinfox[ channum ].chdev ) == CS_IDLE )
		playtone_rep(connchan[channum], 480, 620, -25, -27, 50, 50, 40);
                    else dx_stopch( dxinfox[ channum ].chdev, (EV_ASYNC | EV_NOSTOP ) );
        break;

        default:
        dxinfox[ connchan[ channum ] ].state = ST_REORDER;
        if ( ATDX_STATE ( dxinfox[ connchan[channum ]].chdev ) == CS_IDLE )
        playtone_rep( connchan[channum], 480, 620, -25, -27, 25, 25, 40 );
        else {
            sprintf(tmpbuff, "DEBUG: Channel was busy when trying to give reorder! State %li", ATDX_STATE(dxinfox[ connchan[channum]].chdev));
            disp_msg(tmpbuff);
            dx_stopch( dxinfox[ connchan[channum] ].chdev, (EV_ASYNC | EV_NOSTOP ) );
        }
    }
    return;
}
/***********************************************************
                   Disconnect a call
***********************************************************/

int isdn_discohdlr()

{
    GC_INFO callresults;
    METAEVENT           metaevent;
    int ldev;

    if (gc_GetMetaEvent(&metaevent) != GC_SUCCESS) {
        disp_msg("gc_GetMetaEvent failed. It really shouldn't.");
        return (-1);
    }

    // This event structure is required to derive cause codes ^

    if (gc_ResultInfo(&metaevent, &callresults) != GC_SUCCESS) {
        disp_msg("Failed to get ISDN disconnect results!");
        ldev = sr_getevtdev(); // Backup event device deriving
        // Write general failure to wardialer result log
    }

    else {
        ldev = metaevent.evtdev;
    }
    short  channum = get_linechan(ldev);
    isdninfo[channum].status = 1; // This is here for debugging purposes. Erase if not needed.
    // Clear outstanding data
    sprintf(tmpbuff, "DEBUG: Call entered discohdlr. channum is %d, fxo is %d, connchan is %d, fxo-c is %x, state is %d, state-c is %d, ownies is %d, cutthrough is %d", channum, fxo[channum], connchan[channum], fxo[connchan[channum]], dxinfox[channum].state, dxinfox[connchan[channum]].state, ownies[channum], cutthrough[channum]);
    disp_msg(tmpbuff);
    if ( connchan[channum] != 0 ) {
        dx_stopch(dxinfox[ connchan[channum] ].chdev, EV_ASYNC);
        dx_stopch(dxinfox[ channum ].chdev, EV_ASYNC);
    
        if( fxo[connchan[channum]] == 1) {
            dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BON | DTB_COFF | DTB_DON ); // We're not removing loop current for FXO, just hanging the interface up. ESF/B8ZS
            dxinfox[connchan[channum]].state = ST_WTRING;
            if (cutthrough[ channum ] != 0 ) {
                disp_msg("Unrouting channels from voice devices...1");

                if (nr_scunroute(dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].tsdev, SC_DTI, SC_FULLDUP) == -1) {
                    sprintf(tmpbuff, "Uhh, I hate to break it to you man, but SCUnroute1 is shitting itself, state %d", dxinfox[channum].state);
                    disp_err(channum, dxinfox[ channum ].chdev, tmpbuff);
                    return (-1);
                }

                if (nr_scroute(dxinfox[ connchan[channum] ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].chdev, SC_VOX, SC_FULLDUP) == -1) {
                    sprintf(tmpbuff, "Holy shit! SCUnroute2 threw an error! State %d", dxinfox[connchan[channum]].state);
                    disp_err(connchan[channum], dxinfox[ connchan[channum] ].chdev, tmpbuff);
                    return (-1);
                }

                if (nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP) == -1) {
                    sprintf(tmpbuff, "Holy shit! SCroute threw an error! State %d", dxinfox[ channum ].state);
                    disp_err(channum, dxinfox[channum].chdev, tmpbuff);
                    return (-1);
                }
            }
        
        cutthrough[ channum ] = 0;
        dxinfox[ channum ].state = ST_ONHOOK;
        if( (int) callresults.ccValue & 0x200) {
            dx_clrdigbuf(dxinfox[channum].chdev);
            if (isdn_drop( channum, ( (int) (callresults.ccValue & 0x7F) ) ) == -1 ) {
                disp_msg("HOLY SHIT! isdn_drop FAILED! D:");
            }
        }
        else isdn_hkstate(channum, DX_ONHOOK);
        return (0);
        }
        
        else if ( (dxinfox[channum].state == ST_RINGPHONE1) || (dxinfox[channum].state == ST_RINGPHONE2) || (ownies[channum] == 2) ) {
            dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // For E&M hack - set receiving phone back to an idle state
            dxinfox[connchan[channum]].state = ST_WTRING;
            cutthrough[ channum ] = 0;
            dxinfox[ channum ].state = ST_ONHOOK;
            if( (int) callresults.ccValue & 0x200) {
                dx_clrdigbuf(dxinfox[channum].chdev);
                if (isdn_drop( channum, ( (int) (callresults.ccValue & 0x7F) ) ) == -1 ) {
                    disp_msg("HOLY SHIT! isdn_drop FAILED! D:");
                }
             }
            else isdn_hkstate(channum, DX_ONHOOK);
            connchan[ channum ] = 0;
            connchan[ connchan[ channum ] ] = 0;
            return(0);
        }
        
        else if ( (dxinfox[channum].state == ST_ROUTED) && (fxo[channum] == 2) ) {
            // Teardown for when the ISDN end terminates the call

            if (nr_scunroute(dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].tsdev, SC_DTI, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "Uhh, I hate to break it to you man, but SCUnroute1 is shitting itself. State %d", dxinfox[ channum ].state);
                disp_err(channum, dxinfox[ channum ].chdev, tmpbuff);
                return (-1);
            }

            if (nr_scroute(dxinfox[ connchan[channum] ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].chdev, SC_VOX, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "Holy shit! SCUnroute2 threw an error! State %d", dxinfox[ connchan[ channum ] ].state);
                disp_err(connchan[channum], dxinfox[ connchan[channum] ].chdev, tmpbuff);
                return (-1);
            }

            if (nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "Holy shit! SCroute threw an error! State %d", dxinfox[ channum ].state);
                disp_err(channum, dxinfox[channum].chdev, tmpbuff);
                return (-1);
            }

                sprintf( tmpbuff, "DEBUG: Returned value was %ld\n", callresults.ccValue);
                disp_msg(tmpbuff);
                dxinfox[connchan[channum]].state = ST_PERMSIG; // Did this other loser not hang up? They're hitting the permanent signal condition.
			    // dt_settssigsim(dxinfox[linenum[channum]].tsdev, DTB_AON | DTB_BON); // SF/B8ZS
			    dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // E&M hack
                dx_stopch( dxinfox[connchan[channum]].chdev, (EV_ASYNC | EV_NOSTOP) );
        }
        
        else if (cutthrough[ channum ] != 0 ) {
            disp_msg("Unrouting channels from voice devices...2");

            if (nr_scunroute(dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].tsdev, SC_DTI, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "Uhh, I hate to break it to you man, but SCUnroute1 is shitting itself. State %d", dxinfox[ channum ].state);
                disp_err(channum, dxinfox[ channum ].chdev, tmpbuff);
                return (-1);
            }

            if (nr_scroute(dxinfox[ connchan[channum] ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].chdev, SC_VOX, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "Holy shit! SCUnroute2 threw an error! State %d", dxinfox[ connchan[ channum ] ].state);
                disp_err(connchan[channum], dxinfox[ connchan[channum] ].chdev, tmpbuff);
                return (-1);
            }

            if (nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "Holy shit! SCroute threw an error! State %d", dxinfox[ channum ].state);
                disp_err(channum, dxinfox[channum].chdev, tmpbuff);
                return (-1);
            }
        
            if ( cutthrough[channum] == 2 ) {
                sprintf( tmpbuff, "DEBUG: Returned value was %ld\n", callresults.ccValue);
                disp_msg(tmpbuff);
                dxinfox[connchan[channum]].state = ST_PERMSIG; // Did this other loser not hang up? They're hitting the permanent signal condition.
			    // dt_settssigsim(dxinfox[linenum[channum]].tsdev, DTB_AON | DTB_BON); // SF/B8ZS
			    dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AOFF | DTB_BOFF | DTB_COFF | DTB_DON ); // E&M hack
                dx_stopch( dxinfox[connchan[channum]].chdev, (EV_ASYNC | EV_NOSTOP) );
            }
        
            else if (cutthrough[channum] == 1) {
                isdn_recordop(channum, callresults.ccValue);
            }
        
        }
        
        else {
            // Did the call fail? This might be all we can do :/
            // TO DO: Test recording change
            isdn_recordop(channum, callresults.ccValue);
            /*
		    playtone_rep( connchan[channum], 480, 620, -25, -27, 25, 25, 40 );
		    dxinfox[ connchan[ channum ] ].state = ST_REORDER;
            */
        }
        connchan[ channum ] = 0;
        connchan[ connchan[ channum ] ] = 0;
        
    }
    else if (fxo[channum] == 2) {
        // On ISDN traffic, we're having issues with the DSP not stopping when we release. So let's fix that.
        dx_stopch( dxinfox[channum].chdev, (EV_ASYNC | EV_NOSTOP) );
    }
    cutthrough[ channum ] = 0;
    dxinfox[ channum ].state = ST_ONHOOK;
    if( (int) callresults.ccValue & 0x200) {
        dx_clrdigbuf(dxinfox[channum].chdev);
        sprintf(tmpbuff, "DEBUG: State is %d, ownies is %d", dxinfox[channum].state, ownies[channum]); 
        disp_msg(tmpbuff);
        if (isdn_drop( channum, ( (int) (callresults.ccValue & 0x7F) ) ) == -1 ) {
            disp_msg("HOLY SHIT! isdn_drop FAILED! D:");
        }
    }
    else isdn_hkstate(channum, DX_ONHOOK);
    //isdn_hkstate(channum, DX_ONHOOK);
    return (0);
}

void config_successhdlr() {
    int ldev = sr_getevtdev();
    sprintf(tmpbuff, "Configuration change on device %d made successfully", ldev );
    disp_msg(tmpbuff);
    return;
}

bool isdn_mediahdlr() {
    // This is likely left over from the dialer, but just to be sure, it's sticking around.
    disp_msg("ISDN Media event detected");

    int ldev = sr_getevtdev(); // Backup event device deriving
    // Write general failure to wardialer result log
    char conntype;
    short  channum = get_linechan(ldev);
    gc_GetCallInfo(port[channum].crn, CONNECT_TYPE, &conntype);

    //causelog(channum, conntype);
    

    if ((conntype != 0x4) && (conntype != 0x9) && (conntype != 0x10)) {
        // At some point, I'd like to make this more efficient; there shouldn't be so many checks hamfisted in.

        dx_stopch(dxinfox[ channum ].chdev, EV_ASYNC);
        isdn_hkstate(channum, DX_ONHOOK);

    }

    return (TRUE);

}

/***********************************************************
         Outgoing Call Connection - Progress message
***********************************************************/

char isdn_progressing() {
    // At the moment, Alerting events are coming here too. If both come back, is it going to cause problems with the timeslot routing functions?
    // gc_MakeCall() was successful. Let's bridge it to the originating channel.

    disp_msg("Entering call progress handler");
    int  ldev = sr_getevtdev();
    disp_msg("Identifying channum for progress handler");
    short  channum = get_linechan(ldev);

    if (q931debug == 1) {
        METAEVENT            metaevent;

        if (gc_GetMetaEvent(&metaevent) != GC_SUCCESS) {
            disp_msg("gc_GetMetaEvent failed. It really shouldn't.");
            return (-1);
        }

        int ldev = metaevent.evtdev; // This'll be more efficient; no need to call the same function twice.
        port[channum].crn = metaevent.crn;

        if (gc_GetSigInfo(ldev, (char *)&info_elements[channum], U_IES, &metaevent) != GC_SUCCESS) {
            disp_msg("gc_GetSigInfo_prog error!");
            gc_errprint("gc_GetSigInfo_prog", channum, -1);
            return (-1);
        }

        FILE  *iefd;
        char iefile[180];
        sprintf(iefile, "%s-%i-%ld_progress.dump", isdninfo[channum].dnis, channum, port[channum].crn);
        iefd = fopen(iefile, "a");
        // fprintf( iefd, "%s", info_elements[channum].data );
        // fwrite(info_elements[channum].data, sizeof(char), info_elements[channum].length, iefd);
        fwrite(&info_elements[channum], 1, sizeof(info_elements[channum]), iefd); // This should return size of IE dump in first two bytes, little endian format
        fclose(iefd);
    }


    disp_msg("Connecting outgoing call");
    sprintf(tmpbuff, "Outgoing call - Progress, state %d", dxinfox[ channum ].state);
    disp_status(channum, tmpbuff);

    // Perform cut-through

    if (cutthrough[ channum ] == 0) {

            if (nr_scunroute(dxinfox[ connchan[channum] ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].chdev, SC_VOX, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "SC_Unroute error in state %d", dxinfox[ connchan[channum] ].state);
                disp_err(channum, dxinfox[ connchan[channum] ].chdev, tmpbuff);
                return (-1);
            }

            if (nr_scunroute(dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "SC_Unroute error in state %d", dxinfox[ channum ].state);
                disp_err(channum, dxinfox[ channum ].chdev, tmpbuff);
                return (-1);
            }

            if (nr_scroute(dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].tsdev, SC_DTI, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "Uhh, I hate to break it to you man, but SCRoute is shitting itself - state %d", dxinfox[channum].state);
                disp_err(channum, dxinfox[ channum ].chdev, tmpbuff);
                return (-1);
            }
        cutthrough[channum] = 1;

    }

    return (0);

}

/***********************************************************
                   Handle a facility event
***********************************************************/
char isdn_facilityhdlr() {
    int  ldev = sr_getevtdev();
    short  channum = get_linechan(ldev);
    sprintf(tmpbuff, "Holy shit! ISDN Facility message returned on channel %d!", channum);
    disp_msg(tmpbuff);

    if (q931debug == 1) {
        METAEVENT                    metaevent;

        if (gc_GetMetaEvent(&metaevent) != GC_SUCCESS) {
            disp_msg("gc_GetMetaEvent failed. It really shouldn't.");
            return (-1);
        }

        int ldev = metaevent.evtdev; // This'll be more efficient; no need to call the same function twice.
        port[channum].crn = metaevent.crn;

        if (gc_GetSigInfo(ldev, (char *)&info_elements[channum], U_IES, &metaevent) != GC_SUCCESS) {
            disp_msg("gc_GetSigInfo_facilty error!");
            gc_errprint("gc_GetSigInfo_facility", channum, -1);
            return (-1);
        }

        FILE  *iefd;
        char iefile[180];
        sprintf(iefile, "%s-%i-%ld_facility.dump", isdninfo[channum].dnis, channum, port[channum].crn);
        iefd = fopen(iefile, "a");
        // fprintf( iefd, "%s", info_elements[channum].data );
        // fwrite(info_elements[channum].data, sizeof(char), info_elements[channum].length, iefd);
        fwrite(&info_elements[channum], 1, sizeof(info_elements[channum]), iefd); // This should return size of IE dump in first two bytes, little endian format
        fclose(iefd);
    }


    return (0);
}

/***********************************************************
                       Accept a call
***********************************************************/

char isdn_accept(short channum)

{

    if (gc_AcceptCall(port[channum].crn, 0, EV_ASYNC) != GC_SUCCESS) {
        gc_errprint("gc_AcceptCall", channum, -1);
        return (-1);
    }

    return (0);

}

/***********************************************************
              Handle an offered incoming call
***********************************************************/

int isdn_offerhdlr()

{
    METAEVENT           metaevent;

    if (gc_GetMetaEvent(&metaevent) != GC_SUCCESS) {
        disp_msg("gc_GetMetaEvent failed. It really shouldn't.");
        return (-1);
    }

    // int  ldev = sr_getevtdev();
    int ldev = metaevent.evtdev; // This'll be more efficient; no need to call the same function twice.
    short  channum = get_linechan(ldev);
    char retcode = 1;
    isdninfo[channum].forwardedtype = 0x00;
    isdninfo[channum].forwardedscn = 0x00;
    isdninfo[channum].forwardedrsn = 0x00;
    isdninfo[channum].forwarddata = 0x81;
    // isdninfo[channum].displayie[0] = 0x00; // Instead of initializing the CPName array, put this here, so the logger doesn't accidentally get old data if there's no display IE
    //memset(isdninfo[channum].displayie, 0x00, 83);  // Change 53 to F4
    //memset(isdninfo[channum].forwardednum, 0x00, 22);
    memset(&isdninfo[channum], 0x00, sizeof(struct isdn));
    port[channum].crn = metaevent.crn;
    sprintf(tmpbuff, "CRN %ld incoming", port[channum].crn);
    disp_msg(tmpbuff);
    disp_msg("Call offered! Take it! TAKE IT QUICK!");
    isdninfo[channum].status = 3; // 3 indicates incoming call to application
    unsigned char offset = 0;
    // char length;

    // gc_CallAck is unnecessary (from what I can tell) and will fail. Keep that shit off.

//    if ( gc_CallAck( port[channum].crn, &callack[channum], EV_ASYNC) != GC_SUCCESS ) {
//    disp_msg("gc_CallAck failed");
//       gc_errprint("gc_CallAck");
    // (insert a call rejection function here)
//       return(-1);
//    }

    if (gc_GetCallInfo(port[channum].crn, DESTINATION_ADDRESS, &(isdninfo[channum].dnis[0])) != GC_SUCCESS) {
        disp_msg("gc_GetCallInfo() error!");
        sprintf(isdninfo[channum].dnis, "00000");
        // If for some reason we can't get the destination (a null destination will not fail), let's write a filler destination and log the error.
        gc_errprint("gc_GetCallInfo_dnis", channum, -1);
    }

    else {
        sprintf(tmpbuff, "Incoming dest. is %s", isdninfo[channum].dnis);
        disp_msg(tmpbuff);
    }

    // If there's no CPN, this function will return an error on Springware boards
    // DM3 boards, however, will just silently keep on going.
    if (gc_GetCallInfo(port[channum].crn, ORIGINATION_ADDRESS, &(isdninfo[channum].cpn[0])) != GC_SUCCESS) {
        disp_msg("gc_GetCallInfo() error!");
        gc_errprint("gc_GetCallInfo_cpn", channum, -1);
        sprintf(isdninfo[channum].cpn, "631");
    }

    else {
        if (strlen(isdninfo[channum].cpn) > 0) {
            sprintf(tmpbuff, "Incoming CPN is %s", isdninfo[channum].cpn);
            disp_msg(tmpbuff);
        }

        // To do: use snprintf to truncate this if it's too long. Read some specs and see what flavor allows the longest destination.

    }

    // U_IES should be used to get the information elements. They will however, be unformatted. We should fix that here.


    if (gc_GetSigInfo(ldev, (char *)&info_elements[channum], U_IES, &metaevent) != GC_SUCCESS) {
        disp_msg("gc_GetSigInfo() error!");
        gc_errprint("gc_GetSigInfo", channum, -1);
    } else {
        // Dump the IE to a log. For development purposes. Make this something a flag in the
        // program will do at some point; it's completely unnecessary under normal circumstances.
        if (q931debug == 1) {
            FILE  *iefd;
            char iefile[180]; // This is a bit short; the maximum space for CPN alone is 128 bytes. The actual spec has shorter destinations, but still, this leaves you at risk of overflow
            sprintf(iefile, "%s-%i-%ld.dump", isdninfo[channum].dnis, channum, port[channum].crn);
            iefd = fopen(iefile, "a");
            //fprintf( iefd, "%s", info_elements[channum].data );
            //fwrite(info_elements[channum].data, sizeof(char), info_elements[channum].length, iefd);
            fwrite(&info_elements[channum], 1, sizeof(info_elements[channum]), iefd); // This should return size of IE dump in first two bytes, little endian format
            fclose(iefd);
        }

        // Write signaling data to RAM
        // while ( retcode == 1 ) {
        // length = info_elements[channum].data[(offset + 1)];
        // offset = (offset + 2); // This is probably invalid. Take another look when your head isn't in your ass.
        retcode = (writesig(offset, channum));
        offset = retcode;

        while (offset > 0) {
            if (offset >= info_elements[channum].length) break; // Make sure we don't extend past the (supposed) length of the buffer
            retcode = (writesig(offset, channum));
            offset = retcode;
            sprintf(tmpbuff, "Offset is %u", offset);
            disp_msg(tmpbuff);
        }

        // offset = (offset + length);
        // }
    }

    if (isdn_accept(channum) == -1) {
        disp_msg("Holy shit! isdn_accept() failed! D:");
        return (-1);
    }

    return (0);

}

/***********************************************************
                 Detect call progress event
***********************************************************/
/* Placeholder event handler */

char isdn_progresshdlr() {
    int  ldev = sr_getevtdev();
    short  lnum = get_linechan(ldev);

    sprintf(tmpbuff, "Call progress indicator received on %i", lnum);
    disp_msg(tmpbuff);

    return (0);
}

/***********************************************************
                Detect call proceeding event
***********************************************************/
/* Placeholder event handler */

char isdn_proceedinghdlr() {
    int  ldev = sr_getevtdev();
    short  lnum = get_linechan(ldev);

    disp_status(lnum, "Outgoing call - Proceeding");
    return (0);
}

/***********************************************************
                       Answer a call
***********************************************************/

char isdn_answerhdlr()

{
    int  ldev = sr_getevtdev();
    short  channum = get_linechan(ldev);
    sprintf(tmpbuff, "Call answered! Routing to places. Current state is %d", dxinfox[channum].state);
    disp_msg(tmpbuff);

    return (0);

}

/***********************************************************
                   Task Failure Handler
***********************************************************/
char isdn_failhdlr()

{
    // int  ldev = sr_getevtdev();
    METAEVENT failevent;
    GC_INFO failinfo;
    int errnum;
    errnum = gc_ResultInfo( &failevent, &failinfo );
        if (errnum != 0) {
            int  ldev = sr_getevtdev();
            short  channum = get_linechan(ldev);
            sprintf(tmpbuff, "CRITICAL ERROR %d: Cannot get info for ISDN task failure! Channel will remain out of service", errnum);
            disp_msg(tmpbuff);
            sprintf(tmpbuff, "DEBUG: ISDN task failure occurred for channel %d, connected to channel %d\n", channum, connchan[channum]);
            disp_msg(tmpbuff);
            if( ( connchan[channum] != 0) && (connchan[channum] < MAXCHANS) ) {
                dxinfox[ connchan[ channum ] ].state = ST_REORDER;
                playtone_rep( connchan[channum], 480, 620, -25, -27, 25, 25, 40 );
            }
            if (gc_ResetLineDev(port[channum].ldev, EV_ASYNC) == GC_SUCCESS) {
                disp_msg("Taskfail recovery: resetting line device...");
            }
            return(-1);
    }
    FILE  *errfd;
    int ldev = failevent.evtdev;
    short  channum = get_linechan(ldev);

    disp_msg("ISDN task failed!");
    
    if (failinfo.gcValue == GCRV_NONRECOVERABLE_FATALERROR) {
        disp_msg("CRITICAL ERROR: Non-recoverable error encountered! Application must be restarted!");
    }
    
    else {
        disp_msg("Opening error file");
        errfd = fopen("isdn_error.txt", "a+");
        sprintf(tmpbuff, "%s - Printing to error file, fd %i", failinfo.gcMsg, (int)errfd);
        disp_msg(tmpbuff);
        fprintf(errfd, "Task fail handler: GC ErrorValue: 0x%hx - %s, CC ErrorValue: 0x%lx - %s, additional info: %s, channel %d, program state %d, CRN %ld, ldev %li\n", failinfo.gcValue, failinfo.gcMsg, failinfo.ccValue, failinfo.ccMsg, failinfo.additionalInfo, channum, dxinfox[channum].state, port[channum].crn, port[channum].ldev);
        disp_msg("Closing error file");
        fclose(errfd);
        
        if (gc_ResetLineDev(port[channum].ldev, EV_ASYNC) == GC_SUCCESS) {
            disp_msg("Taskfail recovery: resetting line device...");
        }
        else {
            disp_msg("WARNING: Couldn't reset line device from fatal error state! Will be pulled out of service.");
        }
    }
    
    dxinfox[ connchan[ channum ] ].state = ST_REORDER;
    playtone_rep( connchan[channum], 480, 620, -25, -27, 25, 25, 40 );
    //dxinfox[ channum ].state = ST_WTRING; // No call is active, so send the channel back to an idle state
    return 0;
    //exit(2);
}

/***********************************************************
         Outgoing Call Connection - Answer message
***********************************************************/
char isdn_connecthdlr() {
    int ldev = sr_getevtdev();
    short channum = get_linechan(ldev);
    isdninfo[ channum ].status = 2; // Indicate outgoing call has suped to application

    disp_status(channum, "Outgoing call - Answered");

    if (cutthrough[ channum ] == 0) {

            if (nr_scunroute(dxinfox[ connchan[channum] ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].chdev, SC_VOX, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "Holy shit! SCUnroute1 threw an error! State %d", dxinfox[ connchan[channum] ].state);
                disp_err(channum, dxinfox[ connchan[channum] ].chdev, tmpbuff);
                return (-1);
            }

            if (nr_scunroute(dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP) == -1) {
                sprintf(tmpbuff, "Holy shit! SCUnroute2 threw an error! State %d", dxinfox[ channum ].state);
                disp_err(channum, dxinfox[ channum ].chdev, tmpbuff);
                return (-1);
            }

        if (nr_scroute(dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].tsdev, SC_DTI, SC_FULLDUP) == -1) {
            sprintf(tmpbuff, "Uhh, I hate to break it to you man, but SCRoute is shitting itself - state %d", dxinfox[ channum ].state);
            disp_err(channum, dxinfox[ channum ].chdev, tmpbuff);
            return (-1);
        }

    }
    cutthrough[channum] = 2;
    if (fxo[connchan[channum]] == 0 ) {
        if (dt_settssigsim(dxinfox[connchan[channum]].tsdev, DTB_AON | DTB_BON | DTB_COFF | DTB_DON) != 0) disp_msg("ERROR: Polarity Reversal"); // Test. Reverse polarity for originating end.
    }
    return (0);
}

/***********************************************************
      Outgoing Call Status (timeout/no answer) Handler)
***********************************************************/

char isdn_callstatushdlr() {

    int  ldev = sr_getevtdev();
    short channum = get_linechan(ldev);
    sprintf(tmpbuff, "Outgoing call timed out or didn't answer on channel %d", channum);
    disp_msg(tmpbuff);
    return (0);
}

bool check_crn(short channum) {
    // gc_GetNetCRV() is supposed to be deprecated, but they don't offer a replacement for the DM3. WHICH IS IT!? D:
    int crn;

    // if ( dm3board == TRUE ) {
    if (gc_GetNetCRV(port[channum].crn, &crn) != GC_SUCCESS) {
        disp_msg("check_crn operation failed!");
        gc_errprint("gc_GetNetCRV", channum, -1);
        return (FALSE);
    }

    sprintf(tmpbuff, "NetCRV for channel %d is %d", channum, crn);
    disp_msg(tmpbuff);
    return (TRUE);
}

/***********************************************************
          Handle an accepted call - report progress
***********************************************************/

int isdn_accepthdlr() {
    int  ldev = sr_getevtdev();
    short  channum = get_linechan(ldev);

    // Corrupt progress testing
    check_crn(channum);
    // set_corruptprog(channum, 1);
    //set_corruptprog(channum, 1);
    // Send the call to the ISDN digit processor in the voice routine

    if (isdn_inroute(channum) == -1) {
        sprintf(tmpbuff, "Inbound call routing failed on channel %d!", channum);
        disp_msg(tmpbuff);
        return (-1);
    }

    disp_msg("ISDN call accepted successfully");
    return (0);

}

/***********************************************************
               ISDN extension event handler
***********************************************************/
bool isdn_extension()

{
    // Note: This event should only be invoked by JCT cards. At least in theory.
    METAEVENT           metaevt;

    if (gc_GetMetaEvent(&metaevt) != GC_SUCCESS) {
        disp_msg("gc_GetMetaEvent failed on GCEV_EXTENSION event. That's... that's bad.");
        return (-1);
    }

    int ldev = metaevt.evtdev; // This'll be more efficient; no need to call the same function twice.
    short channum = get_linechan(ldev);

    // unsigned char ext_id = ((EXTENSIONEVTBLK*)(metaevt.extevtdatap))->ext_id;
    // This is sort of a textbook implementation, but whatever. It (hopefully) works, we'll change it as needed.
    switch (((EXTENSIONEVTBLK *)(metaevt.extevtdatap))->ext_id) {

        case GCIS_EXEV_STATUS:
            // Some switches seem to send this for whatever reason
            sprintf(tmpbuff, "STATUS message received from the network on channel %i! Keep debugging on to get it.", channum);
            disp_msg(tmpbuff);

            if (q931debug == 1) {
                port[channum].crn = metaevt.crn;

                if (gc_GetSigInfo(ldev, (char *)&info_elements[channum], U_IES, &metaevt) != GC_SUCCESS) {
                    disp_msg("gc_GetSigInfo_facilty error!");
                    gc_errprint("gc_GetSigInfo_facility", channum, -1);
                    return (-1);
                }

                FILE  *iefd;
                char iefile[180];
                sprintf(iefile, "%s-%i-%ld_status.dump", isdninfo[channum].dnis, channum, port[channum].crn);
                iefd = fopen(iefile, "a");
                // fprintf( iefd, "%s", info_elements[channum].data );
                //fwrite(info_elements[channum].data, sizeof(char), info_elements[channum].length, iefd);
                fwrite(&info_elements[channum], 1, sizeof(info_elements[channum]), iefd); // This should return size of IE dump in first two bytes, little endian format
                fclose(iefd);
            }

            break;

        case GCIS_EXEV_CONGESTION:
            // Use gc_GetCallInfo() in the future
            sprintf(tmpbuff, "CONGESTION message received via extension event on channel %i!", channum);
            disp_msg(tmpbuff);
            break;

        case GCIS_EXEV_CONFDROP:
            // A drop request has been received
            sprintf(tmpbuff, "DROP request received via extension event for channel %i! Event currently ignored.", channum);
            disp_msg(tmpbuff);
            break;

        case GCIS_EXEV_DIVERTED:
            sprintf(tmpbuff, "INFO: Call successfully diverted on channel %i", channum);
            disp_msg(tmpbuff);
            break;

        case GCIS_EXEV_DROPACK:
            sprintf(tmpbuff, "INFO: Network honored our DROP request on channel %i. Yay.", channum);
            disp_msg(tmpbuff);
            break;

        case GCIS_EXEV_DROPREJ:
            sprintf(tmpbuff, "INFO: Network rejected our DROP request on channel %i! RUDE! >:O", channum);
            disp_msg(tmpbuff);
            break;

        case GCIS_EXEV_FACILITY:
            sprintf(tmpbuff, "INFO: FACILITY message received on channel %i! No action taken; ensure debugging is enabled to receive.", channum);
            disp_msg(tmpbuff);
            port[channum].crn = metaevt.crn;

            if (q931debug == 1) {
                if (gc_GetSigInfo(ldev, (char *)&info_elements[channum], U_IES, &metaevt) != GC_SUCCESS) {
                    disp_msg("gc_GetSigInfo_facilty error!");
                    gc_errprint("gc_GetSigInfo_facility", channum, -1);
                    return (-1);
                }

                FILE  *iefd;
                char iefile[180];
                sprintf(iefile, "%s-%i-%ld_facility.dump", isdninfo[channum].dnis, channum, port[channum].crn);
                iefd = fopen(iefile, "a");
                // fprintf( iefd, "%s", info_elements[channum].data );
                //fwrite(info_elements[channum].data, sizeof(char), info_elements[channum].length, iefd);
                fwrite(&info_elements[channum], 1, sizeof(info_elements[channum]), iefd); // This should return size of IE dump in first two bytes, little endian format
                fclose(iefd);
            }

            break;


        default:
            sprintf(tmpbuff, "GCEV_EXTENSION event received with unknown ext_id %u, channel %i", ((EXTENSIONEVTBLK *)(metaevt.extevtdatap))->ext_id, channum);
            disp_msg(tmpbuff);
            break;
    }

    return (0);
}

char glare_hdlr() {
    int ldev = sr_getevtdev();
    short channum = get_linechan(ldev);
    sprintf(tmpbuff, "WARNING: Glare detected on channel %d", channum);
    disp_msg(tmpbuff);
    return (0);
}

/***********************************************************
                Actually open all channels
***********************************************************/

void isdn_open() {
    char dtiname[ 32 ];
    char d4xname[ 32 ];
    char linedti[ 42 ]; // This can be 32 just like the rest - and functionally, it's impossible to overflow, but this makes the compiler happy.
    short channum = startchan;
    // For IE receive buffer size setting
    GC_PARM parm = {0};
    parm.shortvalue = 10;

    while (channum <= isdnmax) {
    //for (channum = 1; channum <= maxchan; channum++) {

        if ((channum % 24) == 0) {
            // Is there a better way to do this? I'd rather not do so many division operations...
            fxo[channum] = -1;
            startchan++;
            continue;
        }

        sprintf(tmpbuff, "Init %i", channum);
        disp_msg(tmpbuff);

        // Initialize settings for information elements
        raw_info_elements[channum].gclib = NULL; // That's what they say to do...
        raw_info_elements[channum].cclib = &info_elements[channum];

        /* Initial channel state is blocked */
        port[channum].blocked = 1; // 1 = Yes, 0 = No

        // I don't see a better way of doing it, but the division bothers me...

        sprintf(dtiname, "dtiB%dT%d", (channum > 24) ? (altbdnum + (channum / 24)) : altbdnum, (channum > 24) ? (channum - (24 * (channum / 24))) : channum);
        sprintf(d4xname, "dxxxB%dC%d", (channum % 4) ? (channum / 4) + isdnbdnum : isdnbdnum + (channum / 4) - 1, (channum % 4) ? (channum % 4) : 4);

        sprintf(linedti, ":N_%s:P_ISDN", dtiname);

        disp_msg("gc_OpenEx init");
        disp_msg(dtiname);
        disp_msg(d4xname);
        disp_msg(linedti);

        // memset(&callack[channum], 0, sizeof(callack[channum]));
        // callack[channum].type = GCACK_SERVICE_INFO;
        // callack[channum].service.info.info_type = DESTINATION_ADDRESS;
        // callack[channum].service.info.info_len = 5; // Five digit DNIS. Maybe don't hardcode this?

        // Open the GlobalCall devices. We should probably modify this for asynchronous execution later.
        if (gc_OpenEx(&port[channum].ldev, linedti, EV_SYNC, (void *)&port[channum]) != GC_SUCCESS) {
            sprintf(tmpbuff, "Uh, we have a problem here. gc_OpenEx dun goofed on channel %d", dxinfox[channum].tsdev);
            disp_msg(tmpbuff);
            gc_errprint("gc_OpenEX", channum, -1);
            sr_release(); // To do: figure out why this is needed/a good idea
            exit(2);
        }

        sprintf(tmpbuff, "%s gc_OpenEx success", linedti);
        disp_msg(tmpbuff);

        // Now for the tricky part...
        // Should we insert voice parameters into the code at a later point?
        // Note all these functions are synchronous. This isn't a problem for you people, is it? It's just a setup function...

        disp_status(channum, "ISDN channel not ready - blocked state");


        // Make sure this works on JCT architecture cards, plz&thx2u
        // (It seems to)

        if (gc_SetParm(port[channum].ldev, RECEIVE_INFO_BUF, parm) != GC_SUCCESS) {
            disp_msg("Fuck, we're hosed. RECEIVE_INFO_BUF setting failed.");
            exit(2);
        }

        // Finish voice routing semi-normally

        if ((dxinfox[ channum ].chdev = dx_open(d4xname, 0)) == -1) {
            sprintf(tmpbuff, "Unable to open channel %s, errno = %d",
                    d4xname, errno);
            disp_msg(tmpbuff);
            exit(2);
        }

        sprintf(tmpbuff, "chdev %i opened", dxinfox[channum].chdev);
        disp_msg(tmpbuff);

        if (dm3board == TRUE) {
            // JCT boards don't support this for ISDN operations; nr_scroute is sufficient.
            if (gc_AttachResource(port[ channum ].ldev, dxinfox[ channum ].chdev, NULL, NULL, GC_VOICEDEVICE, EV_SYNC) != GC_SUCCESS) {
                gc_errprint("gc_AttachResource", channum, -1);
                sprintf(tmpbuff, "gc_AttachResource failed on channel %d!", channum);
                disp_msg(tmpbuff);
                exit(2);
            }
        }

        if (gc_GetResourceH(port[channum].ldev, &dxinfox[ channum ].tsdev, GC_NETWORKDEVICE) != GC_SUCCESS) {
            gc_errprint("gc_GetResourceH(GC_NETWORKDEVICE)", channum, -1);
            exit(2);
        }

        sprintf(tmpbuff, "tsdev %i assigned", dxinfox[channum].tsdev);
        disp_msg(tmpbuff);

        sprintf(tmpbuff, "ldev %li assigned", port[channum].ldev);
        disp_msg(tmpbuff);

        nr_scunroute(dxinfox[ channum ].tsdev, SC_DTI,
                     dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP);


        if (nr_scroute(dxinfox[ channum ].tsdev, SC_DTI,
                       dxinfox[ channum ].chdev, SC_VOX, SC_FULLDUP)
                == -1) {
            sprintf(tmpbuff, "nr_scroute() failed for %s - %s",
                    ATDV_NAMEP(dxinfox[ channum ].chdev),
                    ATDV_NAMEP(dxinfox[ channum ].tsdev));
            disp_msg(tmpbuff);
            exit(2);
        }

//          }

        fxo[channum] = 2;
        
        sprintf(tmpbuff, "Channel %i is ready for calls!", channum);
        disp_msg(tmpbuff);
        channum++;

    }

    return;
}

void isdn_prep() {

    GC_START_STRUCT gclib_start;    // Struct for gc_start(). The Dialogic people like their weird typedefs. This is in gclib.h .
    char dtiname[10]; // For board deriving function
    int tsdev; // For board deriving function
    CT_DEVINFO ctdevinfo;

    // The next part of this routine determines what sort of board we're using.
    sprintf(dtiname, "dtiB%dT1", altbdnum);
    disp_msg(dtiname);

    if ((tsdev = dt_open(dtiname, 0)) == -1) {
        disp_msg("Couldn't open board for type probing! Exiting.");
        exit(2);
    }

    if (dt_getctinfo(tsdev, &ctdevinfo) == -1) {
        sprintf(tmpbuff, "Error message = %s", ATDV_ERRMSGP(tsdev));
        disp_msg(tmpbuff);
        exit(2);
    }

    if (dt_close(tsdev) == -1) {
        disp_msg("Weird. Couldn't close timeslot. This is bad.");
        exit(2);
    }

    if (ctdevinfo.ct_devfamily == CT_DFDM3) {
        dm3board = TRUE;
    }

    // boardtyp = ctdevinfo.ct_devfamily;

    // Can we do this differently? The compiler expects a non-empty struct by the time
    // gclib_start.cclib_list = cclib_Start is applied, and a catch-all else statement
    // won't satisfy it. \/

    CCLIB_START_STRUCT cclib_Start[] = {
        // Ayup. Just ISDN. Fuck you and your weird protocols.
        {"GC_ISDN_LIB", NULL},
    };

    if (dm3board == TRUE) {
        disp_msg("Man dang, this here be a DM3!");
        cclib_Start->cclib_name = "GC_DM3CC_LIB";
        // Apparently there's one library for all DM3 protocols. Heh.
    } else {
        disp_msg("This whoozit is a JCT. Fancy.");
    }

    gclib_start.num_cclibs = 1; // Only one library needed; the ISDN one
    gclib_start.cclib_list = cclib_Start; // List of needed call control libraries; namely, just the ISDN one.

    disp_msg("isdn_init start");
    
    if (gc_Start(&gclib_start) != GC_SUCCESS) {
        gc_errprint("gc_Start", 0, -1);
        gc_Stop();
//    return(gc_error_info.gcValue);
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_UNBLOCKED, (EVTHDLRTYP)isdn_unblock) == -1) {
        disp_msg("Unable to set-up the GlobalCall unblock handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_BLOCKED, (EVTHDLRTYP)isdn_block) == -1) {
        disp_msg("Unable to set-up the GlobalCall block handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, CCEV_SETCHANSTATE, (EVTHDLRTYP) setchanstate_hdlr) == -1 ) {
	disp_msg( "Unable to set-up the channel state setting handler" );
	exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_ACCEPT, (EVTHDLRTYP)isdn_accepthdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Accept handler");
        exit(2);
    }

    // To do: make this work for DM3 devices too:
    if (sr_enbhdlr(EV_ANYDEV, GCEV_FACILITY, (EVTHDLRTYP)isdn_facilityhdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Accept handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_OFFERED, (EVTHDLRTYP)isdn_offerhdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Offer handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_PROCEEDING, (EVTHDLRTYP)isdn_proceedinghdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Offer handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_DROPCALL, (EVTHDLRTYP)isdn_drophdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Drop handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_DISCONNECTED, (EVTHDLRTYP)isdn_discohdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall DISCO handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_CALLPROGRESS, (EVTHDLRTYP)isdn_progresshdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Progress handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_ANSWERED, (EVTHDLRTYP)isdn_answerhdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Answer handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_RELEASECALL, (EVTHDLRTYP)isdn_releasehdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Release handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_CONNECTED, (EVTHDLRTYP)isdn_connecthdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Connect handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_CALLSTATUS, (EVTHDLRTYP)isdn_callstatushdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Release handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_PROGRESSING, (EVTHDLRTYP)isdn_progressing) == -1) {
        disp_msg("Unable to set-up the GlobalCall Progress handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_ALERTING, (EVTHDLRTYP)isdn_progressing) == -1) {
        disp_msg("Unable to set-up the GlobalCall Alerting handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_TASKFAIL, (EVTHDLRTYP)isdn_failhdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Failure handler");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, GCEV_EXTENSION, (EVTHDLRTYP)isdn_extension) == -1) {
        disp_msg("Unable to set-up the GlobalCall Extension handler");
        exit(2);
    }

    // This last one is a test handler

    if (sr_enbhdlr(EV_ANYDEV, GCEV_MEDIADETECTED, (EVTHDLRTYP)isdn_mediahdlr) == -1) {
        disp_msg("Unable to set-up the GlobalCall Media handler");
        exit(2);
    }
    
    if (sr_enbhdlr(EV_ANYDEV, GCEV_RESETLINEDEV, (EVTHDLRTYP)isdn_reset) == -1) {
        disp_msg("Unable to set-up the GlobalCall line reset handler");
        exit(2);
    }    
    
    if (sr_enbhdlr(EV_ANYDEV, GCEV_RESTARTFAIL, (EVTHDLRTYP)isdn_reset) == -1) {
        disp_msg("Unable to set-up the GlobalCall line reset failure handler");
        exit(2);
    }
    
    if (sr_enbhdlr(EV_ANYDEV, GCEV_SETCONFIGDATA, (EVTHDLRTYP)config_successhdlr) == -1) {
        disp_msg("Unable to set up the Globalcall configuration success handler. HOW WILL WE EVER HAVE SUCCESS AGAIN!? D:");
        exit(2);
    }

    if (sr_enbhdlr(EV_ANYDEV, EGC_GLARE, (EVTHDLRTYP)glare_hdlr) == -1) {
        disp_msg("Unable to set-up the Glare handler");
        exit(2);
    }

    isdn_open();
    return;

}

/***********************************************************
             Identify a line device for an event
***********************************************************/
short get_linechan(int linedev)

{
    short linenum = startchan;

    // This is going to bug me; it's crude and really should
    // be a simple translation table instead of this bruteforce
    // garbage.

    while (linenum <= isdnmax) {
        if (port[linenum].ldev == linedev)  {
            return (linenum);
        } else {
            linenum++;
        }
    }

    /*
     * Not Found in the Array, print error and return -1
     */
    sprintf(tmpbuff, "Unknown Event for Line %d - Ignored", linedev);
    disp_msg(tmpbuff);

    return (-1);
}

/***********************************************************
                    Drop an ISDN call
***********************************************************/

char isdn_drop(short channum, int cause)

{
    int callstate;
    isdninfo[channum].status = 1;

    // disp_status( channum, "Dropping call - pre-null check");
    if (port[channum].crn == '\0') {
        sprintf(tmpbuff, "channel %d got isdn_drop on idle call. Responsible state is %d", channum, dxinfox[channum].state);
        disp_msg(tmpbuff);
        return (0);
        // Is the software trying to hang up on a call that doesn't exist?
        // Just smile and nod. And tell the user they're being stupid.

    }

    if (gc_GetCallState(port[channum].crn, &callstate) != GC_SUCCESS) {
        // This is a debug message. You can probably erase it. That being said, this function is part of diagnosing
        // a rather nasty bug where calls fail to release.
        // If this fails, it indicates a serious problem. Should we reset the line device?
        gc_errprint("gc_DropCall1", channum, callstate);
        disp_msg("Disconnected call passed to isdn_drop function. Ignoring...");
        return (-1);
    }

    if (callstate == 0x20) {
        // Call is already idle. If it was dropped/idle, the drop_hdlr will release it for us.
        return (0);
    }

    sprintf(tmpbuff, "Channel dropped via isdn_drop, cause value %d", cause);
    disp_status(channum, tmpbuff);

    if (gc_DropCall(port[channum].crn, cause, EV_ASYNC) != GC_SUCCESS) {
        sprintf(tmpbuff, "Call won't drop in callstate %d. Attempting to release...", callstate);
        disp_msg(tmpbuff);

        if (gc_ReleaseCallEx(port[channum].crn, EV_ASYNC) != GC_SUCCESS) {
            disp_msg("Call won't release. This thing is clingy...");
            gc_errprint("gc_ReleaseCallEx2", channum, callstate);
        }

        return (-1);
    }

    dxinfox[channum].state = ST_WTRING;

    return (0);
}

/***********************************************************
          Answer an incoming call - send offhook
***********************************************************/

int isdn_answer(short channum) {

    disp_msg("Answering ISDN call");

    if (gc_AnswerCall(port[channum].crn, 0, EV_ASYNC) != GC_SUCCESS) {
        gc_errprint("gc_AnswerCall", channum, -1);
        return (-1);
    }

    return (0);

}

/***********************************************************
      Mark a released channel as idle, await new calls
***********************************************************/
int isdn_waitcall(short channum) {
    sprintf(tmpbuff, "Calling isdn_waitcall in state %d! Attempting ", dxinfox[ channum ].state);
    disp_msg(tmpbuff);

    if (gc_WaitCall(port[channum].ldev, NULL, 0, 0, EV_ASYNC) == -1) {
        gc_errprint("gc_WaitCall", channum, -1);
        return (-1);
    }

    cutthrough[ channum ] = 0;

    return (0);
}

/***********************************************************
             Write Q.931 signaling data to RAM
***********************************************************/
char writesig(unsigned char offset, short channum) {
    unsigned char type;
    unsigned char length;
    char returncode = 0; // Verify we actually need returncode; I did this with a migraine.
    unsigned char offset2 = 0;
    unsigned char extra = 0;
    type = info_elements[channum].data[offset];
    sprintf(tmpbuff, "Writesig type %u", type);
    disp_msg(tmpbuff);

    switch (type) {
        // REMINDER: MAXIMUM LENGTH OF AN IE IN THE SPEC SHOULD BE CHECKED TO PREVENT OVERFLOWS.
        // THIS IS ALL CAPS LEVELS OF IMPORTANT.

        // Following data is custom to network. We don't need
        // to handle anything else. Oh, or null data.
        case 0x96:
            disp_msg("Codeset 6 locking IE received!");
            offset++;
            returncode = offset;
            break;
            
        case 0x1C:
            disp_msg("Facility IE received! What the fucking hatbowls!?");
            offset++;
            length = info_elements[channum].data[offset];
            offset++; // Data starts here.
            returncode = offset + length;
            break;

        case 0x04:
            // Bearer cap IE
            disp_msg("Writing bearer cap IE");
            offset++;
            length = info_elements[channum].data[offset];

            // Have a length limiter here if it's out of spec. We don't need buffer overflows.
            if (length > 12) {
                disp_msg("Warning: invalid BCAP length. Truncating to 12 bytes.");
                extra = (length - 12);
                length = 12;
            }

            sprintf(tmpbuff, "Length is %u", length);
            disp_msg(tmpbuff);
            offset++;

            while (length > 0) {
                isdninfo[channum].bcap[offset2] = info_elements[channum].data[offset]; // Is there any way to do this with less variables?
                length--;
                offset++;
                offset2++;
            }

            sprintf(tmpbuff, "BCAP is %02hhX %02hhX %02hhX", isdninfo[channum].bcap[0], isdninfo[channum].bcap[1], isdninfo[channum].bcap[2]);
            disp_msg(tmpbuff);
            returncode = (offset + extra); // We shouldn't have to do addition all the time. This should keep the handler on track in case of an (unlikely) overflow, though.
            break;

        case 0x18:
            // Channel ID IE
            disp_msg("Writing channel ID IE");
            offset++;
            length = info_elements[channum].data[offset];

            if (length > 6) {
                disp_msg("Warning: invalid channel ID length. Truncating to 6 bytes.");
                extra = (length - 6);
                length = 6;
            }

            sprintf(tmpbuff, "Length is %u", length);
            disp_msg(tmpbuff);
            offset++;

            while (length > 0) {
                isdninfo[channum].chanid[offset2] = info_elements[channum].data[offset];
                length--;
                offset++;
                offset2++;
            }

            sprintf(tmpbuff, "CHANID is %02hhX %02hhX %02hhX", isdninfo[channum].chanid[0], isdninfo[channum].chanid[1], isdninfo[channum].chanid[2]);
            disp_msg(tmpbuff);
            returncode = (offset + extra);
            break;

        case 0x1E:
            // Progress indicator IE
            disp_msg("Writing progress indicator IE");
            offset++;
            length = info_elements[channum].data[offset];

            if (length > 4) {
                disp_msg("Warning: invalid channel ID length. Truncating to 4 bytes.");
                extra = (length - 4);
                length = 4;
            }

            sprintf(tmpbuff, "Length is %u", length);
            disp_msg(tmpbuff);
            offset++;

            while (length > 0) {
                isdninfo[channum].progind[offset2] = info_elements[channum].data[offset];
                length--;
                offset++;
                offset2++;
            }

            sprintf(tmpbuff, "PROG is %02hhX %02hhX", isdninfo[channum].progind[0], isdninfo[channum].progind[1]);
            disp_msg(tmpbuff);
            returncode = (offset + extra);
            break;


        case 0x6C:
            disp_msg("Writing CPN screen bit.");
            offset++;
            length = info_elements[channum].data[offset];
            offset++;
            isdninfo[channum].callingtype = info_elements[channum].data[offset];
            sprintf(tmpbuff, "CITYP is %02hhX", isdninfo[channum].callingtype);
            disp_msg(tmpbuff);

            if (isdninfo[channum].callingtype && 0x80) {   // Screening/presentation bits sent
                offset++;
                length--;
                isdninfo[channum].prescreen = info_elements[channum].data[offset];
                sprintf(tmpbuff, "CSCRN is %02hhX", isdninfo[channum].prescreen);
                disp_msg(tmpbuff);
            } else {
                isdninfo[channum].prescreen = 0x00;    // This'll never be all zeroes in real life. We'll use this to internally notate a lack of a presentation/screen bit.
            }

            returncode = (offset + length);
            break;

        case 0x70:
            disp_msg("Called number field received. Grabbing number type...");
            // Called party number
            offset++;
            length = info_elements[channum].data[offset];
            offset++;
            isdninfo[channum].calledtype = info_elements[channum].data[offset];
            sprintf(tmpbuff, "CTYP is %02hhX", isdninfo[channum].calledtype);
            disp_msg(tmpbuff);
            returncode = (offset + length);
            break;

        case 0x28:
            disp_msg("Display IE received!");
            // Calling party name. Just skip through this for now; we don't actually need it.
            offset++;
            length = info_elements[channum].data[offset];

            if ((length > 82) || (length > (260 - offset))) {         // Lots of (basic) arithmetic here. Can we fix that?
                disp_msg("ERROR: Display IE exceeds frame and/or standard length! Skipping...");
                returncode = 0;
                break;
            }

            offset++;
            sprintf(tmpbuff, "Display length is %d", length);
            disp_msg(tmpbuff);
            strncpy(isdninfo[channum].displayie, (info_elements[channum].data + offset), length);
            memcpy((isdninfo[channum].displayie + length + 1), "\x00", 1);   // Stick null terminator at the end of the string. Is this necessary?
            returncode = (offset + length);
            sprintf(tmpbuff, "Return code is %d", returncode);
            disp_msg(tmpbuff);
            sprintf(tmpbuff, "Display IE is %s", isdninfo[channum].displayie);
            disp_msg(tmpbuff);
            break;

        case 0x73:
            // Original Called Number. Basically, RDNIS but not.
            disp_msg("Original Called Number received. Thanks, Nortel -_-");
            offset++;
            length = info_elements[channum].data[offset];
            if (( length > 18) || (length > (260 - offset))) {
                disp_msg("ERROR: Original Called Number IE exceeds frame and/or standard length! Skipping...");
                returncode = 0;
                break;
            }
            offset++;
            isdninfo[channum].forwardedtype = info_elements[channum].data[offset];
            switch ( isdninfo[channum].forwardedtype & 0x70 ) {
                case 0x40:
                    disp_msg("DEBUG: Original called number is subscriber type");
                    break;
                case 0x20:
                    disp_msg("DEBUG: Original called number is national type");
                    break;
                case 0x10:
                    disp_msg("DEBUG: Original called number is international type");
                    break;
                case 0x00:
                    disp_msg("DEBUG: Original called number is unknown type");
                    break;
                default:
                    disp_msg("DEBUG: Original called number has malformed number type field");
                    
            }
            switch ( isdninfo[channum].forwardedtype & 0x0F ) {
                case 0x00:
                    disp_msg("DEBUG: Original called number has unknown numbering plan");
                    break;
                case 0x01:
                    disp_msg("DEBUG: Original called number has ISDN numbering plan");
                    break;
                case 0x09:
                    disp_msg("DEBUG: Original called number has private numbering plan");
                    break;
                default:
                    disp_msg("DEBUG: Original called number has malformed numbering plan");
            }
            offset++;
            offset2++;
            if (!(isdninfo[channum].forwardedtype & 0x80)) { //Extension bit low? Move to Octet 3A
                isdninfo[channum].forwardedscn = info_elements[channum].data[offset];
                switch ( isdninfo[channum].forwardedscn & 0x60 ) {
                    case 0x00:
                        disp_msg("DEBUG: Original called number presentation is allowed");
                        break;
                    case 0x20:
                        disp_msg("DEBUG: Original called number presentation isn't allowed");
                        break;
                    case 0x40:
                        disp_msg("DEBUG: Original called number isn't available");
                        break;
                    default:
                        disp_msg("DEBUG: Original called number presentation bits are malformed");
                }
                
                switch ( isdninfo[channum].forwardedscn & 0x03 ) {
                    case 0x00:
                        disp_msg("DEBUG: Original called number is user provided, not screened");
                        break;
                    case 0x01:
                        disp_msg("DEBUG: Original called number is user provided, verified and passed");
                        break;
                    case 0x02:
                        disp_msg("DEBUG: Original called number is user provided, verified and failed");
                        break;
                    case 0x03:
                        disp_msg("DEBUG: Original called number is network provided");
                }
                offset++;
                offset2++;
                if (!(isdninfo[channum].forwardedscn & 0x80)) { // Process Octet 3B
                    isdninfo[channum].forwardedrsn = info_elements[channum].data[offset];
                    switch ( isdninfo[channum].forwardedrsn & 0x0F ) {
                    case 0x00:
                        disp_msg("DEBUG: Original called number forward reason is unknown");
                        break;
                    case 0x01:
                        disp_msg("DEBUG: Original called number forward reason is call forward busy");
                        break;
                    case 0x02:
                        disp_msg("DEBUG: Original called number forward reason is call forward no reply");
                        break;
                    case 0x0D:
                        disp_msg("DEBUG: Original called number forward reason is call transfer");
                        break;
                    case 0x0E:
                        disp_msg("DEBUG: Original called number forward reason is call pickup");
                        break;
                    case 0x0F:
                        disp_msg("DEBUG: Original called number forward reason is call forwarding unconditional");
                        break;
                    default:
                        sprintf( tmpbuff, "DEBUG: Original called number forward reason is unknown: 0x%x", isdninfo[channum].forwardedtype );
                        disp_msg(tmpbuff);
                }
                    offset++;
                    offset2++;
                    if (info_elements[channum].data[offset] & 0x80) { // Process Octet 3C
                        isdninfo[channum].forwarddata = info_elements[channum].data[offset];
                        offset++;
                        offset2++;
                        strncpy(isdninfo[channum].forwardednum, (info_elements[channum].data + offset), (length - 4) );
                        memcpy(isdninfo[channum].forwardednum + (length - 3), "\x00", 1);
                    }
                    else {
                        isdninfo[channum].forwarddata = 0x81;
                        strncpy(isdninfo[channum].forwardednum, (info_elements[channum].data + offset), (length - 3) );
                        memcpy(isdninfo[channum].forwardednum + (length - 2), "\x00", 1);
                    }
                }
                else {
                    isdninfo[channum].forwarddata = 0x81;
                    isdninfo[channum].forwardedrsn = 0x00;
                    strncpy(isdninfo[channum].forwardednum, (info_elements[channum].data + offset), (length - 2));
                    memcpy(isdninfo[channum].forwardednum + (length - 1), "\x00", 1);
                }
            }
            else {
                isdninfo[channum].forwarddata = 0x81;
                isdninfo[channum].forwardedscn = 0x00;
                isdninfo[channum].forwardedrsn = 0x00;
                strncpy(isdninfo[channum].forwardednum, (info_elements[channum].data + offset), (length - 1));
                memcpy(isdninfo[channum].forwardednum + (length), "\x00", 1);
            }
            sprintf(tmpbuff, "DEBUG: ISDN original called number number is: %s", isdninfo[channum].forwardednum);
            disp_msg(tmpbuff);
            returncode = (offset + length - offset2);
            break;

        case 0x74:
            // RDNIS. Wait, we're actually getting this?
            disp_msg("RDNIS received. IT'S A MIRACLE!!!!ONE");
            offset++;
            length = info_elements[channum].data[offset];
            if (( length > 25) || (length > (260 - offset))) {
                disp_msg("ERROR: RDNIS IE exceeds frame and/or standard length! Skipping...");
                returncode = 0;
                break;
            }
            offset++;
            isdninfo[channum].forwardedtype = info_elements[channum].data[offset];
            switch ( isdninfo[channum].forwardedtype & 0x70 ) {
                case 0x40:
                    disp_msg("DEBUG: RDNIS has subscriber number");
                    break;
                case 0x20:
                    disp_msg("DEBUG: RDNIS has national number");
                    break;
                case 0x10:
                    disp_msg("DEBUG: RDNIS has international number");
                    break;
                case 0x00:
                    disp_msg("DEBUG: RDNIS has unknown number type");
                    break;
                default:
                    disp_msg("DEBUG: RDNIS has malformed number type field");
                    
            }
            switch ( isdninfo[channum].forwardedtype & 0x0F ) {
                case 0x00:
                    disp_msg("DEBUG: RDNIS has unknown numbering plan");
                    break;
                case 0x01:
                    disp_msg("DEBUG: RDNIS has ISDN numbering plan");
                    break;
                case 0x09:
                    disp_msg("DEBUG: RDNIS has private numbering plan");
                    break;
                default:
                    disp_msg("DEBUG: RDNIS has malformed numbering plan");
            }
            offset++;
            offset2++;
            if (!(isdninfo[channum].forwardedtype & 0x80)) { //Extension bit low? Move to Octet 3A
                isdninfo[channum].forwardedscn = info_elements[channum].data[offset];
                switch ( isdninfo[channum].forwardedscn & 0x60 ) {
                    case 0x00:
                        disp_msg("DEBUG: RDNIS presentation is allowed");
                        break;
                    case 0x20:
                        disp_msg("DEBUG: RDNIS presentation isn't allowed");
                        break;
                    default:
                        disp_msg("DEBUG: RDNIS presentation bits are malformed");
                }
                
                switch ( isdninfo[channum].forwardedscn & 0x03 ) {
                    case 0x00:
                        disp_msg("DEBUG: RDNIS number is user provided, not screened");
                        break;
                    case 0x01:
                        disp_msg("DEBUG: RDNIS number is user provided, verified and passed");
                        break;
                    case 0x02:
                        disp_msg("DEBUG: RDNIS number is user provided, verified and failed");
                        break;
                    case 0x03:
                        disp_msg("DEBUG: RDNIS number is network provided");
                }
                offset++;
                offset2++;
                if ((info_elements[channum].data[offset] & 0x80)) { // Process Octet 3B
                    isdninfo[channum].forwardedrsn = info_elements[channum].data[offset];
                    switch ( isdninfo[channum].forwardedrsn & 0x0F ) {
                    case 0x00:
                        disp_msg("DEBUG: RDNIS forward reason is unknown");
                        break;
                    case 0x01:
                        disp_msg("DEBUG: RDNIS forward reason is call forward busy");
                        break;
                    case 0x02:
                        disp_msg("DEBUG: RDNIS forward reason is call forward no reply");
                        break;
                    case 0x03:
                        disp_msg("DEBUG: RDNIS forward reason is call forward network busy");
                        break;
                    case 0x04:
                        disp_msg("DEBUG: RDNIS forward reason is call deflection");
                        break;
                    case 0x09:
                        disp_msg("DEBUG: RDNIS forward reason is called DTE out of order");
                        break;
                    case 0x0A:
                        disp_msg("DEBUG: RDNIS forward reason is call forwarding ordered by DTE");
                        break;
                    case 0x0F:
                        disp_msg("DEBUG: RDNIS forward reason is call forwarding unconditional or systemic redirection");
                        break;
                    default:
                        sprintf( tmpbuff, "DEBUG: RDNIS forward reason is unknown: 0x%x", isdninfo[channum].forwardedtype );
                        disp_msg(tmpbuff);
                }
                    offset++;
                    offset2++;
                    strncpy(isdninfo[channum].forwardednum, (info_elements[channum].data + offset), (length - 3) );
                    memcpy(isdninfo[channum].forwardednum + (length - 2), "\x00", 1);
                }
                else {
                    isdninfo[channum].forwardedrsn = 0x00;
                    strncpy(isdninfo[channum].forwardednum, (info_elements[channum].data + offset), (length - 2));
                    memcpy(isdninfo[channum].forwardednum + (length - 1), "\x00", 1);
                }
            }
            else {
                isdninfo[channum].forwardedscn = 0x00;
                isdninfo[channum].forwardedrsn = 0x00;
                strncpy(isdninfo[channum].forwardednum, (info_elements[channum].data + offset), (length - 1));
                memcpy(isdninfo[channum].forwardednum + (length), "\x00", 1);
            }
            sprintf(tmpbuff, "DEBUG: ISDN forwarded number is: %s", isdninfo[channum].forwardednum);
            disp_msg(tmpbuff);
            returncode = (offset + length - offset2);
            break;

        case 0xA1:
            disp_msg("Sending complete IE received - exiting IE parser");
            returncode = 0;
            break;

        // Should we implement the rest?

        case 0x00:
            disp_msg("Null terminator received - exiting IE parser");
            returncode = 0;
            break;

        default:
            sprintf(tmpbuff, "Unkn ISDN IE: %u, off %u", type, offset);
            disp_msg(tmpbuff);
            returncode = 0;
            break;

    }

    return (returncode);
}

/***************************************************************************
 *        NAME: char randomcpn()
 * DESCRIPTION: Function that generates a random calling party number. To
 *              simplify, this only generates phone numbers in old (pre-95)
 *              area codes, and in non-0xx/1xx exchanges.
 *       INPUT: short channum (channel to set with random CPN)
 *      OUTPUT: None.
 *     RETURNS: 0 on success, -1 on failure.
 *    CAUTIONS: None.
 **************************************************************************/

char randomcpn(short channum) {
    char cpn[11]; // Does this start with zero or one?
    cpn[0] = (random_at_most(8) + 0x32);
    cpn[1] = (random_at_most(1) + 0x30);
    cpn[2] = (random_at_most(8) + 0x32);
    cpn[3] = (random_at_most(8) + 0x32);
    cpn[4] = (random_at_most(10) + 0x30);
    cpn[5] = (random_at_most(10) + 0x30);
    cpn[6] = (random_at_most(10) + 0x30);
    cpn[7] = (random_at_most(10) + 0x30);
    cpn[8] = (random_at_most(10) + 0x30);
    cpn[9] = (random_at_most(10) + 0x30);
    cpn[10] = '\0';

    if (set_cpn(channum, cpn, (ISDN | NATIONAL), (PRES_ALLOW | NETSCREEN)) != GC_SUCCESS) {
        disp_msg("set_cpn returned an error!");
        return (-1);
    }

    return (0);
}

bool set_corruptie(short channum, char type) {
    GC_IE_BLK gc_corruptcpn_ie;
    IE_BLK corruptcpn_ie;
    unsigned char payload[152];
    memset(payload, 0, 152);

    switch (type) {

        case 1:
            memcpy(&payload, "\x6C\x00", 2);
            //memcpy(&payload, "\x6C\x14\x80", 3);   // Calling party number
            // unsigned char payload[] = { 0x6c, 0x14, 0xA1 }; // Calling party number
            // 0x6C indicates calling party number IE, 0xDB specifies a length of 219 octets (anything larger, and called party numbers over four digits overflow on the NEC),  0xA1 specifies national number, ISDN/telephony numbering plan
            corruptcpn_ie.length = 2;
            gc_corruptcpn_ie.gclib = NULL;
            gc_corruptcpn_ie.cclib = &corruptcpn_ie;
            memcpy(corruptcpn_ie.data, payload, corruptcpn_ie.length);

            if (gc_SetInfoElem(port[channum].ldev, &gc_corruptcpn_ie) != GC_SUCCESS) {
                gc_errprint("gc_SetInfoElem", channum, -1);
                return (FALSE);
            }

            disp_msg("gc_setcorruptIE OK!");
            return (TRUE);

            //break;

        case 2:
            // Change second byte back to x22!
            memcpy(&payload, "\x28\xE5\x00", 3);
            //unsigned char payload[] = { 0x28, 0x22, 0x00 }; //Display IE test
            corruptcpn_ie.length = 3;

            gc_corruptcpn_ie.gclib = NULL;
            gc_corruptcpn_ie.cclib = &corruptcpn_ie;
            memcpy(corruptcpn_ie.data, payload, corruptcpn_ie.length);

            if (gc_SetInfoElem(port[channum].ldev, &gc_corruptcpn_ie) != GC_SUCCESS) {
                gc_errprint("gc_SetInfoElem", channum, -1);
                return (FALSE);
            }

            disp_msg("gc_setcorruptIE OK!");

            if (set_cpn(channum, "3115552368", (ISDN | NATIONAL), (PRES_ALLOW | NETSCREEN)) != GC_SUCCESS) {
                disp_msg("set_cpn returned an error!");
                return (FALSE);
            }

            return(TRUE);
            //break;

        case 3:
            // Pay no attention to the base-16 11 value; that's irrelevant to the buffer length specifier.
            //memcpy(&payload, "\x70\x05\x80\x31\x33\x33\x38\x36", 8);
            memcpy(&payload, "\x70\x81\x80\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36", 131);
            corruptcpn_ie.length = 131;
            //memcpy(&payload, "\x70\x11\x80\x31\x37\x2A\x32\x32\x31\x31\x33", 11);
            // unsigned char payload[] = { 0x70, 0x0F, 0x80, 0x37, 0x31, 0x31, 0x31, 0x38, 0x39 }; // Called party number IE test
            //corruptcpn_ie.length = 11;

            gc_corruptcpn_ie.gclib = NULL;
            gc_corruptcpn_ie.cclib = &corruptcpn_ie;
            memcpy(corruptcpn_ie.data, payload, corruptcpn_ie.length);
 
            if (gc_SetInfoElem(port[channum].ldev, &gc_corruptcpn_ie) != GC_SUCCESS) {
                gc_errprint("gc_SetInfoElem", channum, -1);
                return (FALSE);
            }
            disp_msg("gc_setcorruptIE OK!");
            return (TRUE);

            if (set_cpn(channum, "3115552369", (ISDN | NATIONAL), (PRES_ALLOW | NETSCREEN)) != GC_SUCCESS) {
                disp_msg("set_cpn returned an error!");
                return (FALSE);
            }

            break;

        case 4:
            //memcpy(&payload, "\x70\x41\x80\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36", 67);
            memcpy(&payload, "\x70\x04\x80\x38\x33\x37\x38\x00\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36\x36", 152);
            // unsigned char payload[] = { 0x70, 0x41, 0x80, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36 }; // Called party number overflow test
            corruptcpn_ie.length = 152;

            gc_corruptcpn_ie.gclib = NULL;
            gc_corruptcpn_ie.cclib = &corruptcpn_ie;
            memcpy(corruptcpn_ie.data, payload, corruptcpn_ie.length);

            if (gc_SetInfoElem(port[channum].ldev, &gc_corruptcpn_ie) != GC_SUCCESS) {
                gc_errprint("gc_SetInfoElem", channum, -1);
                return (FALSE);
            }

            disp_msg("gc_setcorruptIE OK!");


            if (set_cpn(channum, "3115552370", (ISDN | NATIONAL), (PRES_ALLOW | NETSCREEN)) != GC_SUCCESS) {
                disp_msg("set_cpn returned an error!");
                return (FALSE);
            }
            return(TRUE);
            //break;

        case 5:
            //memcpy(&payload, "\x7E\x0C\x42\x4F\x57\x4C\x4F\x46\x48\x41\x54\x53\x53\x53", 14);
            memcpy(&payload, "\x7E\x30\x00", 3);
            //unsigned char payload[] = { 0x7E, 0x80, 0x00 }; // User-to-user IE test
            corruptcpn_ie.length = 3;
            gc_corruptcpn_ie.gclib = NULL;
            gc_corruptcpn_ie.cclib = &corruptcpn_ie;
            memcpy(corruptcpn_ie.data, payload, corruptcpn_ie.length);
            if (gc_SetInfoElem(port[channum].ldev, &gc_corruptcpn_ie) != GC_SUCCESS) {
                gc_errprint("gc_SetInfoElem", channum, -1);
                return (FALSE);
            }

            disp_msg("gc_setcorruptIE OK!");


            if (set_cpn(channum, "3115552371", (ISDN | NATIONAL), (PRES_ALLOW | NETSCREEN)) != GC_SUCCESS) {
                disp_msg("set_cpn returned an error!");
                return (FALSE);
            }
            return(TRUE);
            //break;

        default:
            disp_msg("Incorrect value specified to set_corruptie. Continuing without corrupt element...");

            if (set_cpn(channum, "3115552372", (ISDN | NATIONAL), (PRES_ALLOW | NETSCREEN)) != GC_SUCCESS) {
                disp_msg("set_cpn returned an error!");
                return (FALSE);
            }

            return (FALSE);
    }

    return(FALSE);
}

/***********************************************************
                        Make call
***********************************************************/
char makecall(short channum, char *destination, char *callingnum) {

    // To do: Make calling number passing possible by sending 0 or something
    //GC_MAKECALL_BLK block;
    //block.gclib = &gclib_makecallp;

    int callstate;
    
    dxinfox[ channum ].state = ST_ROUTEDISDN2;    // Mark this call as being used for bridging to an existing channel

    switch (callingnum[0]) {

        case 0x72: // r
            randomcpn(channum);
            break;

        case 0x70: // p
            set_cpn(channum, (callingnum + 1), (ISDN | NATIONAL), (PRES_RESTRICT | NETSCREEN));
            break;

        case 0x21:
            set_corruptie(channum, 1);   // !
            break;

        case 0x40:
            set_corruptie(channum, 2);   // @
            break;

        case 0x23:
            set_corruptie(channum, 3);   // #
            destination[0] = '\0'; // A call to a function was made to write an invalid destination. We shouldn't have two.
            break;

        case 0x24:
            set_corruptie(channum, 4);   // $
            destination[0] = '\0'; // A call to a function was made to write an invalid destination. We shouldn't have two.
            break;

        case 0x25:
            set_corruptie(channum, 5);   // %
            break;

        case 0x26:
            set_corruptie(channum, 6);   // &
            break;

        default:

            // This is a normal calling party number. Let's just set it, well, normally.

            if (set_cpn(channum, callingnum, (ISDN | NATIONAL), (PRES_ALLOW | NETSCREEN)) != GC_SUCCESS) {
                disp_msg("set_cpn returned an error!");
                return (-1);
            }
    }

    sprintf(tmpbuff, "Finished setting CPN, originating %s on channel %d with CPN %s", destination, channum, callingnum);
    disp_msg(tmpbuff);

    // Only DM3 cards allow a call timeout in asynchronous mode, so keep call timeout to 0 for interoperability

    sprintf(tmpbuff, "destination in makecall is %s", destination);
    disp_msg(tmpbuff);
    // set_rdnis( channum, "2127365000", 0x81);
    gc_GetCallState(port[channum].crn, &callstate);
    int res = gc_MakeCall(port[channum].ldev, &port[channum].crn, destination, NULL, 0, EV_ASYNC);
    if (res != GC_SUCCESS) {
        // JCT cards occasionally go bananas and I want to know why
        sprintf(tmpbuff, "gc_MakeCall function returned an error! - %d", res);
        disp_msg(tmpbuff);
        gc_errprint("gc_MakeCall", channum, callstate);
        return (-1);
    }

    isdninfo[channum].status = 0; // Set ISDN status to in-progress/outgoing call if gc_MakeCall succeeds
    disp_status(channum, "Originating ISDN call...");

    return (0);

}

/***********************************************************
                    Trunk hunt operation
***********************************************************/
int isdn_trunkhunt(short channum) {

    int callstate = 1; // GCST_NULL is zero, so let's define this as 1 for the while loop.

    if (channum < 0) {
        return (-1);    // Channum is less than zero? We can't have this.
    }

    while (callstate != GCST_NULL) {

        channum++;

        if (gc_GetCallState(port[channum].crn, &callstate) != SUCCESS) {
            if (port[channum].crn == '\0') {
                return (channum);
            } else {
                sprintf(tmpbuff, "Invalid CRN is %li", port[channum].crn);
                disp_msg(tmpbuff);
                gc_errprint("gc_GetCallState_th", channum, callstate);
                return (-1);
            }
        }

    }

    return (channum);

}

/***********************************************************
       Set RDNIS information for the requested channel
***********************************************************/
char set_rdnis(short channum, char *number, char reason) {
    GC_IE_BLK gc_rdnis_ie;
    IE_BLK rdnis_ie;

    gc_rdnis_ie.gclib = NULL;
    gc_rdnis_ie.cclib = &rdnis_ie;

    rdnis_ie.length = (strlen(number) + 5);   // Three fields, the number, the length indicator, and the IE identifier

    if ((rdnis_ie.length < 1) || (rdnis_ie.length > MAXLEN_IEDATA)) {
        return (-1);    // No overflows plz
    }

    sprintf(rdnis_ie.data, "%c%c%c%c%c%s", 0x74, (rdnis_ie.length - 2), 33, 3, reason, number);
    // 0x74, 0x0D (for 10-digit numbers), 0x21, 0x03, 0x01, number

    if (gc_SetInfoElem(port[channum].ldev, &gc_rdnis_ie) != GC_SUCCESS) {
        gc_errprint("gc_SetInfoElem", channum, -1);
        return (-1);
    }

    sprintf(tmpbuff, "gc_SetInfoElem OK! %s", rdnis_ie.data);
    disp_msg(tmpbuff);
    return (0);
}

char set_bearer (short channum, char bearer) {

GC_IE_BLK gc_bearer_ie;
IE_BLK bearer_ie;

gc_bearer_ie.gclib = NULL;
gc_bearer_ie.cclib = &bearer_ie;

bearer_ie.length = 4;

bearer_ie.data[0] = 0x04;
bearer_ie.data[1] = 2; // This should be something different. Change plz.
bearer_ie.data[2] = bearer;
bearer_ie.data[3] = 0x90; // For the moment, let's keep this hardcoded to 64 kbps/circuit mode.
//bearer_ie.data[4] = 0xA2; // Honestly? Not sure.

    if (gc_SetInfoElem(port[channum].ldev, &gc_bearer_ie) != GC_SUCCESS) {
        gc_errprint("gc_SetInfoElem_set_bearer", channum, -1);
        return (-1);
    }

    sprintf(tmpbuff, "gc_SetInfoElem OK! %s", bearer_ie.data);
    disp_msg(tmpbuff);
    return (0);
}

/***********************************************************
     Set calling party number with non-standard fields
***********************************************************/
char set_cpn(short channum, char *number, char plan_type, char screen_pres) {
    // At this point, we've replaced all the standard Dialogic API calls for CPN with this; they won't inter-operate between DM3/JCT, so fuck those losers.
    // LSH is pre-performed to get plan_type - just OR the two values. For example, plantype = (ISDN | NATIONAL)
    // Same for screen_pres - do, for example, ( PRES_RESTRICT | NETSCREEN )
    GC_IE_BLK gc_cpn_ie;
    IE_BLK cpn_ie;

    gc_cpn_ie.gclib = NULL;
    gc_cpn_ie.cclib = &cpn_ie;

    // First is 0x6C, then length, then plan_type, then presentation/screening string, then CPN string. See spec page 84.
    cpn_ie.length = strlen(number); // Don't compensate for extra fields... yet.

    // cpn_ie.length = ( strlen(number) + 4 ); // Three fields, the number, the length indicator, and the IE identifier
    if ((cpn_ie.length < 1) || (cpn_ie.length > 19)) {
        disp_msg("Malformed CPN inputted: over 23 characters. No CPN sent!");
        return (-1); // No overflows plz. CPN IE max is 23 octets.
    }

    cpn_ie.data[0] = 0x6c;
    cpn_ie.data[1] = (cpn_ie.length + 2);
    cpn_ie.data[2] = plan_type;
    cpn_ie.data[3] = screen_pres;
    memcpy(&cpn_ie.data[4], number, cpn_ie.length);
    cpn_ie.length = (cpn_ie.length + 4); // Set proper length here. This avoids the issue of doing excess arithmetic

    //sprintf( cpn_ie.data, "%c%c%c%c%s", 0x6C, ( cpn_ie.length - 2 ), plan_type, screen_pres, number ); // Can we make this a memcpy() op instead?
    //memcpy( &cpn_ie.data, "\x6C\x0C\x21\xA3\x32\x30\x32\x34\x38\x34\x30\x30\x30\x30", 14); // For testing...
    if (gc_SetInfoElem(port[channum].ldev, &gc_cpn_ie) != GC_SUCCESS) {
        gc_errprint("gc_SetInfoElem_set_cpn", channum, -1);
        return (-1);
    }

    sprintf(tmpbuff, "gc_SetInfoElem OK! %s", cpn_ie.data);
    disp_msg(tmpbuff);
    return (0);
}

/***********************************************************
       Set calling party name for a requested channel
***********************************************************/
char set_cpname(short channum, char *name) {
    GC_IE_BLK gc_cpname_ie;
    IE_BLK cpname_ie;

    gc_cpname_ie.gclib = NULL;
    gc_cpname_ie.cclib = &cpname_ie;
    cpname_ie.length = strlen(name);

    if (cpname_ie.length > 15) {
        // Check length
        name[16] = '\0';
        cpname_ie.length = 17; // Fifteen characters plus two for the IE identifier and length indicator
    }

    else {
        (cpname_ie.length = (cpname_ie.length + 2));
    }

    sprintf(cpname_ie.data, "%c%c%s", 0x28, (cpname_ie.length - 2), name);

    if (gc_SetInfoElem(port[channum].ldev, &gc_cpname_ie) != GC_SUCCESS) {
        gc_errprint("gc_SetInfoElem", channum, -1);
        return (-1);
    }

    sprintf(tmpbuff, "gc_SetInfoElem OK! %s", cpname_ie.data);
    disp_msg(tmpbuff);
    return (0);
}

bool set_corruptprog(short channum, char msgtype) {
    GC_IE_BLK gc_corrupt_ie;
    IE_BLK corrupt_ie;
    GC_PARM_BLKP progressp = NULL;
    GC_PARM_BLKP returnblkp = NULL;
    int progval = IN_BAND_INFO;

    if (msgtype == 1) {
        corrupt_ie.length = 33;
        memcpy(corrupt_ie.data, "\x28\x1F\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21\x21", corrupt_ie.length); // Set corrupt display IE data as the payload
    } else {
        corrupt_ie.length = 3;
        memset(corrupt_ie.data, 0, 3);  // Change 53 to F4
        memcpy(corrupt_ie.data, "\x28\xF1\x21", corrupt_ie.length); // Set corrupt display IE data as the payload
    }

    gc_corrupt_ie.gclib = NULL;
    gc_corrupt_ie.cclib = &corrupt_ie;

    if (gc_SetInfoElem(port[channum].ldev, &gc_corrupt_ie) != GC_SUCCESS) {
        disp_msg("gc_SetInfoElem returned an error! D:");
        gc_errprint("gc_SetInfoElem", channum, -1);
        return (FALSE);
    }

    gc_util_insert_parm_ref(&progressp, GCIS_SET_CALLPROGRESS, GCIS_PARM_CALLPROGRESS_INDICATOR, sizeof(int), &progval);

    if (progressp == NULL) {
        disp_msg("Memory allocation error in set_corruptprog");
        return (FALSE);
    }

    if (gc_Extension(GCTGT_GCLIB_CRN, port[channum].crn, GCIS_EXID_CALLPROGRESS, progressp, &returnblkp, EV_SYNC) != GC_SUCCESS) {
        disp_msg("set_corruptprog operation failed!");
        gc_errprint("gc_Extension_corruptprog", channum, -1);
        return (FALSE);
    }

    disp_msg("Corrupt progress message sent!");
    return (TRUE);
}

/***********************************************************
*   Functionized call cleanup task for ST_ROUTED and ROUTED2
*   ***********************************************************/
bool routed_cleanup( short channum ) {
    dx_stopch(dxinfox[ connchan[channum] ].chdev, EV_ASYNC);
    dx_stopch(dxinfox[ channum ].chdev, EV_ASYNC);
    disp_msg("Unrouting channels from voice devices...3");

    if (nr_scunroute(dxinfox[ channum ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].tsdev, SC_DTI, SC_FULLDUP) == -1) {
        sprintf(tmpbuff, "Uhh, I hate to break it to you man, but SCUnroute1 is shitting itself - state %d", dxinfox[channum].state);
        disp_err(channum, dxinfox[ channum ].chdev, tmpbuff);
        return (-1);
    }

    if (nr_scroute(dxinfox[ connchan[channum] ].tsdev, SC_DTI, dxinfox[ connchan[channum] ].chdev, SC_VOX, SC_FULLDUP) == -1) {
        sprintf(tmpbuff, "Holy shit! SCUnroute2 threw an error! State %d", dxinfox[ connchan [ channum] ].state );
        disp_err(connchan[channum], dxinfox[ connchan[channum] ].chdev, tmpbuff);
        return (-1);
    }

    if (nr_scroute(dxinfox[channum].tsdev, SC_DTI, dxinfox[channum].chdev, SC_VOX, SC_FULLDUP) == -1) {
        sprintf(tmpbuff, "Holy shit! SCroute threw an error! State %d", dxinfox[channum].state);
        disp_err(channum, dxinfox[channum].chdev, tmpbuff);
        return (-1);
    }

    dxinfox[ connchan[channum] ].state = ST_ONHOOK;

    cutthrough[ channum ] = 0;
    dxinfox[ channum ].state = ST_ONHOOK;
    dx_stopch(dxinfox[ channum ].chdev, EV_ASYNC);
    isdn_hkstate(channum, DX_ONHOOK);
    return (0);
}
